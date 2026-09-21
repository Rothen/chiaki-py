"""Keeps a window at a given aspect ratio while the user drags its edges.

Qt has no such option for a window, so on Windows this answers WM_SIZING: while a drag is in
progress Windows asks what rectangle the window should have, and the answer is adjusted to the
ratio before the window is resized, so it never takes a wrong size (and there is nothing to
correct afterwards). Elsewhere the lock does nothing.

The message is answered by replacing the window procedure of the native window, rather than with a
Qt native event filter: Qt does not pass WM_SIZING to the application-wide filter, and a window
made by QML has no nativeEvent() to override.
"""

import sys
from collections.abc import Callable

WM_SIZING = 0x0214
WM_NCDESTROY = 0x0082
WMSZ_LEFT, WMSZ_RIGHT, WMSZ_TOP, WMSZ_TOPLEFT, WMSZ_TOPRIGHT, WMSZ_BOTTOM, WMSZ_BOTTOMLEFT, WMSZ_BOTTOMRIGHT = range(1, 9)

Rect = tuple[int, int, int, int]   # left, top, right, bottom


def constrain_rect(edge: int, rect: Rect, extra_width: int, extra_height: int, aspect: float) -> Rect:
    """The window rectangle `rect` that is being dragged by `edge` (a WMSZ_* value), changed so that its
    client area, which is `extra_width` x `extra_height` smaller (frame, title bar), is `aspect` (width
    divided by height). The edge or corner under the mouse keeps following it; the opposite one stays put."""
    left, top, right, bottom = rect
    width, height = right - left - extra_width, bottom - top - extra_height
    if width < 1 or height < 1:
        return rect
    if edge in (WMSZ_LEFT, WMSZ_RIGHT):
        height = round(width / aspect)
    elif edge in (WMSZ_TOP, WMSZ_BOTTOM):
        width = round(height * aspect)
    elif width > height * aspect:      # a corner: grow whichever dimension is too small
        height = round(width / aspect)
    else:
        width = round(height * aspect)
    width, height = width + extra_width, height + extra_height

    if edge in (WMSZ_LEFT, WMSZ_TOPLEFT, WMSZ_BOTTOMLEFT):
        left = right - width
    else:
        right = left + width
    if edge in (WMSZ_TOP, WMSZ_TOPLEFT, WMSZ_TOPRIGHT):
        top = bottom - height
    else:
        bottom = top + height
    return left, top, right, bottom


if sys.platform == "win32":
    import ctypes
    from ctypes import wintypes

    GWLP_WNDPROC = -4
    _WNDPROC = ctypes.WINFUNCTYPE(ctypes.c_ssize_t, wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM)

    _user32 = ctypes.WinDLL("user32")
    _user32.GetWindowRect.argtypes = _user32.GetClientRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
    _user32.GetWindowRect.restype = _user32.GetClientRect.restype = wintypes.BOOL
    _set_window_proc = getattr(_user32, "SetWindowLongPtrW", None) or _user32.SetWindowLongW   # 32-bit Windows has no ...Ptr
    _set_window_proc.argtypes = [wintypes.HWND, ctypes.c_int, ctypes.c_void_p]
    _set_window_proc.restype = ctypes.c_void_p
    _user32.CallWindowProcW.argtypes = [ctypes.c_void_p, wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
    _user32.CallWindowProcW.restype = ctypes.c_ssize_t

    def _frame_extra(hwnd: int) -> tuple[int, int] | None:
        """How much larger the window is than its client area, in pixels."""
        window, client = wintypes.RECT(), wintypes.RECT()
        if not (_user32.GetWindowRect(hwnd, ctypes.byref(window)) and _user32.GetClientRect(hwnd, ctypes.byref(client))):
            return None
        return (window.right - window.left) - (client.right - client.left), \
               (window.bottom - window.top) - (client.bottom - client.top)


class AspectRatioLock:
    """Constrains a top-level window to an aspect ratio while it is resized by dragging.

    `aspect` returns the ratio, width divided by height, and is asked for on every drag step (0 or
    less means not to constrain the window yet, e.g. before the video's size is known). Give the window
    to it with attach() once it has a native window, and call remove() when the window is closed.
    """

    def __init__(self, aspect: Callable[[], float]):
        self._aspect = aspect
        self._hwnd = 0
        self._old_proc = 0
        self._new_proc = None          # kept referenced: Windows calls it for as long as the window lives

    def attach(self, hwnd: int) -> None:
        """Start constraining the window with this native handle (QWidget.winId(), QWindow.winId()).
        Does nothing off Windows, or when the window is already attached."""
        if sys.platform != "win32" or hwnd == self._hwnd:
            return
        self.remove()
        self._new_proc = _WNDPROC(self._window_proc)
        self._old_proc = _set_window_proc(hwnd, GWLP_WNDPROC, ctypes.cast(self._new_proc, ctypes.c_void_p))
        self._hwnd = hwnd

    def remove(self) -> None:
        if sys.platform == "win32" and self._hwnd:
            _set_window_proc(self._hwnd, GWLP_WNDPROC, self._old_proc)   # fails harmlessly if the window is gone
        self._hwnd = self._old_proc = 0

    def _window_proc(self, hwnd, message, wparam, lparam):
        try:
            if message == WM_SIZING:
                self._constrain(hwnd, wparam, lparam)
            elif message == WM_NCDESTROY:
                self._hwnd = 0         # nothing left to restore once the window is gone
        except Exception:              # an exception must not escape into Windows
            pass
        result = _user32.CallWindowProcW(self._old_proc, hwnd, message, wparam, lparam)
        return 1 if message == WM_SIZING else result   # TRUE: the rectangle was processed

    def _constrain(self, hwnd: int, edge: int, lparam: int) -> None:
        aspect = self._aspect()
        extra = _frame_extra(hwnd)
        if aspect <= 0 or extra is None:
            return
        rect = wintypes.RECT.from_address(lparam)
        rect.left, rect.top, rect.right, rect.bottom = constrain_rect(
            edge, (rect.left, rect.top, rect.right, rect.bottom), *extra, aspect)
