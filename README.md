# chiaki-py

PS4/PS5 Remote Play from Python, built on [chiaki-ng](https://github.com/streetpea/chiaki-ng): discover consoles on the network, pair with them, stream video, and send controller input.

## Quickstart

```python
from chiaki_py import Session, discover_hosts, register, Serializer
from chiaki_py.lib import Settings
from chiaki_py.psn.login import PSNLoginQt
from chiaki_py import Serializer

settings = Settings()
host = discover_hosts(settings, timeout=3.0)[0]
psn_account = PSNLoginQt.login()
registration = register(
    settings,
    host=host.host_addr,
    psn_id=psn_account.user_rpid,  # or .online_id for a PS4
    pin="12345678",                # shown on the console's Link Device screen
    target=host.target,
)

with Session.connect(settings, registration) as session:
    for frame in session.frames(max_fps=60):
        ...  # frame is an (H, W, 3) uint8 numpy array
```

What `session.frames()` yields depends on the frame handler class you pass as the third argument to `Session.connect`. The default, `CPUFrameHandler`, downloads every frame to system memory as the numpy array above. The other two keep frames in GPU memory and need a hardware decoder, set with `settings.set_hardware_decoder(...)` before connecting:

- **`GPUFrameHandler`** (any hardware decoder, e.g. `"vulkan"`, `"cuda"` or `"d3d11va"`) yields `GpuFrame` handles: raw Vulkan/CUDA/D3D11 handles as integers plus format, size and timestamp. Frames are NV12/P010, not RGB, and each one holds a slot in the decoder's frame pool until dropped, so release them promptly.
- **`CUDAFrameHandler`** (NVIDIA only, `settings.set_hardware_decoder("cuda")`) yields `CudaFrame`s whose planes torch and cupy can wrap without a copy: `torch.as_tensor(frame.y, device="cuda")` or `cupy.asarray(frame.uv)`. `frame.y` is the (H, W) luma plane and `frame.uv` the (H/2, W/2, 2) interleaved chroma plane, so colour conversion to RGB is up to you. To get RGB instead, pass a uint8 CUDA tensor or array of shape (H, W, 3) as `out`, e.g. `session.frames(out=torch.empty((1080, 1920, 3), dtype=torch.uint8, device="cuda"))`: each frame is converted to RGB on the GPU into it and released right away.

```python
from chiaki_py.lib import CUDAFrameHandler

settings.set_hardware_decoder("cuda")
with Session.connect(settings, registration, CUDAFrameHandler) as session:
    for frame in session.frames():
        ...  # frame is a CudaFrame
```

## Examples

Run these from the root of a clone of the repo, e.g. `python examples/1.2_discover_hosts.py`. The scripts share a `./cache` directory (relative to where you run them) for the PSN account and the console registration, so run them from the same place every time.

The `1.x` scripts are the step-by-step path; `2_...` does the same in one script.

### Step by step

1. **`examples/1.1_login.py`**: PSN login. Opens a Qt web view for you to sign in (`--headless` logs in from the terminal instead) and saves the account to `cache/psn_account.json`.
2. **`examples/1.2_discover_hosts.py [--timeout 3.0]`**: scans the network and prints the consoles that answer. No login or pairing needed.
3. **`examples/1.3_register_console.py <host> <pin> [--ps4] [--console-pin PIN]`**: pairs with the console at `<host>` using the PIN from its Link Device screen (PS5: Settings > System > Remote Play > Link Device; PS4: Settings > Remote Play Connection Settings > Add Device) and saves `cache/registration.json`. It needs the account from step 1 and exits if `cache/psn_account.json` isn't there. Defaults to a PS5; pass `--ps4` for a PS4.
4. **Stream.** Each of these loads `cache/registration.json` from step 3, attaches a DualSense if one is connected, and differs in how the frames get on screen:
   - **`examples/1.4.1_stream_qt.py`**: the built-in PyQt6 `StreamDisplay`, with frames decoded to system memory. Works everywhere, no GPU needed.
   - **`examples/1.4.2_stream_gpu_qt.py`**: the same `StreamDisplay`, but with the `CUDAFrameHandler`: frames are converted to RGB on the GPU and drawn by an OpenGL widget straight from GPU memory, never downloaded to the CPU. NVIDIA only: `pip install cupy-cuda12x cuda-python PyOpenGL`.
   - **`examples/1.4.3_stream_cuda_glfw.py`**: the same idea without Qt, in a plain GLFW window. Each frame lands in a CuPy array and CUDA-OpenGL interop hands it to OpenGL. NVIDIA only: `pip install cupy-cuda12x cuda-python glfw PyOpenGL`.
   - **`examples/1.4.4_stream_tensor.py`**: like 1.4.3 but the frame is a PyTorch tensor on the GPU, so it can be fed to a model without leaving the GPU. The example computes the per-channel mean colour on the GPU and shows it in the window title. NVIDIA plus a CUDA build of [PyTorch](https://pytorch.org): `pip install cuda-python glfw PyOpenGL`.
   - **`examples/1.4.5_stream_vulkan_qt.py`**: the same `StreamDisplay`, with the `GPUFrameHandler` and the Vulkan hardware decoder (`settings.set_hardware_decoder("vulkan")`). The window is drawn by a shader on the very Vulkan device that decoded the frames, so they are not copied at all, not even within the GPU. No CUDA or OpenGL, so it isn't tied to NVIDIA and needs no extra packages, only a GPU and driver with Vulkan video decoding. Windows only so far.

   The GLFW examples draw the frame rate in the top-right corner (`q` or Esc quits). In the Qt viewers it starts hidden: press `F` to show it, together with the time spent per frame (in the title bar for 1.4.5, where Vulkan draws over anything Qt puts on the window).

### All in one

- **`examples/2_discover_and_stream_opencv.py [--force-pair] [--headless] [--dir ./cache]`**: discover a console (you pick one if several answer), log in to PSN, pair, and stream into an OpenCV window (`q` quits) with the frame rate in the top-right corner, all without the other scripts. The pairing is cached per console as `<console name>.json` in `--dir`, so later runs skip login and pairing; pass `--force-pair` to pair again (e.g. after removing the device on the console). Frames come to the CPU as a numpy array, which makes it the easiest one to adapt if you want to process frames with OpenCV.

### Shared code

- **`examples/helpers.py`**: `setup_controller()`, which attaches the first DualSense it finds to the stream.
- **`examples/glfw_video.py`**, **`examples/cuda_gl.py`**: the GLFW window and the CUDA-OpenGL interop used by 1.4.3 and 1.4.4.
- **`examples/fps_overlay.py`**: the frame-rate counter and its top-right text overlay, used by the GLFW and OpenCV examples.

## Building from source

Only needed for development, or for platforms/Python versions without a wheel. CMake fetches [chiaki-ng](https://github.com/streetpea/chiaki-ng) into `libs/` automatically on first configure; on Windows it also downloads FFmpeg into `deps/`, and the first build is slow.

### Windows

Install [Git](https://git-scm.com/), [Python 3.11](https://www.python.org/downloads/), [Visual Studio](https://visualstudio.microsoft.com/) 2022 or newer (or Build Tools) with the *Desktop development with C++* workload, LLVM (for `clang-cl`), CMake and Ninja (e.g. `choco install llvm cmake ninja`), and [vcpkg](https://github.com/microsoft/vcpkg):

```powershell
git clone https://github.com/microsoft/vcpkg C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
setx VCPKG_ROOT C:\vcpkg    
setx VCPKG_TOOLCHAIN $env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake    # then restart your terminal
```

Then, from a Visual Studio dev shell (*Developer PowerShell for VS*, or this repo's VS Code terminal):

```powershell
git clone https://github.com/Rothen/chiaki-py; cd chiaki-py

pip install "protobuf==5.29.3" "grpcio-tools==1.71.0" pybind11_stubgen

cmake --fresh -S . -B build-debug -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM="3.5" `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_TOOLCHAIN" `
  -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug --config Debug --target chiaki-py -- -j4

pip install -e .
```

### Ubuntu

Install the native dependencies from apt; FFmpeg, SDL2, protobuf, OpenSSL, etc. come from there rather than vcpkg:

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential ninja-build cmake git pkg-config nasm python3-dev \
  protobuf-compiler libprotobuf-dev \
  libopus-dev libjson-c-dev libminiupnpc-dev libpsl-dev libevdev-dev libevent-dev \
  libgf-complete-dev libspeexdsp-dev libidn2-dev libnghttp2-dev libssh2-1-dev \
  libfmt-dev libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libavdevice-dev \
  libsdl2-dev libhidapi-dev libssl-dev libfftw3-dev liblcms2-dev libvulkan-dev zlib1g-dev
```

Then build the extension:

```bash
git clone https://github.com/Rothen/chiaki-py && cd chiaki-py

pip install "protobuf==5.29.3" "grpcio-tools==1.71.0" pybind11_stubgen

cmake --fresh -S . -B build-release -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=/usr/bin/gcc -DCMAKE_CXX_COMPILER=/usr/bin/g++ \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON
cmake --build build-release --config Release --target chiaki-py -- -j"$(nproc)"

pip install -e .
```

### macOS

Apple Silicon (arm64) only - that's what CI builds. Install the Xcode command line tools, [Homebrew](https://brew.sh/) and Python 3.11, then the native dependencies:

```bash
xcode-select --install
brew install python@3.11 cmake ninja nasm pkgconf protobuf@29 openssl@3 fmt \
  opus json-c miniupnpc libevent speexdsp ffmpeg hidapi fftw lcms2 \
  vulkan-headers vulkan-loader
```

Build SDL2 from source instead of using `brew install sdl2`: Homebrew's `sdl2` is now [sdl2-compat](https://github.com/libsdl-org/sdl2-compat), a shim that loads SDL3 with `dlopen()`, which can't be bundled into a wheel and hangs on import when SDL3 isn't found. `ffmpeg` installs it as a dependency anyway, so unlink it to keep CMake from
finding it first:

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

pip install "protobuf==5.29.3" "grpcio-tools==1.71.0" pybind11_stubgen

# /opt/homebrew isn't on clang's default search path, and protobuf@29 is keg-only
export CPATH="$(brew --prefix)/include" LIBRARY_PATH="$(brew --prefix)/lib"
export PKG_CONFIG_PATH="$SDL2_PREFIX/lib/pkgconfig:$(brew --prefix openssl@3)/lib/pkgconfig:$(brew --prefix protobuf@29)/lib/pkgconfig"
export PATH="$(brew --prefix protobuf@29)/bin:$PATH"

# The -rpath flags let the extension find the Homebrew and SDL2 libraries at
# runtime: pybind/CMakeLists.txt skips CMake's own RPATH on non-Windows builds.
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

The exports only last for the current shell; set them again before re-running CMake in a new terminal. `.github/workflows/build-macos.yml` is the reference for this recipe.

### Notes

- All builds put the compiled module (and, on Windows, its DLLs) in `chiaki_py/lib/`. After C++ changes under `pybind/`, re-run only the `cmake --build` step (the CMake target is `chiaki-py`, with a hyphen). Re-run the configure step too if you add or remove a source file.

## Known limitations

- **Audio:** the console's audio is not played or exposed to Python yet; only video frames are delivered. The audio settings exist but currently have no effect.
- **Rumble and haptics:** controller rumble and haptic feedback from the console aren't forwarded to your controller yet.
- **Microphone:** microphone input isn't sent to the console yet, so voice chat doesn't work.
- Only Windows, Ubuntu and macOS (Apple Silicon) are tested; other Linux distros and Intel Macs aren't supported yet.
- PS4 pairing is implemented but far less tested than PS5.

## License

AGPL-3.0-only (see `LICENSE`) - the compiled extension statically links [chiaki-ng](https://github.com/streetpea/chiaki-ng)'s AGPL-3.0-licensed `chiaki-lib`, so the combined work must be distributed under the same terms.
