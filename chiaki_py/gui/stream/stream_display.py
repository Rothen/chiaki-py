import time
from pathlib import Path

import numpy as np
import numpy.typing as npt
from PyQt6.QtCore import QCoreApplication, QObject, QThread, QUrl, pyqtSignal, pyqtSlot
from PyQt6.QtGui import QImage
from PyQt6.QtMultimedia import QVideoFrame, QVideoSink
from PyQt6.QtQml import QQmlApplicationEngine
from PyQt6.QtWidgets import QApplication

from chiaki_py import Session
from chiaki_py.controller import attach_controller
from chiaki_py.gui.stream.aspect_ratio import AspectRatioLock
from chiaki_py.gui.stream.frame_stats import FrameStats
from chiaki_py.lib import CPUFrameHandler, CUDAFrameHandler
from dualsense_py.backends import SDL3Backend
from dualsense_py.utils import get_available_controllers


QML_PATH = Path(__file__).with_name("stream_display.qml")


class FrameProducer(QThread):
    """Bridges Session.frames() (a plain generator) into a Qt signal, along with how long getting the frame took."""
    frame_ready = pyqtSignal(np.ndarray, float)

    def __init__(self, session: Session, max_fps: float = 60.0):
        super().__init__()
        self.session = session
        self.max_fps = max_fps
        self._running = True

    def run(self) -> None:
        for frame in self.session.frames(max_fps=self.max_fps):
            if not self._running:
                break
            self.frame_ready.emit(frame, self.session.last_get_time)

    def stop(self) -> None:
        self._running = False
        self.quit()
        self.wait()


class ControllerThread(QThread):
    def __init__(self, session: Session):
        super().__init__()
        self.session = session

    def run(self) -> None:
        SDL3Backend.init()
        available_controllers = get_available_controllers()
        if not available_controllers:
            print("No DualSense controllers found.")
            return
        attach_controller(
            available_controllers[0], self.session.stream_session)

    def stop(self) -> None:
        self.quit()
        self.wait()
        self.session.stream_session.release_right()
        self.session.stream_session.release_left()
        self.session.stream_session.send_feedback_state()


