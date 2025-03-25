from typing import Any
import threading
import signal
import sys
from chiaki_py import Settings, StreamSessionConnectInfo, StreamSession, get_frame
from chiaki_py.core.log import Log, LogLevel
from chiaki_py.core.common import Target
from chiaki_py.core.audio import AudioHeader
# from chiaki_py.core.session import chiaki_rp_application_reason_string, chiaki_rp_version_string
import numpy as np
import cv2

from .chiaki_py_settings import ChiakiPySettings

chiaki_py_settings: ChiakiPySettings = ChiakiPySettings.from_file("settings.json")

exit_event = threading.Event()

settings: Settings = Settings()
settings.set_log_verbose(True)

connect_info: StreamSessionConnectInfo = StreamSessionConnectInfo(
    settings=settings,
    target=Target.PS5_1,
    host=chiaki_py_settings.host,
    nickname=chiaki_py_settings.nickname,
    regist_key=chiaki_py_settings.regist_key,
    morning=bytes.fromhex(chiaki_py_settings.morning),
    initial_login_pin=chiaki_py_settings.initial_login_pin,
    duid=chiaki_py_settings.duid,
    auto_regist=chiaki_py_settings.auto_regist,
    fullscreen=chiaki_py_settings.fullscreen,
    zoom=chiaki_py_settings.zoom,
    stretch=chiaki_py_settings.stretch
)

img = np.zeros((1080, 1920, 3), np.uint8)
img_to_show = np.zeros((1080, 1920, 3), np.uint8)

def get_ffmpeg_frame() -> None:
    """Get the frame from the stream session."""
    get_frame(stream_session, False, img)
    cv2.cvtColor(img, cv2.COLOR_RGB2BGR, img_to_show)

stream_session: StreamSession = StreamSession(connect_info)
stream_session.ffmpeg_frame_available = lambda: get_ffmpeg_frame()
stream_session.session_quit = lambda a, b: print('session_quit')
stream_session.login_pin_requested = lambda a: print('login_pin_requested')
stream_session.data_holepunch_progress = lambda a: print('data_holepunch_progress')
stream_session.auto_regist_succeeded = lambda a: print('auto_regist_succeeded')
stream_session.nickname_received = lambda a: print('nickname_received')
stream_session.connected_changed = lambda : print('connected_changed')
stream_session.measured_bitrate_changed = lambda : print('measured_bitrate_changed')
stream_session.average_packet_loss_changed = lambda : print('average_packet_loss_changed')
stream_session.cant_display_changed = lambda a: print('cant_display_changed')


def signal_handler(sig: int, frame: Any) -> None:
    """Handles Ctrl+C to stop the session gracefully."""
    print("\nCtrl+C detected! Stopping stream session...")
    stream_session.stop()
    exit_event.set()


if __name__ == "__main__":
    signal.signal(signal.SIGINT, signal_handler)

    if "-w" in sys.argv:
        input("Press Enter to continue...")
    print("Starting stream session...")
    stream_session.start()
    print("Started")

    while not exit_event.is_set():
        cv2.imshow('frame', img_to_show)
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break
    
    stream_session.stop()
    cv2.destroyAllWindows()

    print("Session closed. Exiting...")

    # log.set_callback(lambda level, message: print(f"[{level}] {message}"))
    # print(wakeup(log, host, registKey, ps5))
    # print(discover(log, host, discover_timout))
