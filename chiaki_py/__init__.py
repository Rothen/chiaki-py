"""A Pythonic client library for PS4/PS5 Remote Play.

`chiaki_py.lib` is the raw pybind11 extension (built from `pybind/`,
mirroring chiaki-ng's C++ API closely: `StreamSession`, `Settings`,
`Backend`, `DiscoveryManager`, ...). Everything else in this top-level
package - `Session`, `register()`, `discover_hosts()` - is a thin Python
layer on top of it, and is what you want unless `chiaki_py.lib` has
something this layer doesn't expose yet.

`psn` (PSN OAuth login) and `controller` (DualSense input) pull in extra
dependencies (requests/pycryptodome+PyQt6, dualsensepy respectively) and are
imported lazily - use `from chiaki_py.psn import login` /
`from chiaki_py.controller import dualsense` when you actually need them.
"""

from importlib.metadata import PackageNotFoundError, version

from .discovery import discover_hosts
from .registration import connect_info_kwargs, register
from .session import Session

try:
    __version__ = version("chiaki-py")
except PackageNotFoundError:  # running from a source tree that was never pip-installed
    __version__ = "0.0.0"

__all__ = ["Session", "register", "connect_info_kwargs", "discover_hosts", "lib", "__version__"]
