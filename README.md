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

To keep frames in GPU memory instead of downloading them, use `session.gpu_frames()`. It yields `GpuFrame` handles (raw Vulkan/CUDA/D3D11 handles as integers plus format, size and timestamp) and needs a hardware decoder, e.g. `settings.set_hardware_decoder("cuda")`. Frames are NV12/P010, not RGB, and each one holds a slot in the decoder's frame pool until dropped, so release them promptly.

On an NVIDIA GPU, `session.cuda_frames()` (with `settings.set_hardware_decoder("cuda")`) yields `CudaFrame`s whose planes torch and cupy can wrap without a copy: `torch.as_tensor(frame.y, device="cuda")` or `cupy.asarray(frame.uv)`. `frame.y` is the (H, W) luma plane and `frame.uv` the (H/2, W/2, 2) interleaved chroma plane, so colour conversion to RGB is up to you. To get RGB instead, pass a uint8 CUDA tensor or array of shape (H, W, 3) as `out`, e.g. `session.cuda_frames(out=torch.empty((1080, 1920, 3), dtype=torch.uint8, device="cuda"))`: each frame is converted to RGB on the GPU into it and released right away.

## Examples

Run these from a clone of the repo:

- **`examples/discover_and_stream.py`**: the full path in one script: discover a console, PSN login, pair, then  tream with an optional DualSense attached. Start here. Pairing credentials are cached per console, so later runs skip login/pairing; pass `--force-pair` to pair again.
- **`examples/discover_hosts.py`**: just the network scan.
- **`examples/register_console.py`**: just PSN login + pairing, writes a `chiaki_py_config.json`.
- **`examples/gui_stream.py`**: a PyQt6 viewer that reads the config written by `register_console.py`.
- **`examples/stream_from_gpu.py`** / **`examples/stream_from_gpu_qt.py`**: show the stream in an OpenGL window (GLFW) or a Qt widget (`QOpenGLWidget`) straight from GPU memory via CUDA-OpenGL interop, so frames are never downloaded to the CPU. The frame rate is drawn in the top-right corner (NVIDIA only; the shared interop code is in `examples/cuda_gl.py`).
- **`examples/tensor_stream.py`**: streams straight into a PyTorch tensor on the GPU (NVIDIA + CUDA torch), so frames can be fed to a model without leaving the GPU. `discover_and_stream.py --cuda` does the same with a CuPy array.

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
