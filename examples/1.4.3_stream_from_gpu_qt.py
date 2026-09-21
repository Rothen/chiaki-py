"""Stream from the GPU into a Qt window - the pixels never touch the CPU.

The same path as stream_from_gpu.py (NVDEC decode, RGB conversion into a CUDA
buffer, CUDA-OpenGL interop, see cuda_gl.py), but drawn by a QOpenGLWidget, so
the video can sit in any Qt layout next to other widgets. CudaVideoWidget is
the reusable part; the rest is a small window around it.

Frames are pulled on the GUI thread: the decoder's "frame available" event is
turned into a Qt signal (queued across threads), the slot converts the newest
frame into a CUDA buffer (a fraction of a millisecond), and paintGL copies it to
the texture and draws it. So all CUDA and OpenGL work happens on one thread,
with the widget's own OpenGL context current, and there is nothing to lock.

Usage:
    python examples/stream_from_gpu_qt.py [--force-pair] [--headless] [--dir ./cache]

Discovery, pairing and the pairing cache are exactly those of
discover_and_stream.py (which see). Press 'q' or Esc in the window, or Ctrl+C
in the terminal, to quit.

Needs: an NVIDIA GPU that also renders the window, and
    pip install cupy-cuda12x cuda-python PyOpenGL
    pip install -e .[psn,cv,controller]
"""

import logging
import signal
import sys
import warnings
from pathlib import Path

import typer
from OpenGL import GL
from PyQt6.QtCore import Qt, QTimer, pyqtSignal
from PyQt6.QtGui import QSurfaceFormat
from PyQt6.QtOpenGLWidgets import QOpenGLWidget
from PyQt6.QtWidgets import QApplication, QMainWindow

with warnings.catch_warnings():
    # cupy warns on Windows when there is no system CUDA Toolkit, though the pip wheels bring what it needs.
    warnings.filterwarnings("ignore", message="CUDA path could not be detected")
    import cupy as cp

from chiaki_py import Serializer, Session
from chiaki_py.registration import Registration
from chiaki_py.lib import Settings, CUDAFrameHandler, StreamSession
from cuda_gl import CudaGLTexture
from fps_overlay import FpsCounter
from helpers import setup_controller

_logger = logging.getLogger(__name__)


class CudaVideoWidget(QOpenGLWidget):
    """Shows a stream_session's frames, decoded, converted and drawn without leaving the GPU.

    With `show_fps` the frame rate (frames shown per second) is drawn in the top-right corner.
    """

    _frame_available = pyqtSignal()   # emitted from the decoder's thread, delivered on the GUI thread

    def __init__(self, stream_session: StreamSession, width: int, height: int, handler=None, show_fps: bool = True,
                 parent=None):
        super().__init__(parent)
        fmt = QSurfaceFormat()
        fmt.setVersion(3, 3)
        fmt.setProfile(QSurfaceFormat.OpenGLContextProfile.CoreProfile)
        fmt.setSwapInterval(1)
        self.setFormat(fmt)

        # Every frame is converted to RGB into this device buffer, which must match the stream's
        # size exactly (a PS5 streams what the profile says; a PS4 asked for 1080p downgrades to
        # 720p once connected, which would not fit).
        self._size = (width, height)
        self._frame = cp.empty((height, width, 3), dtype=cp.uint8)
        self._handler = handler or CUDAFrameHandler(stream_session)
        self._texture: CudaGLTexture | None = None
        self._fps = FpsCounter() if show_fps else None
        self._new_frame = False
        self._has_frame = False
        self._warned = False
        self._released = False

        self._frame_available.connect(self._pull)
        self._subscription = stream_session.on_frame_available().subscribe(lambda _: self._frame_available.emit())

    def _pull(self) -> None:
        try:
            got_frame = self._handler.get_frame(self._frame) is not None
        except (RuntimeError, ValueError):
            # An exception escaping a slot would abort the application, so report it (once) instead.
            if not self._warned:
                self._warned = True
                _logger.warning("Dropping unusable frames", exc_info=True)
            return
        if got_frame:
            self._new_frame = self._has_frame = True
            self.update()

    def initializeGL(self) -> None:
        self._texture = CudaGLTexture(*self._size)

    def paintGL(self) -> None:
        if self._texture is None or not self._has_frame:
            GL.glClearColor(0.0, 0.0, 0.0, 1.0)
            GL.glClear(GL.GL_COLOR_BUFFER_BIT)
            return
        if self._new_frame:
            self._texture.upload(self._frame.data.ptr)
            self._new_frame = False
            if self._fps is not None:
                fps = self._fps.tick()      # once per frame shown, not per repaint (a resize repaints too)
                if fps is not None:
                    self._texture.set_overlay(f"{fps:.1f} FPS")
        ratio = self.devicePixelRatioF()
        self._texture.render(round(self.width() * ratio), round(self.height() * ratio), ratio)

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


class StreamWindow(QMainWindow):
    def __init__(self, stream_session: StreamSession, width: int, height: int):
        super().__init__()
        self.setWindowTitle("chiaki-py")
        self.video = CudaVideoWidget(stream_session, width, height)
        self.setCentralWidget(self.video)
        self.resize(width, height)

    def keyPressEvent(self, event) -> None:
        if event.key() in (Qt.Key.Key_Q, Qt.Key.Key_Escape):
            self.close()
        else:
            super().keyPressEvent(event)

    def closeEvent(self, event) -> None:
        self.video.release()
        super().closeEvent(event)


def main() -> None:
    cache_dir = Path("./cache")
    registration_file = Path(cache_dir, "registration.json")
    
    if not cache_dir.exists() or not registration_file.exists():
        print(f"Registration not found under {registration_file}. Run examples/1.3_login.py first")
        sys.exit(1)

    registration = Serializer.load(Registration, Path("./cache", "registration.json"))

    # The widgets below need a QApplication (a QGuiApplication is not enough) before any of them exists.
    app = QApplication(sys.argv)
    signal.signal(signal.SIGINT, lambda *_: app.quit())
    keep_python_running = QTimer()   # lets the interpreter handle Ctrl+C while Qt's event loop runs
    keep_python_running.timeout.connect(lambda: None)
    keep_python_running.start(200)

    settings = Settings()
    settings.set_log_verbose(False)
    settings.set_hardware_decoder("cuda")

    session = Session.connect(
        settings,
        registration,
        CUDAFrameHandler
    )

    session.stream_session.on_session_quit().subscribe(lambda reason: print("Session Quit:", reason))
    session.stream_session.on_login_pin_requested().subscribe(lambda incorrect: print("Login Pin Requested:", incorrect))
    session.stream_session.on_connected_changed().subscribe(lambda connected: print("Connected Changed:", connected))

    with session:
        controller_attached = setup_controller(session.stream_session)

        try:
            profile = session.stream_session.get_video_profile()
            window = StreamWindow(session.stream_session, profile.width, profile.height)
            window.show()

            print("Streaming - press 'q' or Esc in the window, or Ctrl+C in the terminal, to quit.")
            app.exec()
            window.close()   # frees the GPU resources if the loop ended by Ctrl+C rather than the window
        finally:
            if controller_attached:
                session.stream_session.release_right()
                session.stream_session.release_left()
                session.stream_session.send_feedback_state()


if __name__ == "__main__":
    typer.run(main)
