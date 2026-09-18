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
  controller/          DualSense input via `dualsensepy` (`chiaki_py.controller`)
  session.py, registration.py, discovery.py, config.py
examples/           Runnable scripts built on chiaki_py
libs/chiaki-ng/     chiaki-ng (the underlying Remote Play library), fetched by CMake
scripts/            Native dependency / build setup scripts
```

`chiaki_py.lib` mirrors chiaki-ng's C++ API closely: `StreamSession`, `Settings`, `Backend`, `DiscoveryManager`, raw frame/controller plumbing. Everything else in `chiaki_py` is a Pythonic layer on top of it - `Session`, `register()`, `discover_hosts()` - and is what you want for actually writing an application; reach into `chiaki_py.lib` only for something that layer doesn't expose yet.

`chiaki_py.lib` is implemented as a thin package (`chiaki_py/lib/__init__.py`) that re-exports the compiled extension, which CMake builds directly into that same directory (`chiaki_py/lib/`, alongside the `.pyi` stubs it also generates there - see `pybind/CMakeLists.txt`). This exists because the compiled module's own internal name is "chiaki_py" (set by `PYBIND11_MODULE(chiaki_py, m)` in `pybind/src/bindings.cpp`), and this whole package is now called that too; nesting it under `lib/` avoids the two colliding on import.

## Building the native extension

Requires a native toolchain (this repo currently uses Ninja + clang-cl on Windows), the Windows SDK, and [vcpkg](https://github.com/microsoft/vcpkg) for `pybind/`'s C dependencies (FFmpeg, SDL2, protobuf, OpenSSL, opus, ...).

[chiaki-ng](https://github.com/streetpea/chiaki-ng) itself doesn't need to be cloned by hand - the top-level `CMakeLists.txt` fetches it into `libs/chiaki-ng` on first configure, pinned to `CHIAKI_NG_VERSION` (a tag, branch, or commit; defaults to the version this repo currently targets). Pass `-DCHIAKI_NG_VERSION=<tag>` on the configure line to pin a different one. Once `libs/chiaki-ng` exists, it's left alone on later configures (it won't be re-cloned or updated); delete the directory to fetch a different version from scratch. It still needs to be built on its own with its own CMake project (see `scripts/build_chiaki_lib_debug.ps1`) before `pybind/` can link against it.

Configure once:

```powershell
cmd /c '"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" && "C:\Program Files\CMake\bin\cmake.exe" --fresh -S . -B build -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_C_COMPILER="C:/Program Files/LLVM/bin/clang-cl.exe" -DCMAKE_CXX_COMPILER="C:/Program Files/LLVM/bin/clang-cl.exe" -DCMAKE_BUILD_TYPE=Release'
```

(Point `-DCMAKE_TOOLCHAIN_FILE` at your own vcpkg install if it's not at `C:\vcpkg`. Re-run this whenever `pybind/CMakeLists.txt` itself changes, e.g. adding/removing a source file.)

Then, after any C++ change under `pybind/`:

```powershell
cmd /c '"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" && "C:\Program Files\CMake\bin\cmake.exe" --build build --config Release --target chiaki_py.cp311-win_amd64.pyd -- -j4'
```

The `vcvars64.bat` prefix is required - without it, `clang-cl`/`lld-link` can't find the Windows SDK's `kernel32.lib` etc. and linking fails outright. This is a `cmd /c` one-liner rather than plain PowerShell because `vcvars64.bat` is a batch script that sets environment variables for the shell that calls it.

The build produces `chiaki_py/lib/chiaki_py.cp<version>-win_amd64.pyd` alongside every DLL it depends on (FFmpeg, OpenSSL, SDL2, ...) - `chiaki_py.lib` imports it straight from there (see above).

## Running without installing (development)

`chiaki_py` (everything except `chiaki_py.lib`'s compiled part) is pure Python, so there's no install step needed for it - just get the repo root onto `PYTHONPATH`:

```powershell
cd C:\Users\benir\Documents\Projects\chiaki-py
$env:PYTHONPATH = "$PWD"
python examples\discover_and_stream.py
```

Edits to `chiaki_py/` or `examples/` take effect immediately; edits under `pybind/` need a rebuild (see above).

## Installing as a package

```powershell
pip install -e .
```

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

- Only verified building/running on Windows in this repo's current state; the vendored `pybind/CMakeLists.txt` paths (`vcpkg_installed`, `libs/chiaki-ng/build*`) are Windows-oriented.
- PS4 pairing/registration is implemented but has seen far less real-world testing than PS5 in this codebase.
