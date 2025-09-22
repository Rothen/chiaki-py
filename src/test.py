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
from pydualsense import pydualsense

exit_event = threading.Event()

log = Log(level=LogLevel.INFO)
audio_header: AudioHeader = AudioHeader(2, 16, 480 * 100, 480)

host = "..."
regist_key = "..."
nickname = "..."
morning = '...'
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

img = np.zeros((1080, 1920, 3), np.uint8)
img_to_show = np.zeros((1080, 1920, 3), np.uint8)

def get_ffmpeg_frame() -> None:
    """Get the frame from the stream session."""
    get_frame(stream_session, False, img)
    cv2.cvtColor(img, cv2.COLOR_RGB2BGR, img_to_show)

stream_session: StreamSession = StreamSession(connect_info)
stream_session.on_frame_available().subscribe(lambda x: get_ffmpeg_frame())

def dpad_up(state: bool):
    if state:
        stream_session.press_up()
    else:
        stream_session.release_up()

def dpad_right(state: bool):
    if state:
        stream_session.press_right()
    else:
        stream_session.release_right()

def dpad_down(state: bool):
    if state:
        stream_session.press_down()
    else:
        stream_session.release_down()

def dpad_left(state: bool):
    if state:
        stream_session.press_left()
    else:
        stream_session.release_left()


def signal_handler(sig: int, frame: Any) -> None:
    """Handles Ctrl+C to stop the session gracefully."""
    print("\nCtrl+C detected! Stopping stream session...")
    stream_session.stop()
    exit_event.set()


if __name__ == "__main__":
    signal.signal(signal.SIGINT, signal_handler)

    print("Starting stream session...")
    stream_session.start()

    ds = pydualsense()  # open controller
    ds.init()  # initialize controller
    ds.dpad_up += dpad_up
    ds.dpad_right += dpad_right
    ds.dpad_down += dpad_down
    ds.dpad_left += dpad_left

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
