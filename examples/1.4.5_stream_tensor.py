"""Stream straight into a PyTorch tensor on the GPU, and show it from there.

The CUDA frame handler decodes with NVDEC and converts every frame to RGB on
the GPU, directly into a tensor you own, so the pixels stay on the GPU - which
is where a model wants them. Here each frame is turned into a (3, H, W) float
tensor and its per-channel means are computed on the GPU. The picture is drawn
from the tensor's own GPU memory through CUDA-OpenGL interop (cuda_gl.py), so
nothing but those three numbers, shown in the window title, ever reaches the CPU.

Usage:
    python examples/1.4.4_stream_tensor.py

The registration is the one written by examples/1.3_register_console.py. Press
'q' or Esc in the window, or Ctrl+C in the terminal, to quit.

Needs: an NVIDIA GPU that also renders the window, a CUDA build of PyTorch
(https://pytorch.org), and
    pip install cuda-python glfw PyOpenGL
"""

import sys
import time
from pathlib import Path

import torch
import typer

from chiaki_py import Session, Serializer, HostRegistration
from chiaki_py.lib import Settings, CudaFrameHandler

from glfw_video import GLVideoSurface
from helpers import setup_controller


def main() -> None:
    if not torch.cuda.is_available():
        sys.exit("PyTorch can't see a CUDA device: install a CUDA build of torch.")
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

    session.stream_session.on_session_quit().subscribe(lambda reason: _logger.info(f"session quit ({reason})"))
    session.stream_session.on_connected_changed().subscribe(lambda connected: _logger.info(f"connected to {registration.nickname}" if connected else "connection closed"))

    try:
        with session:
            controller_attached = setup_controller(session.stream_session)

            try:
                profile = session.stream_session.get_video_profile()
                with GLVideoSurface(profile.width, profile.height, "chiaki-py") as surface:
                    # Allocated once, refilled by every frame: (H, W, 3) uint8 RGB on the GPU. It has
                    # to match the stream's size exactly (a PS5 streams what the profile says; a PS4
                    # asked for 1080p downgrades to 720p once connected, which would not fit).
                    frame = torch.empty((profile.height, profile.width, 3), dtype=torch.uint8, device="cuda")

                    _logger.info("Streaming - press 'q' or Esc in the window, or Ctrl+C in the terminal, to quit.")
                    title_updated = 0.0
                    # max_fps=0: no limit, the stream already has its own frame rate.
                    for _ in session.frames(max_fps=0, out=frame):
                        # `frame` now holds the newest frame; work on it on the GPU.
                        x = frame.permute(2, 0, 1).float().div_(255)      # (3, H, W) floats in [0, 1]
                        red, green, blue = x.mean(dim=(1, 2)).tolist()    # per-channel means

                        surface.show(frame.data_ptr())                    # drawn from the tensor's own GPU memory

                        now = time.perf_counter()
                        if now - title_updated >= 0.25:                   # every frame would only flicker
                            surface.set_title(f"chiaki-py - mean RGB {red:.2f} {green:.2f} {blue:.2f}")
                            title_updated = now
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
