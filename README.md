# chiaki-py

Python bindings for [chiaki-ng](https://git.streetpea.com/StreetPea/chiaki-ng) (an
open-source PS4/PS5 Remote Play client), plus `chiaki_py_client`, a Pythonic
wrapper around those bindings for connecting to, streaming from, and sending
controller input to a PS4/PS5 from Python.

## Layout

```
pybind/               C++ pybind11 extension module (builds to `chiaki_py`)
chiaki_py_client/      Pythonic wrapper library on top of `chiaki_py`
examples/               Runnable scripts built on chiaki_py_client
libs/chiaki-ng/         Vendored chiaki-ng (the underlying Remote Play library)
scripts/                Native dependency / build setup scripts
```

`chiaki_py` (native, from `pybind/`) mirrors chiaki-ng's C++ API closely:
`StreamSession`, `Settings`, `Backend`, `DiscoveryManager`, raw frame/controller
plumbing. `chiaki_py_client` (pure Python) is the layer meant for actually
writing an application against - `Session`, `register()`, `discover_hosts()`.
Prefer `chiaki_py_client` unless you need something it doesn't expose yet.

## Building the native extension

Requires a native toolchain (this repo currently uses Ninja + clang-cl on
Windows), the Windows SDK, and [vcpkg](https://github.com/microsoft/vcpkg)
for `pybind/`'s C dependencies (FFmpeg, SDL2, protobuf, OpenSSL, opus, ...).

Configure once:

```powershell
cmd /c '"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" && "C:\Program Files\CMake\bin\cmake.exe" --fresh -S . -B build -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_C_COMPILER="C:/Program Files/LLVM/bin/clang-cl.exe" -DCMAKE_CXX_COMPILER="C:/Program Files/LLVM/bin/clang-cl.exe" -DCMAKE_BUILD_TYPE=Release'
```

(Point `-DCMAKE_TOOLCHAIN_FILE` at your own vcpkg install if it's not at
`C:\vcpkg`. Re-run this whenever `pybind/CMakeLists.txt` itself changes, e.g.
adding/removing a source file.)

Then, after any C++ change under `pybind/`:

```powershell
cmd /c '"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" && "C:\Program Files\CMake\bin\cmake.exe" --build build --config Release --target chiaki_py.cp311-win_amd64.pyd -- -j4'
```

The `vcvars64.bat` prefix is required - without it, `clang-cl`/`lld-link`
can't find the Windows SDK's `kernel32.lib` etc. and linking fails outright.
This is a `cmd /c` one-liner rather than plain PowerShell because
`vcvars64.bat` is a batch script that sets environment variables for the
shell that calls it.

The build produces `build/pybind/chiaki_py.cp<version>-win_amd64.pyd`
alongside every DLL it depends on (FFmpeg, OpenSSL, SDL2, ...) - that one
directory is what needs to be importable as `chiaki_py`.

## Running without installing (development)

`chiaki_py_client` is pure Python, so there's no install step for it - just
get the repo root and the built extension onto `PYTHONPATH`:

```powershell
cd C:\Users\benir\Documents\Projects\chiaki-py
$env:PYTHONPATH = "$PWD;$PWD\build\pybind"
python examples\discover_and_stream.py
```

Edits to `chiaki_py_client/` or `examples/` take effect immediately; edits
under `pybind/` need a rebuild (see above).

## Installing as a package

```powershell
pip install -e .[psn,cv,controller]
$env:PYTHONPATH = "build\pybind"   # still needed - chiaki_py is native, not pip-installed
```

Optional dependency groups (`pyproject.toml`):

| Extra        | Adds                                   | Needed for |
|--------------|-----------------------------------------|------------|
| `psn`        | requests, pycryptodomex, PyQt6(+WebEngine) | PSN OAuth login, `chiaki_py_client.psn` |
| `gui`        | `psn` + PyQt6                          | `examples/gui_stream.py` |
| `cv`         | opencv-python                          | `examples/discover_and_stream.py`'s preview window |
| `controller` | `ds_py`                                | DualSense input via `chiaki_py_client.controller` |

## Examples

- **`examples/discover_and_stream.py`** - the full path in one script:
  broadcast-discover a console, PSN login, pair, then stream with an
  optional DualSense attached. Start here. Pairing credentials are cached
  per-console under your user data dir, so re-running it against an
  already-paired console skips PSN login/pairing entirely; pass
  `--force-pair` to pair again from scratch.
- **`examples/discover_hosts.py`** - just the network scan.
- **`examples/register_console.py`** - just PSN login + pairing, writes a
  `chiaki_py_config.json` for later use.
- **`examples/gui_stream.py`** - a PyQt6 viewer that reads a config file
  written by `register_console.py`.

## Quickstart (library usage)

```python
from chiaki_py import Settings
from chiaki_py_client import Session, discover_hosts, register, connect_info_kwargs
from chiaki_py_client.psn.login import PSNLoginQt

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

- Only verified building/running on Windows in this repo's current state;
  the vendored `pybind/CMakeLists.txt` paths (`vcpkg_installed`,
  `libs/chiaki-ng/build*`) are Windows-oriented.
- `chiaki_py_client.controller` depends on `ds_py` for DualSense input,
  which isn't published anywhere this README can point to - source it
  yourself and `pip install` it before using the `controller` extra.
- PS4 pairing/registration is implemented but has seen far less real-world
  testing than PS5 in this codebase.
