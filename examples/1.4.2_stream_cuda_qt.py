"""PyQt6 remote-play viewer that renders on the GPU - the pixels never touch the CPU.

With the CudaFrameHandler every frame is converted to RGB on the GPU and drawn
straight from GPU memory by an OpenGL widget (see chiaki_py/gui/stream/gpu_view.py).
Compare 1.4.1_stream_cpu_qt.py, which decodes to system memory.

Usage:
    python examples/1.4.2_stream_cuda_qt.py

The registration is the one written by 1.3_register_console.py. Press F in the
window to show the frame rate.

Needs: an NVIDIA GPU that also renders the window, and
    pip install cupy-cuda12x cuda-python PyOpenGL
"""

import sys
from pathlib import Path

from chiaki_py import Session, Serializer, HostRegistration
from chiaki_py.gui import StreamDisplay
from chiaki_py.lib import Settings, LogLevel, CudaFrameHandler
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
    settings.set_log_level(LogLevel.ERROR)
    settings.set_hardware_decoder("cuda")

    session = Session(
        settings,
        registration,
        CudaFrameHandler
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
