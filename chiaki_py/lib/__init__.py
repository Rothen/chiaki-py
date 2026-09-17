"""The raw pybind11 extension module (built from `pybind/`), re-exported here
as `chiaki_py.lib` so it can live inside the same top-level `chiaki_py`
package as the Pythonic wrapper (`chiaki_py.Session` etc.) without a name
collision - the compiled module's own internal name is "chiaki_py" (baked in
by `PYBIND11_MODULE(chiaki_py, m)` in pybind/src/bindings.cpp), which is also
what this whole package is now called.

CMake builds the compiled `.pyd`/`.so` directly into this directory (see the
LIBRARY_/RUNTIME_OUTPUT_DIRECTORY target properties in pybind/CMakeLists.txt)
alongside the `.pyi` stubs it also generates here, so the import below is an
ordinary same-package import - no build-directory search needed.
"""

from .chiaki_py import *  # noqa: F401,F403

# `from .chiaki_py import *` above also binds `core` to the *native*
# `chiaki_py.lib.chiaki_py.core` submodule (it's in that module's __all__).
# Rebind it to the real `chiaki_py/lib/core/` package instead (a thin
# re-export of the same native submodules - see chiaki_py/lib/core/
# __init__.py for why it needs to be real files rather than just an
# attribute binding): dotted imports like `from chiaki_py.lib.core.log
# import Log` need `chiaki_py.lib.core` and `chiaki_py.lib.core.log` to
# resolve as actual files both for Python's import machinery and for static
# type checkers (Pylance/Pyright), which the native submodule - nested under
# `chiaki_py.lib.chiaki_py.core` - doesn't satisfy at this shorter path.
from . import core  # noqa: F401
