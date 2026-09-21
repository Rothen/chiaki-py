"""PyQt6 remote-play viewer built on chiaki_py's high-level API.

Usage:
    python examples/gui_stream.py
"""

import sys
from pathlib import Path

from chiaki_py import Session, Serializer
from chiaki_py.gui import StreamDisplay
from chiaki_py.registration import Registration
from chiaki_py.lib import Settings


def main() -> None:
    cache_dir = Path("./cache")
    registration_path = Path(cache_dir, "registration.json")
    registration = Serializer.load(Registration, registration_path)

    settings = Settings()
    settings.set_log_verbose(False)

    session = Session.connect(
        settings,
        registration
    )

    session.stream_session.on_session_quit().subscribe(lambda reason: print("Session Quit:", reason))
    session.stream_session.on_login_pin_requested().subscribe(lambda incorrect: print("Login Pin Requested:", incorrect))
    session.stream_session.on_connected_changed().subscribe(lambda connected: print("Connected Changed:", connected))
    
    with session:
        sys.exit(StreamDisplay.start(session, sys.argv))


if __name__ == "__main__":
    main()
