"""PyQt6 remote-play viewer built on chiaki_py's high-level API. Frames are
decoded to system memory; see 1.4.2_stream_cuda_qt.py for the GPU version.

Usage:
    python examples/1.4.1_stream_cpu_qt.py

The registration is the one written by 1.3_register_console.py. Press F in the
window to show the frame rate.
"""

import sys
from pathlib import Path

from chiaki_py import Session, Serializer, HostRegistration
from chiaki_py.gui import StreamDisplay
from chiaki_py.lib import Settings, LogLevel
from chiaki_py.controller import detach_controller

from helpers import setup_controller


def main() -> None:
    cache_dir = Path("./cache")
    registration_file = Path(cache_dir, "host_registration.json")
    
    if not cache_dir.exists() or not registration_file.exists():
        print(f"Registration not found under {registration_file}. Run examples/1.3_register_console.py first")
        sys.exit(1)

    registration = Serializer.load(HostRegistration, Path("./cache", "host_registration.json"))

    settings = Settings()
    settings.set_log_verbose(False)
    settings.set_log_level(LogLevel.ERROR)

    session = Session.connect(
        settings,
        registration
    )

    session.stream_session.on_session_quit().subscribe(lambda reason: print(f"session quit ({reason})"))
    session.stream_session.on_connected_changed().subscribe(lambda connected: print(f"connected to {registration.nickname}" if connected else "connection closed"))
    
    with session:
        controller, subscriptions = setup_controller(session.stream_session)

        res = StreamDisplay.start(session, sys.argv)

        if controller is not None and subscriptions is not None:
            detach_controller(controller, session.stream_session, subscriptions)
            
        sys.exit(res)
        


if __name__ == "__main__":
    main()
