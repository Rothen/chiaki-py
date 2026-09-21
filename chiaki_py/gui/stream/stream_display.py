from pathlib import Path

import numpy as np
import numpy.typing as npt
from PyQt6.QtCore import QObject, QThread, QUrl, pyqtSignal, pyqtSlot
from PyQt6.QtGui import QGuiApplication, QImage
from PyQt6.QtMultimedia import QVideoFrame, QVideoSink
from PyQt6.QtQml import QQmlApplicationEngine

from chiaki_py import Session
from chiaki_py.controller import attach_controller
from dualsense_py.backends import SDL3Backend
from dualsense_py.utils import get_available_controllers


QML_PATH = Path(__file__).with_name("stream_display.qml")


class FrameProducer(QThread):
    """Bridges Session.frames() (a plain generator) into a Qt signal."""
    frame_ready = pyqtSignal(np.ndarray)

    def __init__(self, session: Session, max_fps: float = 60.0):
        super().__init__()
        self.session = session
        self.max_fps = max_fps
        self._running = True

    def run(self) -> None:
        for frame in self.session.frames(max_fps=self.max_fps):
            if not self._running:
                break
            self.frame_ready.emit(frame)

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
    """Shows the session's frames in the window defined by gui_stream.qml."""

    def __init__(self, session: Session):
        super().__init__()
        self.session = session
        self.frame_size: tuple[int, int] | None = None

        self.engine = QQmlApplicationEngine()
        self.engine.load(QUrl.fromLocalFile(str(QML_PATH)))
        if not self.engine.rootObjects():
            raise RuntimeError(f"Failed to load {QML_PATH}")

        self.window = self.engine.rootObjects()[0]
        self.video_sink: QVideoSink = self.window.property("videoSink")
        self.window.closeRequested.connect(
            self.close)  # type: ignore[attr-defined]

        self.frame_thread = FrameProducer(session)
        self.frame_thread.frame_ready.connect(self.update_frame)

        self.controller_thread = ControllerThread(session)

        self.frame_thread.start()
        self.controller_thread.start()

    @pyqtSlot(np.ndarray)
    def update_frame(self, frame: npt.NDArray[np.uint8]) -> None:
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

    def close(self) -> None:
        self.controller_thread.stop()
        self.frame_thread.stop()
        self.session.stop()
    
    @classmethod
    def start(cls, session: Session, argv: list[str]):
        app = QGuiApplication(argv)
        window = StreamDisplay(session)
        return app.exec()
