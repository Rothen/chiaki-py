"""Stream from the GPU into an OpenGL window - the pixels never touch the CPU.

NVDEC decodes the console's video on the GPU, chiaki-py converts each frame to
RGB on the GPU into a CUDA buffer, and CUDA-OpenGL interop (cuda_gl.py) then
hands that buffer to OpenGL: it is copied (GPU to GPU) into a pixel buffer
object, from which OpenGL fills a texture that is drawn on the screen. Compare
2_discover_and_stream_opencv.py, which downloads every frame to the CPU for
OpenCV, and 1.4.2_stream_gpu_qt.py, which does this inside a Qt window.

Usage:
    python examples/1.4.3_stream_cuda_glfw.py

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

    session = Session.connect(
        settings,
        registration,
        CudaFrameHandler
    )

    session.stream_session.on_session_quit().subscribe(lambda reason: print(f"session quit ({reason})"))
    session.stream_session.on_connected_changed().subscribe(lambda connected: print(f"connected to {registration.nickname}" if connected else "connection closed"))
    
    try:
        with session:
            controller_attached = setup_controller(session.stream_session)

            try:
                profile = session.stream_session.get_video_profile()
                with GLVideoSurface(profile.width, profile.height, "chiaki-py") as surface:
                    # Every frame is converted to RGB into this device buffer, which must match the
                    # stream's size exactly (a PS5 streams what the profile says; a PS4 asked for 1080p
                    # downgrades to 720p once connected, which would not fit).
                    frame = cp.empty((profile.height, profile.width, 3), dtype=cp.uint8)

                    print("Streaming - press 'q' or Esc in the window, or Ctrl+C in the terminal, to quit.")
                    # max_fps=0: no limit, the stream already has its own frame rate.
                    for _ in session.frames(max_fps=0, out=frame):
                        surface.show(frame.data.ptr)
                        if surface.should_close:
                            break
            finally:
                if controller_attached:
                    session.stream_session.release_right()
                    session.stream_session.release_left()
                    session.stream_session.send_feedback_state()
    except KeyboardInterrupt:
        print("\nInterrupted, shutting down.")


if __name__ == "__main__":
    typer.run(main)
