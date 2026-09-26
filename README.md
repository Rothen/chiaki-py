# chiaki-py

PS4/PS5 Remote Play from Python, built on [chiaki-ng](https://github.com/streetpea/chiaki-ng): discover consoles, pair, stream video and audio, and send controller input.

## Install

```bash
pip install chiaki-py
```

Python 3.11–3.14. Wheels for Windows, Ubuntu, macOS 15+ (Apple Silicon) and 64-bit Raspberry Pi OS; elsewhere, [build from source](#building-from-source).

On Linux, the Qt windows (`StreamDisplay`, `PSNLoginQt`) also need `sudo apt install libxcb-cursor0`.

## Quickstart

```python
from chiaki_py import Session, discover_hosts, register_host, Serializer, AudioSink
from chiaki_py.psn import PSNLoginQt

host = discover_hosts(timeout=3.0)[0]
psn_account = PSNLoginQt.login()
registration = register_host(
    host=host.host_addr,
    psn_id=psn_account.user_rpid,  # or .online_id for a PS4
    pin="12345678",                # shown on the console's Link Device screen
    target=host.target,
)
Serializer.save(registration, "registration.json")  # reload later with Serializer.load(HostRegistration, path)

with Session(registration) as session:
    audio_sink = AudioSink(session)  # plays audio on the default output device
    for frame in session.frames(max_fps=60):
        ...  # frame is an (H, W, 3) uint8 numpy array
```

### Frame handlers

The third argument to `Session` picks what `session.frames()` yields:

| Handler | Yields | Notes |
| --- | --- | --- |
| `CpuFrameHandler` (default) | (H, W, 3) uint8 numpy array | Works everywhere |
| `CudaFrameHandler` | (H, W, 3) uint8 CuPy array on the GPU | NVIDIA only; `settings.set_hardware_decoder("cuda")`. Pass `out=` to write into your own tensor (see `CudaFrameHandler.empty_frame`) |
| `VulkanFrameHandler` | `VulkanFrame` with raw GPU handles (NV12/P010) | Any hardware decoder; release frames promptly |

```python
from chiaki_py.lib import CudaFrameHandler

settings.set_hardware_decoder("cuda")
with Session(registration, CudaFrameHandler) as session:
    for frame in session.frames():
        ...
```

## Logging

chiaki-py logs through Python's `logging` under `chiaki_py.*`, including the native libraries (`chiaki_py.lib`, `chiaki_py.lib.ffmpeg`, `chiaki_py.lib.placebo`):

```python
import logging
logging.basicConfig(level=logging.INFO)
logging.getLogger("chiaki_py.lib.ffmpeg").setLevel(logging.CRITICAL)  # e.g. hide decoder errors
```

For noisy streams, filter at the source instead: `settings.set_log_level(LogLevel.WARNING)`, and `CHIAKI_PY_PLACEBO_LOG=warning` for libplacebo.

## Examples

Run from the repo root; the scripts share a `./cache` directory for the PSN account and console registration.

| Script | What it does | Extra packages |
| --- | --- | --- |
| `1.1_login.py` | PSN login, saved to `cache/` | |
| `1.2_discover_hosts.py` | List consoles on the network | |
| `1.3_register_console.py <host> <pin> [--ps4]` | Pair with a console | |
| `1.4.1_stream_cpu_qt.py` | Stream in a Qt window (CPU) | |
| `1.4.2_stream_cuda_qt.py` | Same, via CUDA + OpenGL (NVIDIA) | `cupy-cuda12x cuda-python PyOpenGL` |
| `1.4.3_stream_vulkan_qt.py` | Same, via Vulkan + libplacebo, zero-copy (Windows) | |
| `2_discover_and_stream_opencv.py` | Discover, log in, pair and stream in one script | `opencv-python typer` |
| `3_simple_stream.py` | Minimal OpenCV viewer; good starting point | `opencv-python` |
| `4_stream_cuda_glfw.py` | CUDA stream in a GLFW window (NVIDIA) | `cupy-cuda12x glfw opencv-python typer` |
| `5_stream_tensor_opengl.py` | Frames as PyTorch GPU tensors, e.g. for YOLO (NVIDIA) | CUDA PyTorch, `glfw opencv-python typer` |

All examples also need `pip install miniaudio`.

## Building from source

CMake fetches chiaki-ng automatically (and on Windows, FFmpeg and libplacebo — the first build is slow). The CI workflows in [.github/workflows/](.github/workflows/) are the reference recipes for each platform, including exact dependency versions.

```bash
git clone https://github.com/Rothen/chiaki-py && cd chiaki-py
pip install "protobuf==7.36.2" "grpcio-tools==1.84.0" pybind11_stubgen
cmake --fresh -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 <platform flags>
cmake --build build --target chiaki-py
pip install -e .
```

- **Windows:** Visual Studio 2022+ C++ tools, LLVM, CMake, Ninja, Meson and [vcpkg](https://github.com/microsoft/vcpkg). Run from a VS dev shell with `-DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl`.
- **Ubuntu:** native deps from apt; see [build-ubuntu.yml](.github/workflows/build-ubuntu.yml) for the package list.
- **macOS:** deps from Homebrew, plus SDL2 built from source (Homebrew's `sdl2` is an SDL3 shim that can't be bundled); see [build-macos.yml](.github/workflows/build-macos.yml).

After C++ changes, re-run only the `cmake --build` step.

## Known limitations

- `settings.set_audio_volume()` has no effect yet.
- Rumble/haptics and microphone aren't forwarded yet.
- PS4 pairing is far less tested than PS5.

## License

AGPL-3.0-only (see `LICENSE`), since the extension statically links chiaki-ng's AGPL-3.0 `chiaki-lib`.
