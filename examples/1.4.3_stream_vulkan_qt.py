"""PyQt6 remote-play viewer that renders with Vulkan - the pixels are never copied.

The Vulkan hardware decoder decodes on the GPU and, with the VulkanFrameHandler, the
frames stay right where it put them: the window is drawn on that same Vulkan device
by libplacebo, which converts the NV12 frames to RGB (see chiaki_py/gui/stream/vulkan_view.py).
Unlike 1.4.2_stream_gpu_qt.py there is no CUDA or OpenGL involved, so it is not tied to
NVIDIA and needs nothing beyond chiaki-py itself. Compare 1.4.1_stream_qt.py, which
decodes to system memory.

Usage:
    python examples/1.4.5_stream_vulkan_qt.py

The registration is the one written by 1.3_register_console.py. Press F in the
window to show the frame rate (drawn over the video by libplacebo), A to lock its aspect ratio.

Needs: a GPU and driver with Vulkan video decoding (H.264 for a PS4, H.264 or HEVC
for a PS5). Windows only so far.
"""

import sys
from pathlib import Path

from chiaki_py import Session, Serializer
from chiaki_py.gui import StreamDisplay
from chiaki_py.registration import Registration
from chiaki_py.lib import Settings, VulkanFrameHandler
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
    settings.set_log_level(LogLevel.ERROR)
    settings.set_hardware_decoder("vulkan")

    session = Session.connect(
        settings,
        registration,
        VulkanFrameHandler
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
