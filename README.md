# chiaki-py

PS4/PS5 Remote Play from Python, built on
[chiaki-ng](https://github.com/streetpea/chiaki-ng): discover consoles on the
network, pair with them, stream video, and send controller input.

## Installation

Prebuilt wheels are published for **Windows (x86_64)** and **Linux (x86_64)**
on **Python 3.11 only**.

### Windows

1. Install [Python 3.11](https://www.python.org/downloads/) (64-bit).
2. Create a virtual environment and install the package:

   ```powershell
   py -3.11 -m venv .venv
   .\.venv\Scripts\Activate.ps1
   pip install chiaki-py
   ```

### Ubuntu

1. Install Python 3.11. Ubuntu 24.04 ships 3.12 and 22.04 ships 3.10, so add the
   deadsnakes PPA if `python3.11` isn't available:

   ```bash
   sudo add-apt-repository ppa:deadsnakes/ppa
   sudo apt update
   sudo apt install python3.11 python3.11-venv
   ```

2. Create a virtual environment and install the package:

   ```bash
   python3.11 -m venv .venv
   source .venv/bin/activate
   pip install chiaki-py
   ```

The Linux wheel bundles its native libraries (FFmpeg, SDL2, ...), so no extra
system packages are needed for the core library. The Qt-based parts (PSN login,
`examples/gui_stream.py`) may additionally need `sudo apt install libxcb-cursor0`.

Other platforms and Python versions have to build from source (see below).

## Quickstart

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

`chiaki_py.lib` is the raw native extension and mirrors chiaki-ng's C++ API.
Everything else in `chiaki_py` (`Session`, `register()`, `discover_hosts()`, ...)
is a Pythonic layer on top of it - use that unless you need something it doesn't
expose yet.

## Examples

Run these from a clone of the repo:

- **`examples/discover_and_stream.py`** - the full path in one script: discover a console, PSN login, pair, then stream with an optional DualSense attached. Start here. Pairing credentials are cached per console, so later runs skip login/pairing; pass `--force-pair` to pair again.
- **`examples/discover_hosts.py`** - just the network scan.
- **`examples/register_console.py`** - just PSN login + pairing, writes a `chiaki_py_config.json`.
- **`examples/gui_stream.py`** - a PyQt6 viewer that reads the config written by `register_console.py`.

## Building from source

Only needed for development, or for platforms/Python versions without a wheel.
CMake fetches chiaki-ng into `libs/` automatically on first configure; on
Windows it also downloads FFmpeg into `deps/`, and the first build is slow.

### Windows (from source)

Install [Git](https://git-scm.com/), [Python 3.11](https://www.python.org/downloads/),
[Visual Studio](https://visualstudio.microsoft.com/) 2022 or newer (or Build Tools) with the
*Desktop development with C++* workload, LLVM (for `clang-cl`), CMake and Ninja
(e.g. `choco install llvm cmake ninja`), and [vcpkg](https://github.com/microsoft/vcpkg):

```powershell
git clone https://github.com/microsoft/vcpkg C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
setx VCPKG_ROOT C:\vcpkg    # then restart your terminal
```

Then, from a Visual Studio dev shell (*Developer PowerShell for VS*, or this repo's VS Code terminal):

```powershell
git clone https://github.com/Rothen/chiaki-py; cd chiaki-py

python -m venv .venv; .\.venv\Scripts\Activate.ps1
pip install "protobuf==5.29.3" "grpcio-tools==1.71.0" pybind11_stubgen

cmake --fresh -S . -B build-debug -G Ninja "-DCMAKE_POLICY_VERSION_MINIMUM=3.5" `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_BUILD_TYPE=Debug `
  -DPYTHON_EXECUTABLE="$((Get-Command python).Source)"
cmake --build build-debug --config Debug --target chiaki-py -- -j4

pip install -e .
```

### Ubuntu (from source)

```bash
git clone https://github.com/Rothen/chiaki-py && cd chiaki-py

python3.11 -m venv .venv && source .venv/bin/activate
./scripts/build_chiaki_ubuntu.sh    # installs apt packages (asks for sudo), then builds

pip install -e .
```

The script uses whichever `python3` is on `PATH`; export `PYTHON_EXECUTABLE`
first to build against a different interpreter. It links FFmpeg, SDL2, protobuf,
OpenSSL, etc. from apt rather than vcpkg.

### Notes

- Both builds put the compiled module (and, on Windows, its DLLs) in `chiaki_py/lib/`. After C++ changes under `pybind/`, re-run only the `cmake --build` step (the CMake target is `chiaki-py`, with a hyphen). Re-run the configure step too if you add or remove a source file.
- `-DPYTHON_EXECUTABLE` must point at a Python with `protobuf` and `grpcio-tools` installed (nanopb's code generator needs them), and keep protobuf on 5.x - the bundled nanopb breaks on newer releases.
- Pin a different chiaki-ng with `-DCHIAKI_NG_VERSION=<tag|branch|commit>`; delete `libs/chiaki-ng` first if it's already been fetched.
- Building against a Python other than 3.11 works, but tags the output for that version (e.g. `cp313`).
- On Ubuntu inside a conda environment, `find_package(fmt)` can pick up conda's `fmt` instead of apt's, which fails at import time with `libfmt.so.<N>: cannot open shared object file`. `build_chiaki_ubuntu.sh` handles this by pinning `-Dfmt_DIR` when `$CONDA_PREFIX` is set.

## Releasing

`build-windows.yml` and `build-ubuntu.yml` build the wheels on every push/PR.
`publish.yml` rebuilds them and publishes to PyPI when a GitHub Release is
published, using [Trusted Publishing](https://docs.pypi.org/trusted-publishers/)
(owner `Rothen`, repo `chiaki-py`, workflow `publish.yml`, environment `pypi`).

To cut a release: bump the version in `pyproject.toml`, `CMakeLists.txt`
(`CHIAKI_PY_VERSION_MAJOR/MINOR/PATCH`) and `chiaki_py/__init__.py`, commit, tag
`vX.Y.Z`, and publish a GitHub Release from that tag.

To test packaging without touching PyPI, run `publish.yml` manually from the
Actions tab: manual runs publish only to [TestPyPI](https://test.pypi.org/project/chiaki-py/)
(environment `testpypi`, which needs its own trusted publisher). Each upload needs
an unused version (e.g. `0.1.2.dev1`); install it with:

```bash
pip install --index-url https://test.pypi.org/simple/ --extra-index-url https://pypi.org/simple/ chiaki-py
```

To build a wheel locally, see the header of `scripts/build_wheel.ps1`.

## Known limitations

- Only Windows and Ubuntu are tested; other Linux distros and macOS aren't supported yet.
- Wheels are Windows/Linux x86_64 and cp311 only.
- PS4 pairing is implemented but far less tested than PS5.

## License

AGPL-3.0-only (see `LICENSE`) - the compiled extension statically links
chiaki-ng's AGPL-3.0-licensed `chiaki-lib`, so the combined work must be
distributed under the same terms.
