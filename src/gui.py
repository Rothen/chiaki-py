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
import time
import numpy as np
from pydualsense import pydualsense
from dualsense_controller import DualSenseController

class FrameProducer(QThread):
    """Worker thread that continuously fetches new frames and emits a signal."""
    frame_ready = pyqtSignal(np.ndarray)  # Signal to send new frames

    def __init__(self, stream_session: StreamSession):
        super().__init__()
        self.running = True  # Control flag
        
        self.stream_session = stream_session
        self.stream_session.on_frame_available().subscribe(lambda a: self.ffmpeg_frame_available())
        self.frame = np.zeros((1080, 1920, 3), np.uint8)
        self.frame_ready_event = threading.Event()

    def run(self):
        """Continuously grab frames in a separate thread."""
        while self.running:
            if self.frame_ready_event.wait():
                start = time.perf_counter()
                self.frame_ready.emit(self.frame)
                self.frame_ready_event.clear()
                end = time.perf_counter()
                time.sleep(max(0, 1.0 / 60 - (end - start)))  # Limit to 60 FPS

    def ffmpeg_frame_available(self):
        get_frame(self.stream_session, False, self.frame)
        self.frame_ready.emit(self.frame)

    def stop(self):
        """Stop the thread safely."""
        self.running = False
        self.frame_ready_event.set()
        self.quit()
        self.wait()

class LeftRightToggler(QThread):
    def __init__(self, stream_session: StreamSession):
        super().__init__()
        self.running = True  # Control flag
        self.stream_session = stream_session

    def run(self):
        """Continuously grab frames in a separate thread."""
        self.controller = DualSenseController()

        self.controller.btn_cross.on_down(self.stream_session.press_cross)
        self.controller.btn_cross.on_up(self.stream_session.release_cross)

        self.controller.btn_circle.on_down(self.stream_session.press_circle)
        self.controller.btn_circle.on_up(self.stream_session.release_circle)

        self.controller.btn_square.on_down(self.stream_session.press_square)
        self.controller.btn_square.on_up(self.stream_session.release_square)
        
        self.controller.btn_triangle.on_down(self.stream_session.press_triangle)
        self.controller.btn_triangle.on_up(self.stream_session.release_triangle)
        
        self.controller.btn_left.on_down(self.stream_session.press_left)
        self.controller.btn_left.on_up(self.stream_session.release_left)

        self.controller.btn_right.on_down(self.stream_session.press_right)
        self.controller.btn_right.on_up(self.stream_session.release_right)

        self.controller.btn_up.on_down(self.stream_session.press_up)
        self.controller.btn_up.on_up(self.stream_session.release_up)

        self.controller.btn_down.on_down(self.stream_session.press_down)
        self.controller.btn_down.on_up(self.stream_session.release_down)

        self.controller.btn_l1.on_down(self.stream_session.press_l1)
        self.controller.btn_l1.on_up(self.stream_session.release_l1)

        self.controller.btn_r1.on_down(self.stream_session.press_r1)
        self.controller.btn_r1.on_up(self.stream_session.release_r1)

        self.controller.btn_l3.on_down(self.stream_session.press_l3)
        self.controller.btn_l3.on_up(self.stream_session.release_l3)

        self.controller.btn_r3.on_down(self.stream_session.press_r3)
        self.controller.btn_r3.on_up(self.stream_session.release_r3)

        self.controller.btn_options.on_down(self.stream_session.press_options)
        self.controller.btn_options.on_up(self.stream_session.release_touchpad)

        self.controller.btn_create.on_down(self.stream_session.press_create)
        self.controller.btn_create.on_up(self.stream_session.release_touchpad)

        self.controller.btn_touchpad.on_down(self.stream_session.press_touchpad)
        self.controller.btn_touchpad.on_up(self.stream_session.release_touchpad)

        self.controller.btn_ps.on_down(self.stream_session.press_ps)
        self.controller.btn_ps.on_up(self.stream_session.release_ps)
        
        '''self.controller.btn_l2.on_change(lambda state: print(state))
        
        self.controller.left_stick.on_change(lambda x, y: self.stream_session.move_left_stick(x, y))'''
        
        self.controller.activate()
        while self.running:            
            time.sleep(1.0)

    def stop(self):
        """Stop the thread safely."""
        self.running = False
        self.quit()
        self.wait()
        self.stream_session.release_right()
        self.stream_session.release_left()
        self.stream_session.send_feedback_state()

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
        
        self.left_right_toggler = LeftRightToggler(self.stream_session)
        
        self.frame_thread.start()  # Start fetching frames
        self.left_right_toggler.start()  # Start toggling left/right

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
        self.left_right_toggler.stop()
        self.left_right_toggler.wait()

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

host = "192.168.42.32"
regist_key = "b02d1ceb"
nickname = "PS5-083"
morning = 'aa3f52ff47431d2f2cf0f14110f679b3'
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
stream_session.on_session_quit().subscribe(on_next=lambda reason: print('Session Quit:', reason))
stream_session.on_login_pin_requested().subscribe(lambda pin_incorrect: print('Login Pin Requested:', pin_incorrect))
stream_session.on_data_holepunch_progress().subscribe(lambda finished: print('Data Holepunch Progress:', finished))
stream_session.on_nickname_received().subscribe(lambda nickname: print('Nickname Received:', nickname))
stream_session.on_connected_changed().subscribe(lambda connected: print('Connected Changed:', connected))
stream_session.on_measured_bitrate_changed().subscribe(lambda bitrate: print('Measured Bitrate Changed:', bitrate))
stream_session.on_average_packet_loss_changed().subscribe(lambda packet_loss: print('Average Packet Loss Changed:', packet_loss))
stream_session.on_cant_display_changed().subscribe(lambda cant_display: print('Cant Display Changed:', cant_display))

if __name__ == "__main__":
    stream_session.start()
    window.show()
    sys.exit(app.exec())