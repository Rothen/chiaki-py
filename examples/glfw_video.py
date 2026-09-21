"""A GLFW window that shows RGB frames straight from CUDA device memory, shared by
1.4.2_stream_cuda_gl.py (frames in a CuPy array) and 1.4.4_stream_tensor.py (frames in a
PyTorch tensor). The interop itself is in cuda_gl.py.

Needs: pip install glfw cuda-python PyOpenGL opencv-python
"""

import glfw

from cuda_gl import CudaGLTexture
from fps_overlay import FpsCounter


class GLVideoSurface:
    """A GLFW window showing width x height RGB frames that arrive as CUDA device memory.

    With `show_fps` the frame rate (frames shown per second) is drawn in the top-right corner.
    """

    def __init__(self, width: int, height: int, title: str, show_fps: bool = True):
        if not glfw.init():
            raise RuntimeError("Failed to initialise GLFW")
        glfw.window_hint(glfw.CONTEXT_VERSION_MAJOR, 3)
        glfw.window_hint(glfw.CONTEXT_VERSION_MINOR, 3)
        glfw.window_hint(glfw.OPENGL_PROFILE, glfw.OPENGL_CORE_PROFILE)
        self.window = glfw.create_window(width, height, title, None, None)
        if not self.window:
            glfw.terminate()
            raise RuntimeError("Failed to create the OpenGL window")
        glfw.make_context_current(self.window)
        glfw.swap_interval(1)
        self.texture = CudaGLTexture(width, height)
        self.fps = FpsCounter() if show_fps else None

    def __enter__(self) -> "GLVideoSurface":
        return self

    def __exit__(self, exc_type, exc_value, traceback) -> None:
        self.close()

    @property
    def should_close(self) -> bool:
        return (glfw.window_should_close(self.window)
                or glfw.get_key(self.window, glfw.KEY_Q) == glfw.PRESS
                or glfw.get_key(self.window, glfw.KEY_ESCAPE) == glfw.PRESS)

    def set_title(self, title: str) -> None:
        glfw.set_window_title(self.window, title)

    def upload(self, device_ptr: int) -> None:
        self.texture.upload(device_ptr)

    def render(self) -> None:
        self.texture.render(*glfw.get_framebuffer_size(self.window))

    def show(self, device_ptr: int) -> None:
        """Show the RGB frame at `device_ptr` (width * height * 3 bytes of device memory)."""
        self.upload(device_ptr)
        if self.fps is not None:
            fps = self.fps.tick()
            if fps is not None:
                self.texture.set_overlay(f"{fps:.1f} FPS")
        self.render()
        glfw.swap_buffers(self.window)
        glfw.poll_events()

    def close(self) -> None:
        self.texture.close()
        glfw.destroy_window(self.window)
        glfw.terminate()
