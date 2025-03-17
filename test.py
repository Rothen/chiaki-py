from typing import Any
import threading
import signal
import sys
from chiaki_py import Settings, StreamSessionConnectInfo, StreamSession, get_frame
from chiaki_py.core.log import LogLevel, Log, CHIAKI_LOG_ALL
from chiaki_py.core.common import Target
# from chiaki_py.core.session import chiaki_rp_application_reason_string, chiaki_rp_version_string
import numpy as np
import cv2

exit_event = threading.Event()

# level_mask=CHIAKI_LOG_ALL & ~LogLevel.INFO.value
log = Log()
host = "192.168.42.43"
regist_key = "b02d1ceb"
nickname = "PS5-083"
ps5Id = "78c881a8214a"
morning = "ª?RÿGC\\x1d/,ðñA\\x10öy³"
morning_ints: list[int] = [170, 63, 82, 255, 71, 67, 29, 47, 44, 240, 241, 65, 16, 246, 121, 179]
initial_login_pin = ""  # None
duid = ""  # None
auto_regist = False
fullscreen = False
zoom = False
stretch = False
ps5 = True
discover_timout = 2000

# regist_key_list: list[int] = [a for a in map(ord, regist_key)]
# morning_list: list[int] = [a for a in map(ord, morning)]

settings: Settings = Settings()
settings.set_log_verbose(False)

connect_info: StreamSessionConnectInfo = StreamSessionConnectInfo(
    settings=settings,
    target=Target.PS5_1,
    host=host,
    nickname=nickname,
    regist_key=regist_key,
    morning=morning_ints,
    initial_login_pin=initial_login_pin,
    duid=duid,
    auto_regist=auto_regist,
    fullscreen=fullscreen,
    zoom=zoom,
    stretch=stretch
)

img = np.zeros((1080, 1920, 3), np.uint8)

stream_session: StreamSession = StreamSession(connect_info)
stream_session.ffmpeg_frame_available = lambda: get_frame(stream_session, False, img)
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
        cv2.imshow('frame', img)
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break
    
    stream_session.stop()
    cv2.destroyAllWindows()

    print("Session closed. Exiting...")

    # log.set_callback(lambda level, message: print(f"[{level}] {message}"))
    # print(wakeup(log, host, registKey, ps5))
    # print(discover(log, host, discover_timout))
