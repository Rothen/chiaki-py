import ctypes.util
import os
import sys


def check_qt_platform_libs() -> None:
    """Raise a clear error if Qt's X11 (xcb) platform plugin is about to fail for lack of
    libxcb-cursor0. Qt 6.5+ needs it, PyQt6 wheels don't bundle it, and without it Qt aborts
    the whole process with a message that doesn't say how to install it."""
    if not sys.platform.startswith("linux"):
        return
    platform = os.environ.get("QT_QPA_PLATFORM", "")
    if platform and not platform.startswith("xcb"):
        return
    if not platform and os.environ.get("XDG_SESSION_TYPE") == "wayland":
        return  # Qt uses its Wayland plugin here, which doesn't need libxcb-cursor
    if ctypes.util.find_library("xcb-cursor") is None:
        raise RuntimeError(
            "Qt needs libxcb-cursor0 to open a window on X11. "
            "Install it with: sudo apt install libxcb-cursor0"
        )
