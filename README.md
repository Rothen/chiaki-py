# chiaki-py

PS4/PS5 Remote Play from Python, built on [chiaki-ng](https://github.com/streetpea/chiaki-ng): discover consoles on the network, pair with them, stream video and audio, and send controller input.

## Install

```bash
pip install chiaki-py
```

Python 3.11 to 3.14. Wheels are built for Windows, Ubuntu, macOS 15+ (Apple Silicon) and 64-bit Raspberry Pi OS; anywhere else, see [Building from source](#building-from-source).

## Quickstart

```python
from chiaki_py import Session, discover_hosts, register_host, Serializer, AudioSink
from chiaki_py.lib import Settings
from chiaki_py.psn import PSNLoginQt

host = discover_hosts(timeout=3.0)[0]
psn_account = PSNLoginQt.login()
registration = register_host(
    host=host.host_addr,
    psn_id=psn_account.user_rpid,  # or .online_id for a PS4
    pin="12345678",                # shown on the console's Link Device screen
    target=host.target,
)

with Session(registration) as session:
    audio_sink = AudioSink(session)
    for frame in session.frames(max_fps=60):
        ...  # frame is an (H, W, 3) uint8 numpy array
```

`Serializer.save(registration, path)` / `Serializer.load(HostRegistration, path)` persist a `registration` to JSON so you don't have to pair every run.

To hear the stream, create an `AudioSink(session)` before connecting: it plays the audio on the default output device (or `AudioSink(session, device=...)`, one of `AudioSink.devices()`) through SDL, which is bundled with the wheel, on its own thread and stops by itself when the session ends. The Qt `StreamDisplay` does this for you.

What `session.frames()` yields depends on the frame handler class passed as the third argument to `Session`:

- **`CpuFrameHandler`** (default): downloads every frame to system memory as the numpy array above.
- **`VulkanFrameHandler`** (any hardware decoder, e.g. `"vulkan"`, `"cuda"` or `"d3d11va"`, set with `settings.set_hardware_decoder(...)` before connecting): yields `VulkanFrame` handles — raw Vulkan/CUDA/D3D11 handles as integers plus format, size and timestamp, NV12/P010 rather than RGB. Each one holds a slot in the decoder's frame pool until dropped, so release them promptly.
- **`CudaFrameHandler`** (NVIDIA only, `settings.set_hardware_decoder("cuda")`): converts every frame to RGB on the GPU and yields a (H, W, 3) uint8 CuPy array. Pass a uint8 CUDA tensor or array of that shape as `out` to have the frame converted into it instead (e.g. a PyTorch tensor); `out` is then returned and released right away. `out` may also be (3, H, W), as long as it is a transpose/permute view of interleaved (H, W, 3) memory — `CudaFrameHandler.empty_frame(width, height, backend="torch", channels_last=False)` makes one. A separately allocated planar tensor such as `torch.empty((3, H, W), device="cuda")` is rejected, because the frame is always written as interleaved RGB.

```python
from chiaki_py.lib import CudaFrameHandler

settings.set_hardware_decoder("cuda")
with Session(registration, CudaFrameHandler) as session:
    for frame in session.frames():
        ...  # frame is a (H, W, 3) uint8 CuPy array on the GPU
```

## Logging

chiaki-py logs through Python's standard `logging` module, including the messages of the C libraries underneath it:

| Logger | What it logs |
| --- | --- |
| `chiaki_py.lib` | chiaki-ng: connecting, streaming, pairing and discovery |
| `chiaki_py.lib.placebo` | libplacebo, which draws `VulkanFrameHandler` frames (Windows) |
| `chiaki_py.lib.ffmpeg` | FFmpeg, which decodes the video (e.g. `Could not find ref with POC 39` after packet loss, which the stream recovers from) |
| `chiaki_py.*` | chiaki-py's own Python code |

Like any library, chiaki-py prints nothing until your program configures logging. To see what it is doing:

```python
import logging

logging.basicConfig(level=logging.INFO)
logging.getLogger("chiaki_py.lib.placebo").setLevel(logging.WARNING)  # loggers can be tuned one by one
logging.getLogger("chiaki_py.lib.ffmpeg").setLevel(logging.CRITICAL)   # e.g. hide decoder errors
```

chiaki-ng's `VERBOSE` and `DEBUG` levels, libplacebo's `TRACE` and FFmpeg's `VERBOSE`, `DEBUG` and `TRACE` all arrive as `DEBUG`. FFmpeg messages less severe than `INFO` are dropped before they reach Python.

These settings drop messages before they reach Python, which is cheaper than filtering them there. The stream sends many messages per second at the lowest levels, so use these rather than a logger level to silence them:

- `settings.set_log_level(LogLevel.WARNING)` sets the least severe chiaki-ng message from the `Session` that is still passed on (`LogLevel` is in `chiaki_py.lib`). The default, `LogLevel.DEBUG`, passes on everything. Discovery and registration are quiet enough that they always pass on everything but `VERBOSE`; filter those with the logger.
- `settings.set_log_verbose(True)` additionally passes on chiaki-ng's `VERBOSE` messages, which are off by default.
- For libplacebo, the `CHIAKI_PY_PLACEBO_LOG` environment variable does the same: `error`, `warning` (the default), `info`, `debug`, `trace` or `none`.

## Examples

Run these from the root of a clone of the repo, e.g. `python examples/1.2_discover_hosts.py`. The scripts share a `./cache` directory (relative to where you run them) for the PSN account and console registration, so run them from the same place every time. The `1.x` scripts are the step-by-step path; `2_...` does the same in one script; `3_...` to `5_...` are more ways to stream once you have paired.

### Step by step

1. **`1.1_login.py`**: PSN login. Opens a Qt web view to sign in (`--headless` for the terminal instead) and saves the account to `cache/psn_account.json`.
2. **`1.2_discover_hosts.py [--timeout 3.0]`**: scans the network and prints the consoles that answer. No login or pairing needed.
3. **`1.3_register_console.py <host> <pin> [--ps4] [--console-pin PIN]`**: pairs with the console at `<host>` using the PIN from its Link Device screen (PS5: Settings > System > Remote Play > Link Device; PS4: Settings > Remote Play Connection Settings > Add Device) and saves `cache/host_registration.json`. Needs the account from step 1. Defaults to a PS5; pass `--ps4` for a PS4.
4. **Stream** in the built-in PyQt6 `StreamDisplay`, which also plays the audio. Each of these loads `cache/host_registration.json` from step 3, attaches a DualSense if one is connected, and differs in how frames get on screen. Press `F` for the frame-rate overlay and `A` to lock the aspect ratio:
   - **`1.4.1_stream_cpu_qt.py`**: frames decoded to system memory. Works everywhere, no GPU needed.
   - **`1.4.2_stream_cuda_qt.py`**: same `StreamDisplay`, with `CudaFrameHandler` — frames are RGB-converted on the GPU and drawn by an OpenGL widget straight from GPU memory. NVIDIA only: `pip install cupy-cuda12x cuda-python PyOpenGL`.
   - **`1.4.3_stream_vulkan_qt.py`**: same `StreamDisplay`, with `VulkanFrameHandler` and the Vulkan hardware decoder. [libplacebo](https://code.videolan.org/videolan/libplacebo) draws the window on the very Vulkan device that decoded the frames, so nothing is copied, not even within the GPU. No CUDA/OpenGL, no extra packages — just a GPU and driver with Vulkan video decoding. Windows only so far.

### All in one

- **`2_discover_and_stream_opencv.py [--force-pair] [--headless] [--dir ./cache]`**: discover a console (you pick one if several answer), log in, pair and stream into an OpenCV window (`q` quits), all without the other scripts. Pairing is cached per console as `<console name>.json` in `--dir`; pass `--force-pair` to pair again. Frames arrive as a CPU numpy array, the easiest one to adapt for processing with OpenCV. `pip install opencv-python typer`.

### Without Qt

These also load `cache/host_registration.json` from step 3 and attach a DualSense if one is connected.

- **`3_simple_stream.py`**: the smallest viewer, a good starting point for your own code. Frames are decoded to system memory into one (H, W, 3) uint8 RGB numpy array that every frame overwrites, and shown with `cv2.imshow` (`q` in the window or Ctrl+C in the terminal quits). Audio goes through [miniaudio](https://github.com/irmen/pyminiaudio) instead of `AudioSink`, to show how to feed the session's audio queue to an audio library of your own (see `start_audio()` in `helpers.py`). Works everywhere, no GPU needed: `pip install opencv-python miniaudio`.
- **`4_stream_cuda_glfw.py`**: the CUDA path in a plain GLFW window (`q`/Esc quits), with the frame rate drawn in the top-right corner. NVIDIA only: `pip install cupy-cuda12x glfw opencv-python typer`.
- **`5_stream_tensor_opengl.py`**: like 4, but each frame lands in a (3, H, W) uint8 PyTorch tensor on the GPU (channels first, over interleaved memory, i.e. PyTorch's `channels_last` memory format). The tensor can feed a model such as YOLO without leaving the GPU (convert it with `.unsqueeze(0).float().div(255)`, then resize), and the window draws straight from it. NVIDIA plus a CUDA build of [PyTorch](https://pytorch.org): `pip install glfw opencv-python typer`.

### Shared code

`helpers.py` (attaches a DualSense with `setup_controller()`, and plays a session's audio through miniaudio with `start_audio()`; every example imports it, so they all need `pip install miniaudio`), `glfw_video.py` + `cuda_gl.py` (the GLFW window and CUDA-OpenGL interop used by 4 and 5; `GLVideoSurface.show()` takes any (3, H, W) uint8 CUDA array — torch or CuPy, planar or a view of interleaved memory), `fps_overlay.py` (the frame-rate overlay used by the GLFW and OpenCV examples).

## Building from source

Only needed for development, or for platforms/Python versions without a wheel. CMake fetches [chiaki-ng](https://github.com/streetpea/chiaki-ng) into `libs/` automatically on first configure; on Windows it also downloads FFmpeg into `deps/` and builds [libplacebo](https://code.videolan.org/videolan/libplacebo) there from source (shaderc as its shader compiler), so the first build is slow.

### Windows

Install [Git](https://git-scm.com/), [Python 3.11](https://www.python.org/downloads/), Visual Studio 2022+ (or Build Tools) with the *Desktop development with C++* workload, LLVM (for `clang-cl`), CMake and Ninja (e.g. `choco install llvm cmake ninja`), [Meson](https://mesonbuild.com/) (`pip install meson`, to build libplacebo), and [vcpkg](https://github.com/microsoft/vcpkg):

```powershell
git clone https://github.com/microsoft/vcpkg C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
setx VCPKG_ROOT C:\vcpkg
setx VCPKG_TOOLCHAIN $env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake    # then restart your terminal
```

Then, from a Visual Studio dev shell (*Developer PowerShell for VS*, or this repo's VS Code terminal):

```powershell
git clone https://github.com/Rothen/chiaki-py; cd chiaki-py

pip install "protobuf==7.36.2" "grpcio-tools==1.84.0" pybind11_stubgen

cmake --fresh -S . -B build-debug -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM="3.5" `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_TOOLCHAIN" `
  -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug --config Debug --target chiaki-py -- -j4

pip install -e .
```

`shaderc` and the Vulkan loader/headers come from vcpkg (`vcpkg.json`); the configure step builds [libplacebo](https://code.videolan.org/videolan/libplacebo) from source with them into `deps/`, which is slow the first time but skipped on later configures as long as `deps/lib/libplacebo.lib` is still there. Point `-DPLACEBO_ROOT=<path>` at an existing libplacebo build (with a shader compiler) to skip building it entirely. `.github/workflows/build-windows.yml` is the reference for this recipe, including the exact CMake/LLVM/vcpkg versions CI pins.

### Ubuntu

Native dependencies come from apt (FFmpeg, SDL2, protobuf, OpenSSL, etc.), rather than vcpkg:

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential ninja-build cmake git pkg-config nasm python3-dev \
  protobuf-compiler libprotobuf-dev \
  libopus-dev libjson-c-dev libminiupnpc-dev libpsl-dev libevdev-dev libevent-dev \
  libgf-complete-dev libspeexdsp-dev libidn2-dev libnghttp2-dev libssh2-1-dev \
  libfmt-dev libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libavdevice-dev \
  libsdl2-dev libhidapi-dev libssl-dev libfftw3-dev liblcms2-dev libvulkan-dev zlib1g-dev

git clone https://github.com/Rothen/chiaki-py && cd chiaki-py

pip install "protobuf==7.36.2" "grpcio-tools==1.84.0" pybind11_stubgen

cmake --fresh -S . -B build-release -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=/usr/bin/gcc -DCMAKE_CXX_COMPILER=/usr/bin/g++ \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON
cmake --build build-release --config Release --target chiaki-py -- -j"$(nproc)"

pip install -e .
```

### macOS

Apple Silicon (arm64) only — that's what CI builds.

```bash
xcode-select --install
brew install python@3.11 cmake ninja nasm pkgconf protobuf@29 openssl@3 fmt \
  opus json-c miniupnpc libevent speexdsp ffmpeg hidapi fftw lcms2 \
  vulkan-headers vulkan-loader
```

Build SDL2 from source instead of `brew install sdl2`: Homebrew's `sdl2` is now [sdl2-compat](https://github.com/libsdl-org/sdl2-compat), a shim that loads SDL3 via `dlopen()`, which can't be bundled into a wheel and hangs on import when SDL3 isn't found. `ffmpeg` installs it as a dependency anyway, so unlink it first:

```bash
brew unlink sdl2-compat 2>/dev/null || true
export SDL2_PREFIX="$HOME/.local/sdl2"
git clone --depth 1 --branch release-2.32.10 https://github.com/libsdl-org/SDL.git /tmp/SDL2-src
cmake -S /tmp/SDL2-src -B /tmp/SDL2-build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_INSTALL_PREFIX="$SDL2_PREFIX" \
  -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TESTS=OFF
cmake --build /tmp/SDL2-build && cmake --install /tmp/SDL2-build
```

Then build the extension:

```bash
git clone https://github.com/Rothen/chiaki-py && cd chiaki-py

pip install "protobuf==7.36.2" "grpcio-tools==1.84.0" pybind11_stubgen

# /opt/homebrew isn't on clang's default search path, and protobuf@29 is keg-only
export CPATH="$(brew --prefix)/include" LIBRARY_PATH="$(brew --prefix)/lib"
export PKG_CONFIG_PATH="$SDL2_PREFIX/lib/pkgconfig:$(brew --prefix openssl@3)/lib/pkgconfig:$(brew --prefix protobuf@29)/lib/pkgconfig"
export PATH="$(brew --prefix protobuf@29)/bin:$PATH"

# -rpath lets the extension find Homebrew and SDL2 libraries at runtime:
# pybind/CMakeLists.txt skips CMake's own RPATH on non-Windows builds.
cmake --fresh -S . -B build-release -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DPython3_EXECUTABLE="$(which python)" -DPython_EXECUTABLE="$(which python)" -DPYTHON_EXECUTABLE="$(which python)" \
  -DCMAKE_PREFIX_PATH="$SDL2_PREFIX;$(brew --prefix);$(brew --prefix openssl@3);$(brew --prefix protobuf@29)" \
  -DCMAKE_SHARED_LINKER_FLAGS="-Wl,-rpath,$(brew --prefix)/lib -Wl,-rpath,$SDL2_PREFIX/lib" \
  -DCMAKE_MODULE_LINKER_FLAGS="-Wl,-rpath,$(brew --prefix)/lib -Wl,-rpath,$SDL2_PREFIX/lib"
cmake --build build-release --config Release --target chiaki-py -- -j"$(sysctl -n hw.ncpu)"

pip install -e .
```

The exports only last for the current shell; set them again in a new terminal. `.github/workflows/build-macos.yml` is the reference for this recipe.

### Notes

All builds put the compiled module (and, on Windows, its DLLs) in `chiaki_py/lib/`. After C++ changes under `pybind/`, re-run only `cmake --build ... --target chiaki-py`; re-run the configure step too if you add or remove a source file.

## Known limitations

- **Audio:** `settings.set_audio_volume()` has no effect yet; set the volume on the output device instead.
- **Rumble/haptics:** controller feedback from the console isn't forwarded to your controller yet.
- **Microphone:** not sent to the console yet, so voice chat doesn't work.
- Only Windows, Ubuntu and macOS (Apple Silicon) are tested; other Linux distros and Intel Macs aren't supported yet.
- PS4 pairing is implemented but far less tested than PS5.

## License

AGPL-3.0-only (see `LICENSE`) — the compiled extension statically links [chiaki-ng](https://github.com/streetpea/chiaki-ng)'s AGPL-3.0-licensed `chiaki-lib`, so the combined work must be distributed under the same terms.
