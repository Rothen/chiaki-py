"""The raw pybind11 extension module (built from `pybind/`), re-exported here
as `chiaki_py.lib` so it can live inside the same top-level `chiaki_py`
package as the Pythonic wrapper (`chiaki_py.Session` etc.) without a name
collision - the compiled module's own internal name is "chiaki_py" (baked in
by `PYBIND11_MODULE(chiaki_py, m)` in pybind/src/bindings.cpp), which is also
what this whole package is now called.

The compiled `.pyd`/`.so` isn't built into this directory - CMake still
builds it into `build/pybind/` (or `build-debug/pybind/`) as before. Rather
than require every consumer to add that directory to PYTHONPATH separately,
we extend this package's own `__path__` to include it, so the relative
import below finds it there via ordinary package-relative import machinery.
No files are copied and no rebuild is required to pick this up - just point
CHIAKI_PY_NATIVE_DIR (or the default build/pybind guess) at wherever your
build actually put it.
"""

import os as _os
import sys as _sys
import types as _types

_here = _os.path.dirname(__file__)
_repo_root = _os.path.normpath(_os.path.join(_here, "..", ".."))

_candidates = []
if _os.environ.get("CHIAKI_PY_NATIVE_DIR"):
    _candidates.append(_os.environ["CHIAKI_PY_NATIVE_DIR"])
_candidates += [
    _os.path.join(_repo_root, "build", "pybind"),
    _os.path.join(_repo_root, "build-debug", "pybind"),
]

for _candidate in _candidates:
    if _os.path.isdir(_candidate) and _candidate not in __path__:
        __path__.append(_candidate)
        break
else:
    raise ImportError(
        "Could not find the compiled chiaki_py extension. Build it first "
        "(see README.md), or set CHIAKI_PY_NATIVE_DIR to the directory "
        "containing chiaki_py.*.pyd/.so."
    )

from .chiaki_py import *  # noqa: F401,F403
from .chiaki_py import core  # noqa: F401

# pybind11's def_submodule() (used for `core` and its children, e.g.
# `core.log`) registers each one in sys.modules under the *native* module's
# own compiled-in name - "chiaki_py.core", "chiaki_py.core.log", etc. (see
# PYBIND11_MODULE(chiaki_py, m) in pybind/src/bindings.cpp) - regardless of
# where we've actually nested the compiled module in Python. Left alone,
# that breaks ordinary dotted imports like `from chiaki_py.lib.core.log
# import Log`, since sys.modules has no "chiaki_py.lib.core.log" entry, only
# the (here, wrong/shadowed) "chiaki_py.core.log". Re-register each of them
# under where they actually live so both `chiaki_py.lib.core.log.Log` and
# `from chiaki_py.lib.core.log import Log` work.
_sys.modules[f"{__name__}.core"] = core
for _attr_name in dir(core):
    _attr = getattr(core, _attr_name)
    if isinstance(_attr, _types.ModuleType):
        _sys.modules[f"{__name__}.core.{_attr_name}"] = _attr
