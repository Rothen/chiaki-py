"""A Pythonic client library on top of the raw `chiaki_py` extension module.

Core pieces (`Session`, `register`, `discover_hosts`) only depend on the
compiled `chiaki_py` extension and are imported eagerly here. `psn` (PSN
OAuth login) and `controller` (DualSense input) pull in extra dependencies
(requests/pycryptodome, PyQt6, ds_py) and are imported lazily - use
`from chiaki_py_client.psn import login` / `from chiaki_py_client.controller
import dualsense` when you actually need them.
"""

from .discovery import discover_hosts
from .registration import connect_info_kwargs, register
from .session import Session

__all__ = ["Session", "register", "connect_info_kwargs", "discover_hosts"]
