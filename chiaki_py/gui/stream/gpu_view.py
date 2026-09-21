"""GPU rendering for StreamDisplay, used when the session has a CUDAFrameHandler.

Every frame is converted to RGB on the GPU into a CUDA buffer, copied (GPU to GPU)
into a pixel buffer object through CUDA-OpenGL interop, and drawn from there by a
QOpenGLWidget, so the pixels never touch the CPU. Requires an NVIDIA GPU that also
renders the window, and the optional dependencies
    pip install cupy-cuda12x cuda-python PyOpenGL
"""

import logging
import time
import warnings

from cuda.bindings import runtime as cudart   # older versions: from cuda import cudart
from OpenGL import GL
from OpenGL.GL.shaders import compileProgram, compileShader
from PyQt6.QtCore import Qt, QTimer, pyqtSignal
from PyQt6.QtGui import QSurfaceFormat
from PyQt6.QtOpenGLWidgets import QOpenGLWidget
from PyQt6.QtWidgets import QLabel, QMainWindow

with warnings.catch_warnings():
    # cupy warns on Windows when there is no system CUDA Toolkit, though the pip wheels bring what it needs.
    warnings.filterwarnings("ignore", message="CUDA path could not be detected")
    import cupy as cp

from chiaki_py import Session
from chiaki_py.gui.stream.aspect_ratio import AspectRatioLock
from chiaki_py.gui.stream.frame_stats import FrameStats
from chiaki_py.lib import StreamSession

_logger = logging.getLogger(__name__)


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


class GpuVideoWidget(QOpenGLWidget):
    """Shows a stream session's frames, decoded, converted and drawn without leaving the GPU.

    Frames are pulled on the GUI thread: the decoder's "frame available" event becomes a Qt
    signal (queued across threads), its slot converts the newest frame into a CUDA buffer
    (a fraction of a millisecond) and paintGL copies it to the texture and draws it. So all
    CUDA and OpenGL work happens on one thread, with this widget's OpenGL context current,
    and there is nothing to lock. The picture is scaled to fit the widget, letterboxed.
    """

    _frame_available = pyqtSignal()   # emitted from the decoder's thread, delivered on the GUI thread
    stream_size_changed = pyqtSignal(int, int)   # the frames turned out to be another size than expected

    def __init__(self, stream_session: StreamSession, handler, width: int, height: int,
                 show_stats: bool = False, parent=None):
        super().__init__(parent)
        fmt = QSurfaceFormat()
        fmt.setVersion(3, 3)
        fmt.setProfile(QSurfaceFormat.OpenGLContextProfile.CoreProfile)
        fmt.setSwapInterval(1)
        self.setFormat(fmt)

        self._handler = handler
        self._size = (width, height)
        self._frame = cp.empty((height, width, 3), dtype=cp.uint8)
        self._texture: CudaGLTexture | None = None
        self._new_frame = False
        self._has_frame = False
        self._warned = False
        self._released = False

        self._stats = FrameStats()
        self._stats_visible = False
        self._pending_get = 0.0            # how long getting the frame that is waiting to be painted took
        self._shown_summary: str | None = None
        self._stats_label = QLabel(self)
        self._stats_label.setStyleSheet("background-color: rgba(0, 0, 0, 150); color: white; "
                                        "font-size: 16px; font-weight: bold; padding: 6px 8px;")
        self._stats_label.setAlignment(Qt.AlignmentFlag.AlignRight)
        self._stats_label.setVisible(False)
        self.set_stats_visible(show_stats)

        self._frame_available.connect(self._pull)
        self._subscription = stream_session.on_frame_available().subscribe(lambda _: self._frame_available.emit())

    # -- the frame rate and time per frame, shown on request -------------------------------------------------

    @property
    def stats_visible(self) -> bool:
        return self._stats_visible

    def set_stats_visible(self, visible: bool) -> None:
        if visible and not self._stats_visible:
            # While they are shown a paint is timed until the GPU has finished it (see paintGL); start
            # over so that the first measurement does not mix in paints that were only timed until submitted.
            self._stats.clear_work()
            self._shown_summary = None
        self._stats_visible = visible
        self._refresh_stats()

    def _refresh_stats(self) -> None:
        text = self._stats.summary()
        self._stats_label.setVisible(self._stats_visible and text is not None)
        if text is not None and text != self._stats_label.text():
            self._stats_label.setText(text)
            self._stats_label.adjustSize()
        self._place_stats()

    def _place_stats(self) -> None:
        self._stats_label.move(self.width() - self._stats_label.width() - 12, 12)

    def resizeEvent(self, event) -> None:
        super().resizeEvent(event)
        self._place_stats()

    # -- frames --------------------------------------------------------------------------------------------

    def _pull(self) -> None:
        if self._released:
            return                     # a signal that was still queued when the widget was released
        try:
            started = time.perf_counter()
            got_frame = self._handler.get_frame(self._frame) is not None
            get_seconds = time.perf_counter() - started
        except ValueError:
            self._adopt_stream_size()
            return
        except RuntimeError:
            # An exception escaping a slot would abort the application, so report it (once) instead.
            self._warn("Dropping unusable frames")
            return
        if got_frame:
            self._pending_get = get_seconds
            self._new_frame = self._has_frame = True
            self.update()

    def _adopt_stream_size(self) -> None:
        """A frame did not fit the buffer: the stream is not the size that was expected (a PS4 asked for
        1080p downgrades to 720p once connected). Without an `out` buffer the handler yields the frame's
        planes, which carry the real size, so learn it from the next one and size the buffer to match."""
        try:
            probe = self._handler.get_frame(None)
        except (RuntimeError, ValueError):
            probe = None
        if probe is None:
            return                     # nothing to learn from yet; the next frame tries again
        size = (probe.width, probe.height)
        del probe                      # gives the decoder's frame back
        if size == self._size:
            self._warn("Dropping unusable frames")
            return
        self._size = size
        self._frame = cp.empty((size[1], size[0], 3), dtype=cp.uint8)
        self._has_frame = False
        self.stream_size_changed.emit(*size)

    def _warn(self, message: str) -> None:
        if not self._warned:
            self._warned = True
            _logger.warning(message, exc_info=True)

    # -- OpenGL --------------------------------------------------------------------------------------------

    def initializeGL(self) -> None:
        self._texture = CudaGLTexture(*self._size)

    def paintGL(self) -> None:
        started = time.perf_counter()
        GL.glClearColor(0.0, 0.0, 0.0, 1.0)
        GL.glClear(GL.GL_COLOR_BUFFER_BIT)
        if self._texture is None or not self._has_frame:
            return
        if (self._texture.width, self._texture.height) != self._size:
            self._texture.close()                       # the stream changed size
            self._texture = CudaGLTexture(*self._size)
        new_frame = self._new_frame
        if new_frame:
            self._texture.upload(self._frame.data.ptr)
            self._new_frame = False
        ratio = self.devicePixelRatioF()
        self._texture.render(round(self.width() * ratio), round(self.height() * ratio))

        if new_frame:                                   # a repaint without a new frame (a resize) is not a frame
            if self._stats_visible:
                GL.glFinish()                           # so the time covers the GPU doing the work, not just being told to
            self._stats.add_get(self._pending_get)
            self._stats.add_paint(time.perf_counter() - started)
            self._stats.tick()
            summary = self._stats.summary()
            if summary != self._shown_summary:
                self._shown_summary = summary
                QTimer.singleShot(0, self._refresh_stats)   # not from inside paintGL: it changes a child widget

    def release(self) -> None:
        """Stop listening for frames and free the GPU resources; the widget must not be used afterwards."""
        if self._released:
            return
        self._released = True
        self._subscription.unsubscribe()
        if self._texture is not None:
            self.makeCurrent()
            self._texture.close()
            self._texture = None
            self.doneCurrent()
        self._frame = None             # free the CUDA buffer now, while CUDA is certainly still there


