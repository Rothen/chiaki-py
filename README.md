# chiaki-py

A single `chiaki_py` package for PS4/PS5 Remote Play from Python, built on
[chiaki-ng](https://git.streetpea.com/StreetPea/chiaki-ng) (an open-source
Remote Play client): discover consoles on the network, pair with them,
stream video, and send controller input.

## Layout

```bash
pybind/           C++ pybind11 extension module (builds the native part of chiaki_py.lib)
chiaki_py/          The chiaki_py package
  lib/                `chiaki_py.lib` - the raw native extension, re-exported here (see below)
  psn/                PSN OAuth login (`chiaki_py.psn`)
  controller/          DualSense input via `dualsense-py` (`chiaki_py.controller`)
  session.py, registration.py, discovery.py, config.py
examples/           Runnable scripts built on chiaki_py
libs/chiaki-ng/     chiaki-ng (the underlying Remote Play library), fetched by CMake
scripts/            Native dependency / build setup scripts
```

`chiaki_py.lib` mirrors chiaki-ng's C++ API closely: `StreamSession`, `Settings`, `Backend`, `DiscoveryManager`, raw frame/controller plumbing. Everything else in `chiaki_py` is a Pythonic layer on top of it - `Session`, `register()`, `discover_hosts()` - and is what you want for actually writing an application; reach into `chiaki_py.lib` only for something that layer doesn't expose yet.

`chiaki_py.lib` is implemented as a thin package (`chiaki_py/lib/__init__.py`) that re-exports the compiled extension, which CMake builds directly into that same directory (`chiaki_py/lib/`, alongside the `.pyi` stubs it also generates there - see `pybind/CMakeLists.txt`). This exists because the compiled module's own internal name is "chiaki_py" (set by `PYBIND11_MODULE(chiaki_py, m)` in `pybind/src/bindings.cpp`), and this whole package is now called that too; nesting it under `lib/` avoids the two colliding on import.

## Building the native extension

