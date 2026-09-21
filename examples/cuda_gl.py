"""CUDA -> OpenGL interop shared by stream_from_gpu.py (GLFW window) and
stream_from_gpu_qt.py (Qt window): shows RGB frames that live in CUDA device
memory as an OpenGL texture, without them ever touching the CPU, with an
optional text overlay (used for the frame rate) in the top-right corner.

Needs: pip install cuda-python PyOpenGL opencv-python
"""

import numpy as np
from cuda.bindings import runtime as cudart   # older versions: from cuda import cudart
from OpenGL import GL
from OpenGL.GL.shaders import compileProgram, compileShader

from fps_overlay import OVERLAY_MARGIN, rasterize_text


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

# A rectangle of `size` pixels at `origin` (its lower-left corner, in pixels from the viewport's), drawn as a
# triangle strip through the corners (0,0) (1,0) (0,1) (1,1).
OVERLAY_VERTEX_SHADER = """#version 330 core
uniform vec2 origin;
uniform vec2 size;
uniform vec2 viewport;
out vec2 uv;
void main() {
    vec2 c = vec2(gl_VertexID & 1, (gl_VertexID >> 1) & 1);
    uv = vec2(c.x, 1.0 - c.y);
    gl_Position = vec4((origin + c * size) / viewport * 2.0 - 1.0, 0.0, 1.0);
}
"""

FRAGMENT_SHADER = """#version 330 core
uniform sampler2D video;
in vec2 uv;
out vec4 color;
void main() { color = texture(video, uv); }
"""

class CudaGLTexture:
    """A width x height RGB texture that is filled from CUDA device memory and drawn over the viewport.

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
        self._configure_texture(self.texture, GL.GL_LINEAR)
        GL.glTexImage2D(GL.GL_TEXTURE_2D, 0, GL.GL_RGB8, width, height, 0, GL.GL_RGB, GL.GL_UNSIGNED_BYTE, None)

        # The pixel buffer object CUDA writes into and OpenGL then reads the texture from. (Not the
        # texture itself: CUDA arrays have no 3-channel format, and the frames are RGB.)
        self.pbo = GL.glGenBuffers(1)
        GL.glBindBuffer(GL.GL_PIXEL_UNPACK_BUFFER, self.pbo)
        GL.glBufferData(GL.GL_PIXEL_UNPACK_BUFFER, self.nbytes, None, GL.GL_STREAM_DRAW)
        GL.glBindBuffer(GL.GL_PIXEL_UNPACK_BUFFER, 0)
        self.resource = check(cudart.cudaGraphicsGLRegisterBuffer(
            int(self.pbo), cudart.cudaGraphicsRegisterFlags.cudaGraphicsRegisterFlagsWriteDiscard))

        # The overlay (created on first use): its own program, texture, and the state that decides
        # when the text has to be rasterized again.
        self._overlay_text: str | None = None
        self._overlay_program = None
        self._overlay_texture = None
        self._overlay_size = (0, 0)
        self._overlay_scale: float | None = None
        self._overlay_stale = False

    @staticmethod
    def _configure_texture(texture, filter) -> None:
        GL.glBindTexture(GL.GL_TEXTURE_2D, texture)
        GL.glTexParameteri(GL.GL_TEXTURE_2D, GL.GL_TEXTURE_MIN_FILTER, filter)
        GL.glTexParameteri(GL.GL_TEXTURE_2D, GL.GL_TEXTURE_MAG_FILTER, filter)
        GL.glTexParameteri(GL.GL_TEXTURE_2D, GL.GL_TEXTURE_WRAP_S, GL.GL_CLAMP_TO_EDGE)
        GL.glTexParameteri(GL.GL_TEXTURE_2D, GL.GL_TEXTURE_WRAP_T, GL.GL_CLAMP_TO_EDGE)

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

    def set_overlay(self, text: str | None) -> None:
        """The text drawn in the top-right corner of the picture (None: nothing). Cheap if it is unchanged."""
        if text != self._overlay_text:
            self._overlay_text = text
            self._overlay_stale = True

    def render(self, viewport_width: int, viewport_height: int, ui_scale: float = 1.0) -> None:
        """Draw the texture over the viewport (in pixels) of whatever framebuffer is bound.

        `ui_scale` sizes the overlay for high-DPI screens (the devicePixelRatio).
        """
        GL.glViewport(0, 0, viewport_width, viewport_height)
        GL.glUseProgram(self.program)
        GL.glBindVertexArray(self.vao)
        GL.glBindTexture(GL.GL_TEXTURE_2D, self.texture)
        GL.glDrawArrays(GL.GL_TRIANGLES, 0, 3)
        if self._overlay_text:
            self._draw_overlay(viewport_width, viewport_height, ui_scale)
        GL.glBindVertexArray(0)
        GL.glUseProgram(0)

    def _draw_overlay(self, viewport_width: int, viewport_height: int, ui_scale: float) -> None:
        if self._overlay_program is None:
            self._overlay_program = compileProgram(compileShader(OVERLAY_VERTEX_SHADER, GL.GL_VERTEX_SHADER),
                                                   compileShader(FRAGMENT_SHADER, GL.GL_FRAGMENT_SHADER))
            self._overlay_texture = GL.glGenTextures(1)
            self._configure_texture(self._overlay_texture, GL.GL_NEAREST)   # drawn 1:1, keep the text crisp
        if self._overlay_stale or ui_scale != self._overlay_scale:
            image = np.ascontiguousarray(rasterize_text(self._overlay_text, ui_scale))
            self._overlay_size = (image.shape[1], image.shape[0])
            GL.glBindBuffer(GL.GL_PIXEL_UNPACK_BUFFER, 0)
            GL.glPixelStorei(GL.GL_UNPACK_ALIGNMENT, 1)
            GL.glBindTexture(GL.GL_TEXTURE_2D, self._overlay_texture)
            GL.glTexImage2D(GL.GL_TEXTURE_2D, 0, GL.GL_RGBA8, image.shape[1], image.shape[0], 0,
                            GL.GL_RGBA, GL.GL_UNSIGNED_BYTE, image)
            self._overlay_stale, self._overlay_scale = False, ui_scale

        width, height = self._overlay_size
        margin = round(OVERLAY_MARGIN * ui_scale)
        GL.glEnable(GL.GL_BLEND)
        GL.glBlendFunc(GL.GL_SRC_ALPHA, GL.GL_ONE_MINUS_SRC_ALPHA)
        GL.glUseProgram(self._overlay_program)
        GL.glUniform2f(GL.glGetUniformLocation(self._overlay_program, "origin"),
                       viewport_width - margin - width, viewport_height - margin - height)
        GL.glUniform2f(GL.glGetUniformLocation(self._overlay_program, "size"), width, height)
        GL.glUniform2f(GL.glGetUniformLocation(self._overlay_program, "viewport"), viewport_width, viewport_height)
        GL.glBindTexture(GL.GL_TEXTURE_2D, self._overlay_texture)
        GL.glDrawArrays(GL.GL_TRIANGLE_STRIP, 0, 4)
        GL.glDisable(GL.GL_BLEND)

    def close(self) -> None:
        if self.resource is None:
            return
        check(cudart.cudaGraphicsUnregisterResource(self.resource))   # before the buffer goes away
        self.resource = None
        GL.glDeleteBuffers(1, [self.pbo])
        GL.glDeleteTextures([self.texture])
        GL.glDeleteVertexArrays(1, [self.vao])
        GL.glDeleteProgram(self.program)
        if self._overlay_program is not None:
            GL.glDeleteTextures([self._overlay_texture])
            GL.glDeleteProgram(self._overlay_program)
