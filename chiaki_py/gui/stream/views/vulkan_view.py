"""Vulkan rendering for StreamDisplay, used when the session has a VulkanFrameHandler.

The decoder decodes on a Vulkan device (Settings.set_hardware_decoder("vulkan")) and the
window is drawn on that same device by VulkanRenderer, which uses libplacebo: it converts
the decoded NV12/P010 frames to RGB (SDR or HDR), scales them and presents them from where
they are, so the pixels never touch the CPU and are not even copied within the GPU. Works
with any GPU that has Vulkan video decoding, Windows only so far. No optional dependencies.
"""

from PyQt6.QtCore import Qt
from PyQt6.QtWidgets import QWidget

from chiaki_py import Session
from chiaki_py.lib import VulkanFrame, VulkanFrameHandler, VulkanRenderer, StreamSession
from .base_view import BaseView, VideoMixin


class VulkanVideoWidget(VideoMixin[VulkanFrame], QWidget):
    """A native window that a stream session's frames are drawn into by a VulkanRenderer.

    Frames are pulled on the GUI thread: the decoder's "frame available" event becomes a Qt
    signal (queued across threads) and its slot hands the newest frame to the renderer, which
    only records and submits the drawing (a fraction of a millisecond) - the GPU does the rest.
    Vulkan owns what is drawn in the window, so Qt paints nothing and widgets on top of it
    would not show; the frame rate is a window of its own (a StatsOverlay) that follows this
    widget around, in the top-right corner, so it is never part of the frame that is drawn.
    """

    def __init__(self, stream_session: StreamSession, handler, width: int, height: int, show_stats: bool = False, parent=None):
        super().__init__(stream_session, handler, width, height, VulkanFrameHandler.empty_frame(), show_stats, parent)
        self.setAttribute(Qt.WidgetAttribute.WA_NativeWindow)
        self.setAttribute(Qt.WidgetAttribute.WA_DontCreateNativeAncestors)
        self.setAttribute(Qt.WidgetAttribute.WA_PaintOnScreen)
        self.setAttribute(Qt.WidgetAttribute.WA_NoSystemBackground)
        self.setAttribute(Qt.WidgetAttribute.WA_OpaquePaintEvent)

        self._renderer: VulkanRenderer | None = None

    def paintEngine(self):
        return None   # required with WA_PaintOnScreen: Qt must not try to paint into a window Vulkan draws in

    def _frame_pulled(self) -> None:
        if self._renderer is None or self._frame is None:
            return
        self._renderer.render(self._frame)

    def _get_frame_size(self) -> tuple[int, int]:
        return (self._frame.width, self._frame.height)
    
    def start(self) -> None:
        """Start drawing the session's frames. The widget must be shown already, for its native window to be there;
        raises RuntimeError if the session can't be drawn (it does not use the Vulkan decoder, ...)."""
        if self._renderer is not None:
            return
        self._renderer = VulkanRenderer(self._stream_session, int(self.winId()))
        super().start()

    def release(self) -> None:
        renderer, self._renderer = self._renderer, None
        if renderer is not None:
            renderer.close()           # before the window it draws into goes away
        super().release()



class VulkanStreamWindow(BaseView[VulkanVideoWidget]):
    """A window around a VulkanVideoWidget. F shows or hides the frame rate and the time needed per frame in
    the top-right corner (getting the frame from the decoder plus recording and submitting its drawing, not
    counting the GPU's own work); A locks or unlocks the video's aspect ratio while the window is resized. With
    `keep_aspect_ratio` the window starts with it locked, so there are no bars."""

    def __init__(self, session: Session, show_stats: bool = False, keep_aspect_ratio: bool = False):
        super().__init__(session, VulkanVideoWidget, show_stats, keep_aspect_ratio)