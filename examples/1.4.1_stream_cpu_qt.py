"""PyQt6 remote-play viewer built on chiaki_py's high-level API. Frames are
decoded to system memory; see 1.4.2_stream_gpu_qt.py for the GPU version.

Usage:
    python examples/1.4.1_stream_qt.py

The registration is the one written by 1.3_register_console.py. Press F in the
window to show the frame rate.
"""

import sys
from pathlib import Path

from chiaki_py import Session, Serializer
from chiaki_py.gui import StreamDisplay
from chiaki_py.registration import Registration
from chiaki_py.lib import Settings
from chiaki_py.lib.core.log import LogLevel

from helpers import setup_controller


def main() -> None:
    cache_dir = Path("./cache")
    registration_file = Path(cache_dir, "registration.json")
    
    if not cache_dir.exists() or not registration_file.exists():
        print(f"Registration not found under {registration_file}. Run examples/1.3_register_console.py first")
        sys.exit(1)

    registration = Serializer.load(Registration, Path("./cache", "registration.json"))

    settings = Settings()
    settings.set_log_verbose(False)
    settings.set_log_level(LogLevel.ERROR)

    session = Session.connect(
        settings,
        registration
    )

    session.stream_session.on_session_quit().subscribe(lambda reason: print("Session Quit:", reason))
    session.stream_session.on_login_pin_requested().subscribe(lambda incorrect: print("Login Pin Requested:", incorrect))
    session.stream_session.on_connected_changed().subscribe(lambda connected: print("Connected Changed:", connected))
    
    with session:
        controller_attached = setup_controller(session.stream_session)

        res = StreamDisplay.start(session, sys.argv)

        if controller_attached:
            session.stream_session.release_right()
            session.stream_session.release_left()
            session.stream_session.send_feedback_state()
            
        sys.exit(res)
        


if __name__ == "__main__":
    main()
