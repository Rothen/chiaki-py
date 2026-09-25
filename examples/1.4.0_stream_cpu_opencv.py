"""PyQt6 remote-play viewer built on chiaki_py's high-level API. Frames are
decoded to system memory; see 1.4.2_stream_gpu_qt.py for the GPU version.

Usage:
    python examples/1.4.1_stream_qt.py

The registration is the one written by 1.3_register_console.py. Press F in the
window to show the frame rate.
"""

import sys
from pathlib import Path

import cv2
from chiaki_py import Session, Serializer, HostRegistration
from chiaki_py.session import CpuFrameHandler
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

        try:
            profile = session.stream_session.get_video_profile()
            frame = CpuFrameHandler.empty_frame(profile.width, profile.height)

            print("Streaming - press 'q' in the window, or Ctrl+C in the terminal, to quit.")

            for _ in session.frames(max_fps=0, out=frame):
                cv2.imshow("chiaki-py", cv2.cvtColor(frame, cv2.COLOR_RGB2BGR))
                if cv2.waitKey(1) & 0xFF == ord("q"):
                    break
        finally:
            if controller is not None and subscriptions is not None:
                detach_controller(controller, session.stream_session, subscriptions)

    cv2.destroyAllWindows()
    sys.exit()


if __name__ == "__main__":
    main()