class StreamDisplay(QObject):
    """Shows the session's frames in a window, scaled to fit it as it is resized.

    How the frames are rendered follows the session's frame handler:
      - CPUFrameHandler: frames are decoded to system memory and shown by a QML window
        (stream_display.qml).
      - CUDAFrameHandler: frames are converted on the GPU and drawn straight from GPU
        memory by an OpenGL widget (gpu_view.py), never touching the CPU. This needs an
        NVIDIA GPU, a QApplication, and pip install cupy-cuda12x cuda-python PyOpenGL.
    Other handlers cannot be shown.

    Press F to show or hide the frame rate and the time needed per frame in the top-right
    corner; `show_stats` says whether they start out shown. The time per frame is how long it
    takes to get a frame plus how long it takes to paint it, averaged over the last half
    second, e.g. "2.0 ms/frame (get 1.5 + paint 0.5)":
      - get: the frame handler fetching the frame, i.e. decoding it to system memory
        (CPU) or converting it to RGB on the GPU (GPU).
      - paint: on the CPU path, preparing the frame and handing it to Qt (the drawing
        itself is done by Qt's render thread and not included); on the GPU path, copying
        it into the texture and drawing it, up to the GPU having finished.

    With `keep_aspect_ratio` (the default) the window keeps the video's aspect ratio while it is
    resized by dragging, so there are no bars; without it the window can take any shape and the
    picture is letterboxed. Only on Windows so far; elsewhere the window is never constrained.
    """

    def __init__(self, session: Session, show_stats: bool = False, keep_aspect_ratio: bool = False):
        super().__init__()
        self.session = session
        self.frame_thread: FrameProducer | None = None
        self.aspect_lock: AspectRatioLock | None = None

        handler = session.frame_handler
        if isinstance(handler, CUDAFrameHandler):
            self._init_gpu(show_stats, keep_aspect_ratio)
        elif isinstance(handler, CPUFrameHandler):
            self._init_cpu(show_stats, keep_aspect_ratio)
        else:
            raise TypeError(f"StreamDisplay can't show frames from a {type(handler).__name__}: "
                            "use a CPUFrameHandler (rendered on the CPU) or a CUDAFrameHandler (rendered on the GPU)")

        self.controller_thread = ControllerThread(session)
        self.controller_thread.start()

    def _init_cpu(self, show_stats: bool, keep_aspect_ratio: bool) -> None:
        self.frame_size: tuple[int, int] | None = None
        self.stats = FrameStats()

        self.engine = QQmlApplicationEngine()
        self.engine.load(QUrl.fromLocalFile(str(QML_PATH)))
        if not self.engine.rootObjects():
            raise RuntimeError(f"Failed to load {QML_PATH}")

        self.window = self.engine.rootObjects()[0]
        self.window.setProperty("showStats", show_stats)
        self.video_sink: QVideoSink = self.window.property("videoSink")
        self.window.closeRequested.connect(self.close)  # type: ignore[attr-defined]
        if keep_aspect_ratio:
            # Once the first frame gives the ratio, the window keeps it while it is resized
            self.aspect_lock = AspectRatioLock(
                lambda: self.frame_size[0] / self.frame_size[1] if self.frame_size else 0.0)
            self.aspect_lock.attach(int(self.window.winId()))  # type: ignore[attr-defined]

        self.frame_thread = FrameProducer(self.session)
        self.frame_thread.frame_ready.connect(self.update_frame)  # type: ignore[attr-defined]
        self.frame_thread.start()

    def _init_gpu(self, show_stats: bool, keep_aspect_ratio: bool) -> None:
        if not isinstance(QCoreApplication.instance(), QApplication):
            raise RuntimeError("Rendering on the GPU needs a QApplication (a QGuiApplication is not enough) "
                               "to exist first; StreamDisplay.start() creates one")
        try:
            from chiaki_py.gui.stream.gpu_view import GpuStreamWindow
        except ImportError as e:
            raise ImportError("Rendering on the GPU needs: pip install cupy-cuda12x cuda-python PyOpenGL") from e

        self.window = GpuStreamWindow(self.session, show_stats, keep_aspect_ratio)
        self.window.closeRequested.connect(self.close)  # type: ignore[attr-defined]
        self.window.show()

    @pyqtSlot(np.ndarray, float)  # type: ignore[attr-defined]
    def update_frame(self, frame: npt.NDArray[np.uint8], get_seconds: float) -> None:
        started = time.perf_counter()
        height, width, channels = frame.shape
        if self.frame_size != (width, height):
            # The size is only known once a frame arrives; resize on the first one and whenever it changes
            self.frame_size = (width, height)
            self.window.setProperty("width", width)
            self.window.setProperty("height", height)
        # QImage does not copy, keep the buffer alive until the frame is built
        data = frame.tobytes()
        q_image = QImage(data, width, height, channels *
                         width, QImage.Format.Format_RGB888)
        self.video_sink.setVideoFrame(QVideoFrame(q_image))
        self.stats.add_get(get_seconds)
        self.stats.add_paint(time.perf_counter() - started)
        self.stats.tick()
        summary = self.stats.summary()
        if summary is not None:
            self.window.setProperty("statsText", summary)

    def close(self) -> None:
        if self.aspect_lock is not None:
            self.aspect_lock.remove()
        self.controller_thread.stop()
        if self.frame_thread is not None:
            self.frame_thread.stop()
        self.session.stop()

    @classmethod
    def start(cls, session: Session, argv: list[str], show_stats: bool = False, keep_aspect_ratio: bool = False):
        app = QCoreApplication.instance() or QApplication(argv)
        _ = StreamDisplay(session, show_stats, keep_aspect_ratio)
        return app.exec()
