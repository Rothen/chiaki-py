import ctypes.util
import os
import sys


def use_xwayland() -> None:
    """On a Wayland session, have Qt open its windows on X11 (its xcb plugin), through XWayland.
    VulkanRenderer can draw into Qt's windows there but not into Wayland ones, whose wl_surface
    PyQt6 gives no access to. Must be called before the QApplication is made; leaves
    QT_QPA_PLATFORM alone if it is set already, and does nothing without XWayland (DISPLAY)."""
    if not sys.platform.startswith("linux") or os.environ.get("QT_QPA_PLATFORM"):
        return
    wayland = os.environ.get("WAYLAND_DISPLAY") or os.environ.get("XDG_SESSION_TYPE") == "wayland"
    if wayland and os.environ.get("DISPLAY"):
        os.environ["QT_QPA_PLATFORM"] = "xcb"


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
