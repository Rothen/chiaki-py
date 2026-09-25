"""Stream from the GPU into an OpenGL window - the pixels never touch the CPU.

NVDEC decodes the console's video on the GPU, chiaki-py converts each frame to
RGB on the GPU into a CUDA buffer, and CUDA-OpenGL interop (cuda_gl.py) then
hands that buffer to OpenGL: it is copied (GPU to GPU) into a pixel buffer
object, from which OpenGL fills a texture that is drawn on the screen. Compare
2_discover_and_stream_opencv.py, which downloads every frame to the CPU for
OpenCV, and 1.4.2_stream_cuda_qt.py, which does this inside a Qt window.

Usage:
    python examples/1.4.4_stream_cuda_glfw.py

The registration is the one written by 1.3_register_console.py. Press 'q' or
Esc in the window, or Ctrl+C in the terminal, to quit.

Needs: an NVIDIA GPU that also renders the window, and
    pip install cupy-cuda12x cuda-python glfw PyOpenGL
"""

import sys
from pathlib import Path
import typer

import cupy as cp
from chiaki_py import Session, Serializer, HostRegistration
from chiaki_py.lib import Settings, CudaFrameHandler
from chiaki_py.controller import detach_controller

from glfw_video import GLVideoSurface
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
    settings.set_hardware_decoder("cuda")

    session = Session(
        settings,
        registration,
        CudaFrameHandler
    )

    session.cp_session.on_session_quit().subscribe(lambda reason: print(f"session quit ({reason})"))
    session.cp_session.on_connected_changed().subscribe(lambda connected: print(f"connected to {registration.nickname}" if connected else "connection closed"))
    
    try:
        with session:
            controller, subscriptions = setup_controller(session.cp_session)

            try:
                profile = session.cp_session.get_video_profile()
                with GLVideoSurface(profile.width, profile.height, "chiaki-py") as surface:
                    # Every frame is converted to RGB into this device buffer, which must match the
                    # stream's size exactly (a PS5 streams what the profile says; a PS4 asked for 1080p
                    # downgrades to 720p once connected, which would not fit).
                    # Shaped (3, H, W) as the window wants it: a transposed view of that (H, W, 3) memory.
                    frame = cp.empty((profile.height, profile.width, 3), dtype=cp.uint8).transpose(2, 0, 1)

                    print("Streaming - press 'q' or Esc in the window, or Ctrl+C in the terminal, to quit.")
                    # max_fps=0: no limit, the stream already has its own frame rate.
                    for _ in session.frames(max_fps=0, out=frame):
                        surface.show(frame)
                        if surface.should_close:
                            break
            finally:
                if controller is not None and subscriptions is not None:
                    detach_controller(controller, session.cp_session, subscriptions)
    except KeyboardInterrupt:
        print("\nInterrupted, shutting down.")


if __name__ == "__main__":
    typer.run(main)
