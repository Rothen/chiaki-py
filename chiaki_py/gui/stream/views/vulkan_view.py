"""Vulkan rendering for StreamDisplay, used when the session has a VulkanFrameHandler.

The decoder decodes on a Vulkan device (Settings.set_hardware_decoder("vulkan")) and the
window is drawn on that same device by VulkanRenderer, which uses libplacebo: it converts
the decoded NV12/P010 frames to RGB (SDR or HDR), scales them and presents them from where
they are, so the pixels never touch the CPU and are not even copied within the GPU. Works
with any GPU that has Vulkan video decoding, Windows only so far. No optional dependencies.
"""

import logging

from PyQt6.QtCore import Qt
from PyQt6.QtGui import QPaintEvent
from PyQt6.QtWidgets import QWidget

from chiaki_py import Session
from chiaki_py.lib import VulkanFrame, VulkanRenderer, StreamSession
from .base_view import VideoMixin
from ..threads.frame_thread import FrameThread
from ..threads.fps_thread import FpsThread

_logger = logging.getLogger(__name__)


class VulkanVideoWidget(VideoMixin[VulkanFrame], QWidget):
    """A native window that a stream session's frames are drawn into by a VulkanRenderer.

    Frames are pulled by a FrameProducer on its own thread and handed over as a fresh VulkanFrame each
    time (a Qt signal, queued across threads); the slot here just marks the frame ready and asks Qt for
    a repaint, with the actual drawing done from paintEvent() - not straight from the slot. The present
    call is vsync-locked (one frame in flight), so it blocks for up to a whole refresh interval; doing it
    synchronously per arriving frame would render every backlogged frame one by one whenever the GUI
    thread falls behind (e.g. while the window is being dragged), which is slow and jittery to catch up
    from. Routing it through update()/paintEvent() instead lets Qt coalesce repaint requests the way it
    already does for the CPU and CUDA widgets, so only the latest frame is ever drawn once the GUI thread
    is free again. Vulkan owns what is drawn in the window, so Qt paints nothing else and widgets on top
    of it would not show; the frame rate is a window of its own (a StatsOverlay) that follows this widget
    around, in the top-right corner, so it is never part of the frame that is drawn.
    """

    def __init__(self, session: Session, frame_thread: FrameThread, fps_thread: FpsThread, parent=None):
        super().__init__(frame_thread, fps_thread, parent)
        self.session = session
        self.setAttribute(Qt.WidgetAttribute.WA_NativeWindow)
        self.setAttribute(Qt.WidgetAttribute.WA_DontCreateNativeAncestors)
        self.setAttribute(Qt.WidgetAttribute.WA_PaintOnScreen)
        self.setAttribute(Qt.WidgetAttribute.WA_NoSystemBackground)
        self.setAttribute(Qt.WidgetAttribute.WA_OpaquePaintEvent)

        self._renderer: VulkanRenderer | None = None

    def paintEngine(self):
        return None   # required with WA_PaintOnScreen: Qt must not try to paint into a window Vulkan draws in

    def _render(self) -> None:
        self.update()   # actually drawn from paintEvent(), so a backlog of frames coalesces into one

    def paintEvent(self, a0: QPaintEvent | None) -> None:
        if self._renderer is None or self._frame is None:
            return
        try:
            self._fps_thread.tick_render()
            self._renderer.render(self._frame)
            self._fps_thread.tock_render()
        except (ValueError, RuntimeError):
            _logger.warning("Dropping unusable frame", exc_info=True)

    def _get_frame_size(self) -> tuple[int, int]:
        return (self._frame.width, self._frame.height)
    
    def start(self) -> None:
        """Start drawing the session's frames. The widget must be shown already, for its native window to be there;
        raises RuntimeError if the session can't be drawn (it does not use the Vulkan decoder, ...)."""
        if self._renderer is not None:
            return
        self._renderer = VulkanRenderer(self.session.stream_session, int(self.winId()))
        super().start()

    def release(self) -> None:
        renderer, self._renderer = self._renderer, None
        if renderer is not None:
            renderer.close()           # before the window it draws into goes away
        super().release()