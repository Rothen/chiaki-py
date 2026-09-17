"""Re-export of the compiled `chiaki_py.lib.chiaki_py.core` submodule.

`chiaki_py.lib.core` (and `chiaki_py.lib.core.common` etc.) need to be real
files to resolve for static type checkers - Pylance/Pyright resolve the
module part of `from a.b.c import d` by looking for real files for
`a`/`a/b`/`a/b/c` on disk, not by executing `a`'s `__init__.py` and seeing
what `b` happens to be bound to at runtime. See the individual submodule
proxies (audio.py, common.py, ...) alongside this file for the same reason,
one level deeper.
"""

from . import (
    audio,
    base64,
    bitstream,
    common,
    controller,
    discovery_service,
    ecdh,
    fec,
    feedback,
    log,
)

__all__ = [
    "audio",
    "base64",
    "bitstream",
    "common",
    "controller",
    "discovery_service",
    "ecdh",
    "fec",
    "feedback",
    "log",
]
