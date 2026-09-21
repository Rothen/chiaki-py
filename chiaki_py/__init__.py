from importlib.metadata import PackageNotFoundError, version

from .discovery import discover_hosts
from .registration import register
from .session import Session
from .serializer import Serializer

try:
    __version__ = version("chiaki-py")
except PackageNotFoundError:
    __version__ = "0.0.0"

__all__ = ["Session", "Serializer", "register", "discover_hosts", "lib", "__version__"]