[chiaki-ng](https://github.com/streetpea/chiaki-ng) doesn't need to be
cloned or built by hand on either platform below. The top-level
`CMakeLists.txt`:

- fetches it into `libs/chiaki-ng` the first time it's configured, pinned to
  `CHIAKI_NG_VERSION` (a tag, branch, or commit; defaults to the version
  this repo currently targets - pass `-DCHIAKI_NG_VERSION=<tag>` to pin a
  different one). Once `libs/chiaki-ng` exists, it's left alone on later
  configures (never re-cloned or updated in place); delete the directory to
  fetch a different version from scratch.
- `add_subdirectory()`s it directly (with its GUI/CLI/tests/Android/Switch/
  Steam Deck-native pieces all disabled - chiaki-py only needs the core
  `chiaki-lib`), so it's built automatically as an ordinary dependency of
  `chiaki-py` - no separate configure/build step.

`-DPYTHON_EXECUTABLE` below must point at a Python that has `protobuf`
installed (`pip install protobuf`) - nanopb's code generator (used to build
chiaki-ng's protocol buffers) imports `google.protobuf`, and if this is
left unset, CMake may pick whichever Python happens to be first on `PATH`,
which can silently lack it and fail the build partway through. It should
also be a Python 3.11 interpreter to match the ABI the published wheel
targets (see `requires-python` in `pyproject.toml`) - building against a
different 3.x works fine for local development, it just tags the output
`.so`/`.pyd` for that version instead (e.g. `cp313` rather than `cp311`).

### Windows

Requires a native toolchain (this repo currently uses Ninja + clang-cl),
the Windows SDK, and [vcpkg](https://github.com/microsoft/vcpkg) for C
dependencies (FFmpeg, SDL2, protobuf, OpenSSL, opus, ...).

All commands below assume `cmake`, `clang-cl`, and `vcpkg` are already on
`PATH`, and that the MSVC environment (`LIB`, `INCLUDE`, etc.) is already
loaded - i.e. you're in a Visual Studio dev shell. Opening this repo's
integrated terminal in VS Code gives you that automatically (see
`.vscode/settings.json`, which launches PowerShell via
`Launch-VsDevShell.ps1`); from any other terminal, run that script yourself
first. `$env:VCPKG_ROOT` is expected to point at your vcpkg install.

Configure once:

```powershell
cmake --fresh -S . -B build-debug -G Ninja "-DCMAKE_POLICY_VERSION_MINIMUM=3.5" `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_BUILD_TYPE=Debug `
  -DPYTHON_EXECUTABLE="<python with protobuf installed>"
```

(Re-run this whenever `pybind/CMakeLists.txt` itself changes, e.g.
adding/removing a source file.)

Then, after any C++ change under `pybind/` or `libs/chiaki-ng/`:

```powershell
cmake --build build-debug --config Debug --target chiaki-py -- -j4
```

(The CMake target is `chiaki-py`, with a hyphen - `set_target_properties(...
OUTPUT_NAME "chiaki_py")` in `pybind/CMakeLists.txt` only renames the
*output file*, so `--target chiaki_py` won't resolve.)

The build produces `chiaki_py/lib/chiaki_py.cp<version>-win_amd64.pyd`
alongside every DLL it depends on (FFmpeg, OpenSSL, SDL2, ...) -
`chiaki_py.lib` imports it straight from there (see above).

### Ubuntu / Debian

Unlike Windows, native dependencies (FFmpeg, SDL2, protobuf, OpenSSL, opus,
Vulkan, ...) are linked from the distro's package manager instead of
vcpkg - `pybind/CMakeLists.txt` branches on `WIN32` for this throughout.

`scripts/build_chiaki_ubuntu.sh` does the whole thing end to end - installs
the apt packages (prompts for `sudo`), then configures and builds:

```bash
./scripts/build_chiaki_ubuntu.sh
```

It defaults `PYTHON_EXECUTABLE` to whatever `python3` resolves to on
`PATH`; export `PYTHON_EXECUTABLE` yourself first to point at a different
interpreter (e.g. a Python 3.11 venv/conda env - see the ABI note above).

Or by hand, equivalently:

```bash
sudo apt-get install -y \
    build-essential ninja-build cmake git pkg-config nasm \
    python3-dev \
    protobuf-compiler libprotobuf-dev \
    libopus-dev libjson-c-dev libminiupnpc-dev libpsl-dev libevdev-dev \
    libgf-complete-dev libspeexdsp-dev libidn2-dev libnghttp2-dev libssh2-1-dev \
    libfmt-dev libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libavdevice-dev \
    libsdl2-dev libhidapi-dev libssl-dev libfftw3-dev liblcms2-dev libvulkan-dev zlib1g-dev

cmake -S . -B build -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DCMAKE_C_COMPILER=/usr/bin/gcc -DCMAKE_CXX_COMPILER=/usr/bin/g++ \
  -DPYTHON_EXECUTABLE="<python with protobuf installed>"

cmake --build build --config Release --target chiaki-py -- -j$(nproc)
```

The build produces
`chiaki_py/lib/chiaki_py.cpython-<abi>-<arch>-linux-gnu.so`; `chiaki_py.lib`
imports it straight from there (see above).

If you build inside an activated conda/mamba environment, watch out for
`find_package(fmt)` resolving to that environment's own `fmt` package
instead of the apt one just installed - the two commonly have different
sonames (e.g. `libfmt.so.11` vs. this distro's `libfmt.so.10`), which links
fine but fails at import time with `libfmt.so.<N>: cannot open shared
object file`. `scripts/build_chiaki_ubuntu.sh` detects an active
`$CONDA_PREFIX` and pins `-Dfmt_DIR` to the system package automatically;
see the comment there if doing this by hand.

## Running without installing (development)

`chiaki_py` (everything except `chiaki_py.lib`'s compiled part) is pure Python, so there's no install step needed for it - just get the repo root onto `PYTHONPATH`:

```powershell
cd C:\Users\benir\Documents\Projects\chiaki-py
$env:PYTHONPATH = "$PWD"
python examples\discover_and_stream.py
```

```bash
cd ~/Projects/chiaki-py
PYTHONPATH="$PWD" python3 examples/discover_and_stream.py
```

Edits to `chiaki_py/` or `examples/` take effect immediately; edits under `pybind/` need a rebuild (see above).

## Installing as a package

```powershell
pip install chiaki-py
```

Only prebuilt for Windows/cp311 right now (see `requires-python` in
`pyproject.toml`) - it bundles a compiled extension, not pure Python.

Optional dependency groups (`pyproject.toml`):

| Extra        | Adds                                        | Needed for |
|--------------|----------------------------------------------|------------|
| `psn`        | requests, pycryptodomex, PyQt6(+WebEngine)  | PSN OAuth login, `chiaki_py.psn` |
| `gui`        | `psn` + PyQt6                               | `examples/gui_stream.py` |
| `cv`         | opencv-python                               | `examples/discover_and_stream.py`'s preview window |
| `controller` | `dualsense-py`                              | DualSense input via `chiaki_py.controller` |

```powershell
pip install chiaki-py[psn,cv,controller]
```

For local development, install from the repo instead:

```powershell
pip install -e .[psn,cv,controller]
```

### Releasing

`.github/workflows/release.yml` builds the wheel + sdist on every push/PR
(so a broken build is caught immediately) and publishes to PyPI whenever a
GitHub Release is published, via [PyPI Trusted
Publishing](https://docs.pypi.org/trusted-publishers/) - no stored API
token. One-time setup on pypi.org, before the first release: under the
project's (or, pre-first-publish, your account's pending publishers)
Publishing settings, add a trusted publisher for owner `Rothen`, repo
`chiaki-py`, workflow `release.yml`, environment `pypi`.

To cut a release: bump `version` in `pyproject.toml` and
`CHIAKI_PY_VERSION_MAJOR/MINOR/PATCH` in `CMakeLists.txt` (kept in sync -
the latter only shows up in a log line, but they should still match),
commit, tag (`vX.Y.Z`), and publish a GitHub Release from that tag - the
workflow does the rest.

To build a wheel locally without publishing, use
`scripts/build_wheel.ps1` directly (see its header comment).

## Examples

- **`examples/discover_and_stream.py`** - the full path in one script: broadcast-discover a console, PSN login, pair, then stream with an optional DualSense attached. Start here. Pairing credentials are cached per-console under your user data dir, so re-running it against an already-paired console skips PSN login/pairing entirely; pass `--force-pair` to pair again from scratch.
- **`examples/discover_hosts.py`** - just the network scan.
- **`examples/register_console.py`** - just PSN login + pairing, writes a `chiaki_py_config.json` for later use.
- **`examples/gui_stream.py`** - a PyQt6 viewer that reads a config file written by `register_console.py`.

## Quickstart (library usage)

```python
from chiaki_py import Session, discover_hosts, register, connect_info_kwargs
from chiaki_py.lib import Settings
from chiaki_py.psn.login import PSNLoginQt

settings = Settings()
host = discover_hosts(settings, timeout=3.0)[0]

psn_account = PSNLoginQt.load_or_get("psn_account.json")
result = register(
    settings,
    host=host.host_addr,
    psn_id=psn_account.user_rpid,  # or .online_id for a PS4
    pin="12345678",                # shown on the console's Link Device screen
    target=host.target,
)

with Session.connect(settings, **connect_info_kwargs(result, host=host.host_addr)) as session:
    for frame in session.frames(max_fps=60):
        ...  # frame is an (H, W, 3) uint8 numpy array
```

## Known limitations

- Verified building/running on Windows and Ubuntu (see above); other Linux distros and macOS aren't set up yet (`pybind/CMakeLists.txt`'s non-Windows branches assume apt package names/layouts).
- The published PyPI wheel is Windows/cp311-only right now (see "Installing as a package" below) - Linux builds are source-only for the moment, via the steps above.
- PS4 pairing/registration is implemented but has seen far less real-world testing than PS5 in this codebase.

## License

AGPL-3.0-only (see `LICENSE`) - the compiled extension statically links
[chiaki-ng](https://github.com/streetpea/chiaki-ng)'s AGPL-3.0-licensed
`chiaki-lib`, which requires the combined work to be distributed under the
same terms.
