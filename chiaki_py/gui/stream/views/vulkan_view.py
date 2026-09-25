"""Vulkan rendering for StreamDisplay, used when the session has a VulkanFrameHandler.

The decoder decodes on a Vulkan device (Settings.set_hardware_decoder("vulkan")) and the
window is drawn on that same device by VulkanRenderer, which uses libplacebo: it converts
the decoded NV12/P010 frames to RGB (SDR or HDR), scales them and presents them from where
they are, so the pixels never touch the CPU and are not even copied within the GPU. Works
with any GPU that has Vulkan video decoding, Windows only so far. No optional dependencies.
"""

import logging

from PyQt6.QtCore import Qt, QThread
from PyQt6.QtWidgets import QWidget

from chiaki_py import Session
from chiaki_py.lib import VulkanFrame, VulkanRenderer, ChiakiPySession
from .base_view import VideoMixin
from ..threads.frame_thread import FrameThread
from ..threads.fps_thread import FpsThread

_logger = logging.getLogger(__name__)


class VulkanRenderThread(QThread):
    """Presents the frames a FrameThread decodes on a VulkanRenderer, off of Qt entirely.

    The present call is vsync-locked (one frame in flight), so it blocks for up to a whole refresh
    interval; run from the GUI thread, that means every arriving frame would be presented one by one
    whenever the GUI thread falls behind (e.g. while the window is being dragged, which stalls Qt's event
    loop) - a backlog that is slow and jittery to catch up from, since Qt's event queue has no way to
    drop the frames it is holding. Pulling frames straight from FrameThread.wait_for_frame() instead,
    which only ever keeps the latest one, means a consumer that falls behind skips the ones in between
    rather than working through them, and none of it depends on Qt's event loop running at all.
    """

    def __init__(self, frame_thread: FrameThread, renderer: VulkanRenderer, fps_thread: FpsThread):
        super().__init__()
        self._frame_thread = frame_thread
        self._renderer = renderer
        self._fps_thread = fps_thread
        self._running = True

    def run(self) -> None:
        while self._running:
            frame = self._frame_thread.wait_for_frame()
            if frame is None or not self._running:
                continue
            try:
                self._fps_thread.tick_render()
                self._renderer.render(frame)
                self._fps_thread.tock_render()
            except (ValueError, RuntimeError):
                _logger.warning("Dropping unusable frame", exc_info=True)
        self._renderer.close()   # from this thread: the one the renderer was used from throughout

    def stop(self) -> None:
        """Stop presenting frames and close the renderer; blocks until both are done."""
        self._running = False
        self.wait()


class VulkanVideoWidget(VideoMixin[VulkanFrame], QWidget):
    """A native window that a stream session's frames are drawn into by a VulkanRenderer.

    Frames are pulled by a FrameProducer on its own thread; a VulkanRenderThread presents them, entirely
    off of Qt's event loop (see its docstring for why). This widget's own _on_frame (Qt's queued signal,
    on the GUI thread) only tracks the frame size, to notice the stream changing resolution - cheap
    bookkeeping, no longer the thing that blocks on presenting a frame. Vulkan owns what is drawn in the
    window, so Qt paints nothing and widgets on top of it would not show; the frame rate is a window of
    its own (a StatsOverlay) that follows this widget around, in the top-right corner, so it is never part
    of the frame that is drawn.
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
        self._render_thread: VulkanRenderThread | None = None

    def paintEngine(self):
        return None   # required with WA_PaintOnScreen: Qt must not try to paint into a window Vulkan draws in

    def _render(self) -> None:
        pass   # VulkanRenderThread presents frames on its own, straight off of FrameThread; nothing to do here

    def _get_frame_size(self) -> tuple[int, int]:
        return (self._frame.width, self._frame.height)

    def start(self) -> None:
        """Start drawing the session's frames. The widget must be shown already, for its native window to be there;
        raises RuntimeError if the session can't be drawn (it does not use the Vulkan decoder, ...)."""
        if self._renderer is not None:
            return
        self._renderer = VulkanRenderer(self.session.chiaki_py_session, int(self.winId()))
        self._render_thread = VulkanRenderThread(self._frame_thread, self._renderer, self._fps_thread)
        self._render_thread.start()
        super().start()

    def release(self) -> None:
        render_thread, self._render_thread = self._render_thread, None
        if render_thread is not None:
            render_thread.stop()       # also closes the renderer, before the window it draws into goes away
        self._renderer = None
        super().release()