class GpuStreamWindow(QMainWindow):
    """A window around a GpuVideoWidget. F shows or hides the frame rate and the time needed per frame
    (getting the frame from the decoder and converting it, plus painting it). With `keep_aspect_ratio`
    the window keeps the video's aspect ratio while it is resized, so there are no bars."""

    closeRequested = pyqtSignal()

    def __init__(self, session: Session, show_stats: bool = False, keep_aspect_ratio: bool = False):
        super().__init__()
        self.setWindowTitle("Live Image Stream")
        profile = session.stream_session.get_video_profile()
        self.video = GpuVideoWidget(session.stream_session, session.frame_handler,
                                    profile.width, profile.height, show_stats)
        self.video.stream_size_changed.connect(self._set_video_size)
        self.setCentralWidget(self.video)
        self._aspect = profile.width / profile.height
        self._aspect_lock = AspectRatioLock(lambda: self._aspect) if keep_aspect_ratio else None
        self.resize(profile.width, profile.height)

    def showEvent(self, event) -> None:
        super().showEvent(event)
        if self._aspect_lock is not None:
            self._aspect_lock.attach(int(self.winId()))   # the native window exists once shown

    def _set_video_size(self, width: int, height: int) -> None:
        self._aspect = width / height
        if self._aspect_lock is not None and not (self.isMaximized() or self.isFullScreen()):
            self.resize(self.width(), round(self.width() / self._aspect))

    def keyPressEvent(self, event) -> None:
        if event.key() == Qt.Key.Key_F:
            self.video.set_stats_visible(not self.video.stats_visible)
        elif event.key() == Qt.Key.Key_A:
            if self._aspect_lock is None:
                self._aspect_lock = AspectRatioLock(lambda: self._aspect)
                self._aspect_lock.attach(int(self.winId()))
                self.resize(self.width(), round(self.width() / self._aspect))
            else:
                self._aspect_lock.remove()
                self._aspect_lock = None
        else:
            super().keyPressEvent(event)

    def closeEvent(self, event) -> None:
        if self._aspect_lock is not None:
            self._aspect_lock.remove()
        self.video.release()
        self.closeRequested.emit()
        super().closeEvent(event)
