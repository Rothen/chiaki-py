"""GPU rendering for StreamDisplay, used when the session has a CudaFrameHandler.

Every frame is converted to RGB on the GPU into a CUDA buffer, copied (GPU to GPU)
into a pixel buffer object through CUDA-OpenGL interop, and drawn from there by a
QOpenGLWidget, so the pixels never touch the CPU. Requires an NVIDIA GPU that also
renders the window, and the optional dependencies
    pip install cupy-cuda12x cuda-python PyOpenGL
"""

import warnings
from typing import TypeVar, Any

from cuda.bindings import runtime as cudart   # older versions: from cuda import cudart
from OpenGL import GL
from OpenGL.GL.shaders import compileProgram, compileShader
from PyQt6.QtGui import QSurfaceFormat
from PyQt6.QtOpenGLWidgets import QOpenGLWidget

import numpy as np
import numpy.typing as npt
from .base_view import VideoMixin
from ..threads.frame_thread import FrameThread
from ..threads.fps_thread import FpsThread


def check(result):
    err, *rest = result
    if err != cudart.cudaError_t.cudaSuccess:
        raise RuntimeError(cudart.cudaGetErrorString(err)[1].decode())
    return rest[0] if len(rest) == 1 else rest


# One triangle covering the whole viewport; the texture is flipped because row 0 of a video frame is its top.
VERTEX_SHADER = """#version 330 core
out vec2 uv;
void main() {
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    uv = vec2(p.x, 1.0 - p.y);
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
"""

FRAGMENT_SHADER = """#version 330 core
uniform sampler2D video;
in vec2 uv;
out vec4 color;
void main() { color = texture(video, uv); }
"""


