from importlib.metadata import PackageNotFoundError, version

from .discovery import discover_hosts
from .registration import connect_info_kwargs, register
from .session import Session

try:
    __version__ = version("chiaki-py")
except PackageNotFoundError:
    __version__ = "0.0.0"

__all__ = ["Session", "register", "connect_info_kwargs", "discover_hosts", "lib", "__version__"]
