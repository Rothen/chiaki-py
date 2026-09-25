"""The smallest remote-play viewer: frames in a numpy array, shown with OpenCV.

Frames are decoded to system memory into one (H, W, 3) uint8 RGB numpy array
that every frame overwrites, and shown with cv2.imshow - the easiest starting
point for processing frames with OpenCV or NumPy. See 1.4.1_stream_cpu_qt.py
for the Qt viewer, and 1.4.2_stream_cuda_qt.py or 5_stream_tensor_opengl.py to
keep the frames on the GPU.

Usage:
    python examples/3_stream_cpu_opencv.py

The registration is the one written by 1.3_register_console.py. Press 'q' in
the window, or Ctrl+C in the terminal, to quit.

Needs: pip install opencv-python
"""

import sys
from pathlib import Path

import cv2
from chiaki_py import Session, Serializer, HostRegistration, AudioSink
from chiaki_py.session import CpuFrameHandler
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

    session = Session(
        settings,
        registration
    )
    _ = AudioSink(session)

    session.cp_session.on_session_quit().subscribe(lambda reason: print(f"session quit ({reason})"))
    session.cp_session.on_connected_changed().subscribe(lambda connected: print(f"connected to {registration.nickname}" if connected else "connection closed"))
    
    with session:
        controller, subscriptions = setup_controller(session.cp_session)

        try:
            profile = session.cp_session.get_video_profile()
            frame = CpuFrameHandler.empty_frame(profile.width, profile.height)

            print("Streaming - press 'q' in the window, or Ctrl+C in the terminal, to quit.")

            for _ in session.frames(max_fps=0, out=frame):
                cv2.imshow("chiaki-py", cv2.cvtColor(frame, cv2.COLOR_RGB2BGR))
                if cv2.waitKey(1) & 0xFF == ord("q"):
                    break
        finally:
            if controller is not None and subscriptions is not None:
                detach_controller(controller, session.cp_session, subscriptions)

    cv2.destroyAllWindows()
    sys.exit()


if __name__ == "__main__":
    main()
