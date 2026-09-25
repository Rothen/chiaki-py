"""Stream straight into a PyTorch tensor on the GPU, and show it from there.

The CUDA frame handler decodes with NVDEC and converts every frame to RGB on
the GPU, directly into a tensor you own, so the pixels stay on the GPU - which
is where a model wants them. The tensor is (3, H, W) uint8, channels first like
a model's input, over interleaved RGB memory (PyTorch's channels_last memory
format), because that is what the handler writes. For a model such as YOLO,
convert it with frame.unsqueeze(0).float().div(255) and resize. The picture is
drawn from the tensor's own GPU memory through CUDA-OpenGL interop (cuda_gl.py),
so no pixel ever reaches the CPU.

Usage:
    python examples/5_stream_tensor_opengl.py

The registration is the one written by examples/1.3_register_console.py. Press
'q' or Esc in the window, or Ctrl+C in the terminal, to quit.

Needs: an NVIDIA GPU that also renders the window, a CUDA build of PyTorch
(https://pytorch.org), and
    pip install cuda-python glfw PyOpenGL
"""

import sys
from pathlib import Path

import torch
import typer

from chiaki_py import Session, Serializer, HostRegistration
from chiaki_py.lib import Settings, CudaFrameHandler
from chiaki_py.controller import detach_controller

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
                    frame = CudaFrameHandler.empty_frame(profile.width, profile.height, backend="torch", channels_last=False)

                    print("Streaming - press 'q' or Esc in the window, or Ctrl+C in the terminal, to quit.")

                    for _ in session.frames(max_fps=0, out=frame):
                        surface.show(frame)                               # drawn from the tensor's own GPU memory
                        if surface.should_close:
                            break
            finally:
                if controller is not None and subscriptions is not None:
                    detach_controller(controller, session.cp_session, subscriptions)
    except KeyboardInterrupt:
        print("\nInterrupted, shutting down.")


if __name__ == "__main__":
    typer.run(main)