class CudaGLTexture:
    """A width x height RGB texture that is filled from CUDA device memory and drawn into a viewport.

    Needs an OpenGL 3.3 core context that is current on the calling thread, both
    when it is created and for every later call, and that renders on the same
    NVIDIA GPU as CUDA.
    """

    def __init__(self, width: int, height: int):
        self.width, self.height = width, height
        self.nbytes = width * height * 3

        self.program = compileProgram(compileShader(VERTEX_SHADER, GL.GL_VERTEX_SHADER),
                                      compileShader(FRAGMENT_SHADER, GL.GL_FRAGMENT_SHADER))
        self.vao = GL.glGenVertexArrays(1)   # core profile wants one bound even though no vertex data is used

        self.texture = GL.glGenTextures(1)
        GL.glBindTexture(GL.GL_TEXTURE_2D, self.texture)
        GL.glTexParameteri(GL.GL_TEXTURE_2D, GL.GL_TEXTURE_MIN_FILTER, GL.GL_LINEAR)
        GL.glTexParameteri(GL.GL_TEXTURE_2D, GL.GL_TEXTURE_MAG_FILTER, GL.GL_LINEAR)
        GL.glTexParameteri(GL.GL_TEXTURE_2D, GL.GL_TEXTURE_WRAP_S, GL.GL_CLAMP_TO_EDGE)
        GL.glTexParameteri(GL.GL_TEXTURE_2D, GL.GL_TEXTURE_WRAP_T, GL.GL_CLAMP_TO_EDGE)
        GL.glTexImage2D(GL.GL_TEXTURE_2D, 0, GL.GL_RGB8, width, height, 0, GL.GL_RGB, GL.GL_UNSIGNED_BYTE, None)

        # The pixel buffer object CUDA writes into and OpenGL then reads the texture from. (Not the
        # texture itself: CUDA arrays have no 3-channel format, and the frames are RGB.)
        self.pbo = GL.glGenBuffers(1)
        GL.glBindBuffer(GL.GL_PIXEL_UNPACK_BUFFER, self.pbo)
        GL.glBufferData(GL.GL_PIXEL_UNPACK_BUFFER, self.nbytes, None, GL.GL_STREAM_DRAW)
        GL.glBindBuffer(GL.GL_PIXEL_UNPACK_BUFFER, 0)
        self.resource = check(cudart.cudaGraphicsGLRegisterBuffer(
            int(self.pbo), cudart.cudaGraphicsRegisterFlags.cudaGraphicsRegisterFlagsWriteDiscard))

    def upload(self, device_ptr: int) -> None:
        """Copy the RGB frame at `device_ptr` (width * height * 3 bytes of device memory) into the texture."""
        check(cudart.cudaGraphicsMapResources(1, self.resource, 0))
        try:
            pbo_ptr, _ = check(cudart.cudaGraphicsResourceGetMappedPointer(self.resource))
            check(cudart.cudaMemcpy(pbo_ptr, device_ptr, self.nbytes,
                                    cudart.cudaMemcpyKind.cudaMemcpyDeviceToDevice))
        finally:
            check(cudart.cudaGraphicsUnmapResources(1, self.resource, 0))

        # With a pixel unpack buffer bound, the "pointer" is an offset into it: OpenGL copies GPU to GPU.
        GL.glPixelStorei(GL.GL_UNPACK_ALIGNMENT, 1)
        GL.glBindBuffer(GL.GL_PIXEL_UNPACK_BUFFER, self.pbo)
        GL.glBindTexture(GL.GL_TEXTURE_2D, self.texture)
        GL.glTexSubImage2D(GL.GL_TEXTURE_2D, 0, 0, 0, self.width, self.height, GL.GL_RGB, GL.GL_UNSIGNED_BYTE, None)
        GL.glBindBuffer(GL.GL_PIXEL_UNPACK_BUFFER, 0)

    def render(self, viewport_width: int, viewport_height: int) -> None:
        """Draw the texture as large as fits the viewport (in pixels of the bound framebuffer) with its
        aspect ratio kept, centred. What is left over is not touched: clear the framebuffer first."""
        scale = min(viewport_width / self.width, viewport_height / self.height)
        width, height = max(1, round(self.width * scale)), max(1, round(self.height * scale))
        GL.glViewport((viewport_width - width) // 2, (viewport_height - height) // 2, width, height)
        GL.glUseProgram(self.program)
        GL.glBindVertexArray(self.vao)
        GL.glBindTexture(GL.GL_TEXTURE_2D, self.texture)
        GL.glDrawArrays(GL.GL_TRIANGLES, 0, 3)
        GL.glBindVertexArray(0)
        GL.glUseProgram(0)

    def close(self) -> None:
        if self.resource is None:
            return
        check(cudart.cudaGraphicsUnregisterResource(self.resource))   # before the buffer goes away
        self.resource = None
        GL.glDeleteBuffers(1, [self.pbo])
        GL.glDeleteTextures([self.texture])
        GL.glDeleteVertexArrays(1, [self.vao])
        GL.glDeleteProgram(self.program)


_ScalarT = TypeVar("_ScalarT", bound=np.generic)


class _DataWithPointer(Any):
    ptr: int

class _CupyArray(npt.NDArray[_ScalarT]):
    @property
    def data(self) -> _DataWithPointer: ...


class CudaVideoWidget(VideoMixin[_CupyArray], QOpenGLWidget): # pyright: ignore[reportIncompatibleMethodOverride]
    """Shows a stream session's frames, decoded, converted and drawn without leaving the GPU.

    Frames are pulled by a FrameProducer on its own thread, which converts each one into a fresh CUDA
    buffer (a fraction of a millisecond) and hands it over as a Qt signal (queued across threads); the
    slot here just marks it ready and paintGL copies it to the texture and draws it, with this widget's
    OpenGL context current. The picture is scaled to fit the widget, letterboxed.
    """

    def __init__(self, frame_thread: FrameThread, fps_thread: FpsThread, parent=None):
        super().__init__(frame_thread, fps_thread, parent)
        fmt = QSurfaceFormat()
        fmt.setVersion(3, 3)
        fmt.setProfile(QSurfaceFormat.OpenGLContextProfile.CoreProfile)
        fmt.setSwapInterval(1)
        self.setFormat(fmt)

        self._texture: CudaGLTexture | None = None
        self._new_frame = False
        self._has_frame = False

    def initializeGL(self) -> None:
        self._texture = CudaGLTexture(*self._size)

    def _render(self) -> None:
        """Called off of paintGL, with no current GL context: just mark the frame ready and ask Qt
        to repaint, so the actual GL work in paintGL() below runs with a valid context."""
        self._new_frame = self._has_frame = True
        self.update()

    def paintGL(self) -> None:
        GL.glClearColor(0.0, 0.0, 0.0, 1.0)
        GL.glClear(GL.GL_COLOR_BUFFER_BIT)
        if self._texture is None or not self._has_frame:
            return
        if (self._texture.width, self._texture.height) != self._size:
            self._texture.close()                       # the stream changed size
            self._texture = CudaGLTexture(*self._size)
        new_frame, self._new_frame = self._new_frame, False

        # Timed only for an actual new frame, not an incidental repaint (e.g. from a resize), and includes
        # glFinish() so the reported time is the real GPU-side cost, not just how long submitting it took.
        if new_frame:
            self._fps_thread.tick_render()
            self._texture.upload(self._frame.data.ptr)
        ratio = self.devicePixelRatioF()
        self._texture.render(round(self.width() * ratio),
                             round(self.height() * ratio))
        if new_frame:
            GL.glFinish()
            self._fps_thread.tock_render()

    def _get_frame_size(self) -> tuple[int, int]:
        return (self._frame.shape[1], self._frame.shape[0])

    def release(self) -> None:
        """Stop listening for frames and free the GPU resources; the widget must not be used afterwards."""
        if self._texture is not None:
            self.makeCurrent()
            self._texture.close()
            self._texture = None
            self.doneCurrent()
        super().release()
