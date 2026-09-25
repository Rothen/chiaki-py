"""CUDA -> OpenGL interop used by glfw_video.py (and so by 4_stream_cuda_glfw.py and
5_stream_tensor_opengl.py): shows (3, H, W) uint8 RGB frames that live in CUDA device memory
(a torch tensor, a CuPy array, anything with __cuda_array_interface__) as an OpenGL
texture, without them ever touching the CPU, with an optional text overlay (used for the
frame rate) in the top-right corner.

A (3, H, W) frame can be laid out two ways, and both are shown as they are, with no conversion:
planar (C-contiguous, one plane per channel - what a model typically produces), or
interleaved (a transpose/permute view of (H, W, 3) memory - what CudaFrameHandler writes).

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

# The planar layout: one single-channel layer per colour.
PLANAR_FRAGMENT_SHADER = """#version 330 core
uniform sampler2DArray video;
in vec2 uv;
out vec4 color;
void main() {
    color = vec4(texture(video, vec3(uv, 0)).r, texture(video, vec3(uv, 1)).r, texture(video, vec3(uv, 2)).r, 1.0);
}
"""

def frame_layout(frame, width: int, height: int) -> tuple[int, bool]:
    """The device pointer of a (3, height, width) uint8 CUDA frame, and whether it is planar
    (True) or an interleaved view (False). Raises ValueError for any other shape, type or layout."""
    interface = frame.__cuda_array_interface__
    if tuple(interface["shape"]) != (3, height, width) or interface["typestr"] != "|u1":
        raise ValueError(f"need a (3, {height}, {width}) uint8 frame, got {tuple(interface['shape'])} "
                         f"{interface['typestr']}")
    strides = interface.get("strides")
    if strides is None or tuple(strides) == (height * width, width, 1):
        planar = True
    elif tuple(strides) == (1, width * 3, 3):
        planar = False
    else:
        raise ValueError(f"unsupported strides {tuple(strides)}: need a C-contiguous (3, H, W) frame or a "
                         f"(3, H, W) transpose/permute view of a C-contiguous (H, W, 3) one")
    return interface["data"][0], planar


class CudaGLTexture:
    """A width x height RGB picture that is filled from a (3, height, width) CUDA frame and drawn over the viewport.

    Needs an OpenGL 3.3 core context that is current on the calling thread, both
    when it is created and for every later call, and that renders on the same
    NVIDIA GPU as CUDA.
    """

    def __init__(self, width: int, height: int):
        self.width, self.height = width, height
        self.nbytes = width * height * 3

        # One program and texture per layout, picked by the layout of the last frame uploaded:
        # interleaved frames go to an RGB texture, planar ones to a 3-layer single-channel array.
        self.program = compileProgram(compileShader(VERTEX_SHADER, GL.GL_VERTEX_SHADER),
                                      compileShader(FRAGMENT_SHADER, GL.GL_FRAGMENT_SHADER))
        self.planar_program = compileProgram(compileShader(VERTEX_SHADER, GL.GL_VERTEX_SHADER),
                                             compileShader(PLANAR_FRAGMENT_SHADER, GL.GL_FRAGMENT_SHADER))
        self.vao = GL.glGenVertexArrays(1)   # core profile wants one bound even though no vertex data is used

        self.texture = GL.glGenTextures(1)
        self._configure_texture(self.texture, GL.GL_LINEAR)
        GL.glTexImage2D(GL.GL_TEXTURE_2D, 0, GL.GL_RGB8, width, height, 0, GL.GL_RGB, GL.GL_UNSIGNED_BYTE, None)

        self.planar_texture = GL.glGenTextures(1)
        self._configure_texture(self.planar_texture, GL.GL_LINEAR, GL.GL_TEXTURE_2D_ARRAY)
        GL.glTexImage3D(GL.GL_TEXTURE_2D_ARRAY, 0, GL.GL_R8, width, height, 3, 0, GL.GL_RED, GL.GL_UNSIGNED_BYTE, None)
        self.planar = False

        # The pixel buffer object CUDA writes into and OpenGL then reads the texture from. (Not the
        # texture itself: CUDA arrays have no 3-channel format, and interleaved frames are RGB.)
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
    def _configure_texture(texture, filter, target=GL.GL_TEXTURE_2D) -> None:
        GL.glBindTexture(target, texture)
        GL.glTexParameteri(target, GL.GL_TEXTURE_MIN_FILTER, filter)
        GL.glTexParameteri(target, GL.GL_TEXTURE_MAG_FILTER, filter)
        GL.glTexParameteri(target, GL.GL_TEXTURE_WRAP_S, GL.GL_CLAMP_TO_EDGE)
        GL.glTexParameteri(target, GL.GL_TEXTURE_WRAP_T, GL.GL_CLAMP_TO_EDGE)

    def upload(self, frame) -> None:
        """Copy `frame`, a (3, height, width) uint8 CUDA array (planar or an interleaved view), into the texture.

        It is read on CUDA's default stream, so work queued there (torch's and CuPy's default) is finished first.
        """
        device_ptr, self.planar = frame_layout(frame, self.width, self.height)
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
        if self.planar:
            GL.glBindTexture(GL.GL_TEXTURE_2D_ARRAY, self.planar_texture)
            GL.glTexSubImage3D(GL.GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, self.width, self.height, 3,
                               GL.GL_RED, GL.GL_UNSIGNED_BYTE, None)
        else:
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
        GL.glBindVertexArray(self.vao)
        if self.planar:
            GL.glUseProgram(self.planar_program)
            GL.glBindTexture(GL.GL_TEXTURE_2D_ARRAY, self.planar_texture)
        else:
            GL.glUseProgram(self.program)
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
        GL.glDeleteTextures([self.texture, self.planar_texture])
        GL.glDeleteVertexArrays(1, [self.vao])
        GL.glDeleteProgram(self.program)
        GL.glDeleteProgram(self.planar_program)
        if self._overlay_program is not None:
            GL.glDeleteTextures([self._overlay_texture])
            GL.glDeleteProgram(self._overlay_program)
