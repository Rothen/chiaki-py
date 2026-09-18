"""PyQt6 remote-play viewer built on chiaki_py's high-level API.

Usage:
    python examples/gui_stream.py path/to/config.json

The config file is a `chiaki_py.config.ChiakiPySettings` JSON document, e.g.:
    {
        "host": "192.168.1.50",
        "nickname": "My PS5",
        "regist_key": "...",
        "morning": "...",
        "duid": ""
    }
`regist_key`/`morning` normally come from `chiaki_py.registration.register()`
(see examples/register_console.py) rather than being typed in by hand.
"""

import sys

import numpy as np
import numpy.typing as npt
from PyQt6.QtCore import Qt, pyqtSignal, pyqtSlot
from PyQt6.QtGui import QCloseEvent, QImage, QPixmap
from PyQt6.QtWidgets import QApplication, QLabel, QMainWindow, QVBoxLayout, QWidget
from PyQt6.QtCore import QThread

from chiaki_py import Session
from chiaki_py.config import ChiakiPySettings
from chiaki_py.controller import attach_controller
from chiaki_py.lib import Settings
from chiaki_py.lib.core.common import Target
from dualsense_py.backends import SDL3Backend
from dualsense_py.utils import get_available_controllers


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
        attach_controller(available_controllers[0], self.session.stream_session)

    def stop(self) -> None:
        self.quit()
        self.wait()
        self.session.stream_session.release_right()
        self.session.stream_session.release_left()
        self.session.stream_session.send_feedback_state()


class ImageStream(QMainWindow):
    def __init__(self, session: Session):
        super().__init__()
        self.session = session

        self.setWindowTitle("Live Image Stream")
        self.setGeometry(100, 100, 640, 480)

        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        layout = QVBoxLayout()
        central_widget.setLayout(layout)

        self.label = QLabel()
        self.label.setScaledContents(True)
        layout.addWidget(self.label)

        self.frame_thread = FrameProducer(session)
        self.frame_thread.frame_ready.connect(self.update_frame)

        self.controller_thread = ControllerThread(session)

        self.frame_thread.start()
        self.controller_thread.start()

    @pyqtSlot(np.ndarray)
    def update_frame(self, frame: npt.NDArray[np.uint8]) -> None:
        height, width, channels = frame.shape
        bytes_per_line = channels * width
        q_image = QImage(frame.data, width, height, bytes_per_line, QImage.Format.Format_RGB888)
        pixmap = QPixmap.fromImage(q_image)
        scaled_pixmap = pixmap.scaled(self.label.size(), aspectRatioMode=Qt.AspectRatioMode.KeepAspectRatioByExpanding)
        self.label.setPixmap(scaled_pixmap)

    def closeEvent(self, event: QCloseEvent | None) -> None:
        self.controller_thread.stop()
        self.frame_thread.stop()
        self.session.stop()
        if event is not None:
            event.accept()


def main() -> None:
    config_path = sys.argv[1] if len(sys.argv) > 1 else "chiaki_py_config.json"
    config = ChiakiPySettings.from_file(config_path)

    settings = Settings()
    settings.set_log_verbose(False)

    target = Target.PS5_1 if config.ps5 else Target.PS4_1
    session = Session.connect(
        settings,
        host=config.host,
        nickname=config.nickname,
        regist_key=config.regist_key,
        morning=bytes.fromhex(config.morning),
        target=target,
        initial_login_pin=config.initial_login_pin,
        duid=config.duid,
        auto_regist=config.auto_regist,
        fullscreen=config.fullscreen,
        zoom=config.zoom,
        stretch=config.stretch,
    )

    session.stream_session.on_session_quit().subscribe(lambda reason: print("Session Quit:", reason))
    session.stream_session.on_login_pin_requested().subscribe(lambda incorrect: print("Login Pin Requested:", incorrect))
    session.stream_session.on_connected_changed().subscribe(lambda connected: print("Connected Changed:", connected))

    app = QApplication(sys.argv)
    window = ImageStream(session)

    session.stream_session.start()
    window.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
