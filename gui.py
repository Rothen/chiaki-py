import sys
from typing import Optional
import numpy as np
from PyQt6.QtWidgets import QApplication, QLabel, QMainWindow
from PyQt6.QtGui import QImage, QPixmap, QCloseEvent
from PyQt6.QtCore import QThread, pyqtSignal, pyqtSlot
from PyQt6.QtWidgets import QVBoxLayout, QWidget
from PyQt6.QtCore import Qt
import numpy.typing as npt
import threading
from chiaki_py import Settings, StreamSessionConnectInfo, StreamSession, get_frame
from chiaki_py.core.log import Log, LogLevel
from chiaki_py.core.common import Target
from chiaki_py.core.audio import AudioHeader
import numpy as np

class FrameProducer(QThread):
    """Worker thread that continuously fetches new frames and emits a signal."""
    frame_ready = pyqtSignal(np.ndarray)  # Signal to send new frames

    def __init__(self, stream_session: StreamSession):
        super().__init__()
        self.running = True  # Control flag
        
        self.stream_session = stream_session
        self.stream_session.ffmpeg_frame_available = lambda: self.ffmpeg_frame_available()
        self.frame = np.zeros((1080, 1920, 3), np.uint8)
        self.frame_ready_event = threading.Event()

    def run(self):
        """Continuously grab frames in a separate thread."""
        while self.running:
            if self.frame_ready_event.wait():
                self.frame_ready.emit(self.frame)
                self.frame_ready_event.clear()

    def ffmpeg_frame_available(self):
        get_frame(self.stream_session, False, self.frame)
        self.frame_ready.emit(self.frame)

    def stop(self):
        """Stop the thread safely."""
        self.running = False
        self.frame_ready_event.set()
        self.quit()
        self.wait()

class ImageStream(QMainWindow):
    def __init__(self, stream_session: StreamSession):
        super().__init__()
        self.stream_session = stream_session

        # Setup window
         # Setup window
        self.setWindowTitle("Live Image Stream")
        self.setGeometry(100, 100, 640, 480)

        # Central widget
        central_widget = QWidget()
        self.setCentralWidget(central_widget)

        # Create a layout
        layout = QVBoxLayout()
        central_widget.setLayout(layout)

        # QLabel for displaying images
        self.label = QLabel()
        self.label.setScaledContents(True)  # Allow QLabel to scale images
        layout.addWidget(self.label)

        # Create frame producer thread
        self.frame_thread = FrameProducer(self.stream_session)
        self.frame_thread.frame_ready.connect(self.update_frame)  # Connect signal
        self.frame_thread.start()  # Start fetching frames

    @pyqtSlot(np.ndarray)
    def update_frame(self, frame: npt.NDArray[np.uint8]):
        """Receive a new frame and update the QLabel."""
        height, width, channels = frame.shape
        bytes_per_line = channels * width
        q_image = QImage(frame.data, width, height, bytes_per_line, QImage.Format.Format_RGB888)
        pixmap = QPixmap.fromImage(q_image)
        scaled_pixmap = pixmap.scaled(self.label.size(), aspectRatioMode=Qt.AspectRatioMode.KeepAspectRatioByExpanding)  # 1 = Qt.KeepAspectRatio
        # Convert QImage to QPixmap and update QLabel
        self.label.setPixmap(scaled_pixmap)
    
    def closeEvent(self, a0: Optional[QCloseEvent]):
        # Stop the frame thread
        self.frame_thread.frame_ready.disconnect(self.update_frame)
        self.frame_thread.stop()
        self.frame_thread.wait()

        # Stop the stream session if connected
        if self.stream_session.is_connected():
            self.stream_session.stop()

        # Accept the close event
        if a0 is not None:
            a0.accept()


log = Log(level=LogLevel.INFO)
audio_header: AudioHeader = AudioHeader(2, 16, 480 * 100, 480)

host = "192.168.42.43"
regist_key = "b02d1ceb"
nickname = "PS5-083"
ps5Id = "78c881a8214a"
morning = 'aa3f52ff47431d2f2cf0f14110f679b3'
initial_login_pin = ""  # None
initial_login_pin = ""  # None
duid = ""  # None
auto_regist = False
fullscreen = False
zoom = False
stretch = False
ps5 = True
discover_timout = 2000

settings: Settings = Settings()
settings.set_log_verbose(False)

connect_info: StreamSessionConnectInfo = StreamSessionConnectInfo(
    settings=settings,
    target=Target.PS5_1,
    host=host,
    nickname=nickname,
    regist_key=regist_key,
    morning=bytes.fromhex(morning),
    initial_login_pin=initial_login_pin,
    duid=duid,
    auto_regist=auto_regist,
    fullscreen=fullscreen,
    zoom=zoom,
    stretch=stretch
)
stream_session: StreamSession = StreamSession(connect_info)
app = QApplication(sys.argv)
window = ImageStream(stream_session)

img = np.zeros((1080, 1920, 3), np.uint8)

# stream_session.ffmpeg_frame_available = ffmpeg_frame_available
stream_session.session_quit = lambda a, b: print('session_quit')
stream_session.login_pin_requested = lambda a: print('login_pin_requested')
stream_session.data_holepunch_progress = lambda a: print('data_holepunch_progress')
stream_session.auto_regist_succeeded = lambda a: print('auto_regist_succeeded')
stream_session.nickname_received = lambda a: print('nickname_received')
stream_session.connected_changed = lambda : print('connected_changed')
stream_session.measured_bitrate_changed = lambda : print('measured_bitrate_changed')
stream_session.average_packet_loss_changed = lambda : print('average_packet_loss_changed')
stream_session.cant_display_changed = lambda a: print('cant_display_changed')

if __name__ == "__main__":
    stream_session.start()
    window.show()
    sys.exit(app.exec())