"""Vulkan rendering for StreamDisplay, used when the session has a GPUFrameHandler.

The decoder decodes on a Vulkan device (Settings.set_hardware_decoder("vulkan")) and the
window is drawn on that same device by VulkanRenderer: the decoded NV12/P010 frames are
converted to RGB by a shader and presented from where they are, so the pixels never touch
the CPU and are not even copied within the GPU. Works with any GPU that has Vulkan video
decoding, Windows only so far. No optional dependencies.
"""

import logging
import time

from PyQt6.QtCore import Qt, pyqtSignal
from PyQt6.QtWidgets import QMainWindow, QWidget

from chiaki_py import Session
from chiaki_py.gui.stream.aspect_ratio import AspectRatioLock
from chiaki_py.gui.stream.frame_stats import FrameStats
from chiaki_py.lib import GpuFrame, StreamSession, VulkanRenderer

_logger = logging.getLogger(__name__)

TITLE = "Live Image Stream"


class VulkanVideoWidget(QWidget):
    """A native window that a stream session's frames are drawn into by a VulkanRenderer.

    Frames are pulled on the GUI thread: the decoder's "frame available" event becomes a Qt
    signal (queued across threads) and its slot hands the newest frame to the renderer, which
    only records and submits the drawing (a fraction of a millisecond) - the GPU does the rest.
    Vulkan owns what is drawn in the window, so Qt paints nothing and widgets on top of it
    would not show; the frame rate is reported by `stats_changed` instead.
    """

    _frame_available = pyqtSignal()   # emitted from the decoder's thread, delivered on the GUI thread
    stream_size_changed = pyqtSignal(int, int)   # the frames turned out to be another size than expected
    stats_changed = pyqtSignal(str)

    def __init__(self, stream_session: StreamSession, handler, show_stats: bool = False, parent=None):
        super().__init__(parent)
        # The renderer needs a window of its own to make a Vulkan surface from, and no one else may paint it
        self.setAttribute(Qt.WidgetAttribute.WA_NativeWindow)
        self.setAttribute(Qt.WidgetAttribute.WA_DontCreateNativeAncestors)
        self.setAttribute(Qt.WidgetAttribute.WA_PaintOnScreen)
        self.setAttribute(Qt.WidgetAttribute.WA_NoSystemBackground)
        self.setAttribute(Qt.WidgetAttribute.WA_OpaquePaintEvent)

        self._stream_session = stream_session
        self._handler = handler
        self._renderer: VulkanRenderer | None = None
        self._subscription = None
        self._frame: GpuFrame | None = None   # handed back to the handler for every frame, so it takes the new one over
        self._size: tuple[int, int] | None = None
        self._warned = False

        self._stats = FrameStats()
        self.stats_visible = show_stats

    def paintEngine(self):
        return None   # required with WA_PaintOnScreen: Qt must not try to paint into a window Vulkan draws in

    def start(self) -> None:
        """Start drawing the session's frames. The widget must be shown already, for its native window to be there;
        raises RuntimeError if the session can't be drawn (it does not use the Vulkan decoder, ...)."""
        if self._renderer is not None:
            return
        self._renderer = VulkanRenderer(self._stream_session, int(self.winId()))
        self._frame_available.connect(self._pull)
        self._subscription = self._stream_session.on_frame_available().subscribe(lambda _: self._frame_available.emit())

    def set_stats_visible(self, visible: bool) -> None:
        self.stats_visible = visible
        self.stats_changed.emit((self._stats.summary() or "") if visible else "")

    def _pull(self) -> None:
        renderer = self._renderer
        if renderer is None:
            return                     # a signal that was still queued when the widget was released
        try:
            started = time.perf_counter()
            frame = self._handler.get_frame(self._frame)
            got = time.perf_counter()
            if frame is None:
                return
            self._frame = frame
            renderer.render(frame)
        except RuntimeError:
            # An exception escaping a slot would abort the application, so report it (once) instead.
            if not self._warned:
                self._warned = True
                _logger.warning("Dropping unusable frames", exc_info=True)
            return
        done = time.perf_counter()

        size = (frame.width, frame.height)
        if size != self._size:
            first = self._size is None
            self._size = size
            if not first:
                self.stream_size_changed.emit(*size)   # a PS4 asked for 1080p downgrades to 720p once connected

        self._stats.add_get(got - started)
        self._stats.add_paint(done - got)
        self._stats.tick()
        if self.stats_visible:
            summary = self._stats.summary()
            if summary is not None:
                self.stats_changed.emit(summary)

    def release(self) -> None:
        """Stop listening for frames and free the GPU resources; the widget must not be used afterwards."""
        if self._subscription is not None:
            self._subscription.unsubscribe()
            self._subscription = None
        renderer, self._renderer = self._renderer, None
        if renderer is not None:
            renderer.close()           # before the window it draws into goes away
        self._frame = None             # gives the decoder's frame back


class VulkanStreamWindow(QMainWindow):
    """A window around a VulkanVideoWidget. F shows or hides the frame rate and the time needed per frame in
    the title bar (getting the frame from the decoder plus recording and submitting its drawing, not counting
    the GPU's own work); A locks or unlocks the video's aspect ratio while the window is resized. With
    `keep_aspect_ratio` the window starts with it locked, so there are no bars."""

    closeRequested = pyqtSignal()

    def __init__(self, session: Session, show_stats: bool = False, keep_aspect_ratio: bool = False):
        super().__init__()
        self.setWindowTitle(TITLE)
        profile = session.stream_session.get_video_profile()
        self.video = VulkanVideoWidget(session.stream_session, session.frame_handler, show_stats)
        self.video.stream_size_changed.connect(self._set_video_size)
        self.video.stats_changed.connect(self._show_stats)
        self.setCentralWidget(self.video)
        self._aspect = profile.width / profile.height
        self._aspect_lock = AspectRatioLock(lambda: self._aspect) if keep_aspect_ratio else None
        self.resize(profile.width, profile.height)

    def start(self) -> None:
        """Start drawing; the window must be shown already."""
        self.video.start()

    def showEvent(self, event) -> None:
        super().showEvent(event)
        if self._aspect_lock is not None:
            self._aspect_lock.attach(int(self.winId()))   # the native window exists once shown

    def _show_stats(self, summary: str) -> None:
        self.setWindowTitle(f"{TITLE} - {summary.replace(chr(10), ' - ')}" if summary else TITLE)

    def _set_video_size(self, width: int, height: int) -> None:
        self._aspect = width / height
        if self._aspect_lock is not None and not (self.isMaximized() or self.isFullScreen()):
            self.resize(self.width(), round(self.width() / self._aspect))

    def keyPressEvent(self, event) -> None:
        if event.key() == Qt.Key.Key_F:
            self.video.set_stats_visible(not self.video.stats_visible)
        elif event.key() == Qt.Key.Key_A:
            if self._aspect_lock is None:
                self._aspect_lock = AspectRatioLock(lambda: self._aspect)
                self._aspect_lock.attach(int(self.winId()))
                self.resize(self.width(), round(self.width() / self._aspect))
            else:
                self._aspect_lock.remove()
                self._aspect_lock = None
        else:
            super().keyPressEvent(event)

    def closeEvent(self, event) -> None:
        if self._aspect_lock is not None:
            self._aspect_lock.remove()
        self.video.release()
        self.closeRequested.emit()
        super().closeEvent(event)
