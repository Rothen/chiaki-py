"""PyQt6 remote-play viewer that renders with Vulkan - the pixels are never copied.

The Vulkan hardware decoder decodes on the GPU and, with the VulkanFrameHandler, the
frames stay right where it put them: the window is drawn on that same Vulkan device
by libplacebo, which converts the NV12 frames to RGB (see chiaki_py/gui/stream/views/vulkan_view.py).
Unlike 1.4.2_stream_cuda_qt.py there is no CUDA or OpenGL involved, so it is not tied to
NVIDIA and needs nothing beyond chiaki-py itself. Compare 1.4.1_stream_cpu_qt.py, which
decodes to system memory.

Usage:
    python examples/1.4.3_stream_vulkan_qt.py

The registration is the one written by 1.3_register_console.py. Press F in the
window to show the frame rate (drawn over the video by libplacebo), A to lock its aspect ratio.

Needs: a GPU and driver with Vulkan video decoding (H.264 for a PS4, H.264 or HEVC
for a PS5). Windows only so far.
"""

import sys
from pathlib import Path

from chiaki_py import Session, Serializer, HostRegistration
from chiaki_py.controller import detach_controller
from chiaki_py.gui import StreamDisplay
from chiaki_py.lib import Settings, VulkanFrameHandler, LogLevel

from helpers import setup_controller


def main() -> None:
    cache_dir = Path("./cache")
    registration_file = Path(cache_dir, "host_registration.json")

    if not cache_dir.exists() or not registration_file.exists():
        print(f"Registration not found under {registration_file}. Run examples/1.3_register_console.py first")
        sys.exit(1)

    registration = Serializer.load(HostRegistration, Path("./cache", "host_registration.json"))

    session = Session(
        registration,
        VulkanFrameHandler
    )

    session.cp_session.on_session_quit().subscribe(lambda reason: print(f"session quit ({reason})"))
    session.cp_session.on_connected_changed().subscribe(lambda connected: print(f"connected to {registration.nickname}" if connected else "connection closed"))

    with session:
        controller, subscriptions = setup_controller(session.cp_session)

        res = StreamDisplay.start(session, sys.argv)

        if controller is not None and subscriptions is not None:
            detach_controller(controller, session.cp_session, subscriptions)

        sys.exit(res)


if __name__ == "__main__":
    main()
