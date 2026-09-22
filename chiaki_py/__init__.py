"""Python bindings and a high-level API for Chiaki's PS4/PS5 Remote Play protocol: discover
consoles on the network, register with one, and stream its video/audio/input as a Session.

The low-level pybind11 bindings (StreamSession, Settings, DiscoveryManager, ...) live in
`chiaki_py.lib`; this package wraps them into the pythonic pieces re-exported below. GUI widgets
that can display a Session's frames are in the optional `chiaki_py.gui` subpackage (needs PyQt6).
"""

from importlib.metadata import PackageNotFoundError, version

from .discovery import discover_hosts
from .registration import register_host, HostRegistration
from .session import Session
from .serializer import Serializer

try:
    __version__ = version("chiaki-py")
except PackageNotFoundError:
    __version__ = "0.0.0"

__all__ = ["Session", "Serializer", "register_host", "discover_hosts", "HostRegistration", "discover_hosts", "__version__"]
