import logging
import time
import warnings
from typing import TYPE_CHECKING, TypeVar, Generic, cast
from abc import ABC, abstractmethod
import traceback

from OpenGL import GL
from OpenGL.GL.shaders import compileProgram, compileShader
from PyQt6.QtCore import Qt, QEvent, QObject, pyqtSignal
from PyQt6.QtGui import QSurfaceFormat, QKeyEvent, QShowEvent, QCloseEvent, QResizeEvent
from PyQt6.QtOpenGLWidgets import QOpenGLWidget
from PyQt6.QtWidgets import QWidget, QLabel, QMainWindow

with warnings.catch_warnings():
    # cupy warns on Windows when there is no system CUDA Toolkit, though the pip wheels bring what it needs.
    warnings.filterwarnings(
        "ignore", message="CUDA path could not be detected")
    import cupy as cp

from chiaki_py import Session
from chiaki_py.gui.stream.aspect_ratio import AspectRatioLock
from chiaki_py.gui.stream.frame_stats import FrameStats
from chiaki_py.lib import StreamSession
from chiaki_py.gui.stream.stats_overlay import StatsOverlay

_logger = logging.getLogger(__name__)

class Renderer(ABC):
    ...


F = TypeVar("F")


if TYPE_CHECKING:
    _QWidgetBase = QWidget
else:
    # No real Qt/sip ancestry at runtime: PyQt/sip requires the *first* base among several wrapped
    # C++ classes to be the concrete type actually constructed (e.g. QOpenGLWidget), which VideoMixin
    # would otherwise contend with as a second, different QWidget-derived class. Staying a plain
    # Python class sidesteps that conflict entirely, so concrete subclasses are free to list VideoMixin
    # first - which they must, for its resizeEvent/eventFilter overrides and cooperative __init__ to
    # take priority over the concrete Qt widget's own (see e.g. CudaVideoWidget).
    _QWidgetBase = object


class VideoMixin(Generic[F], _QWidgetBase):
    """Shared behaviour for a video widget, mixed into a concrete widget class:
    `class ConcreteVideoWidget(VideoMixin[F], QOpenGLWidget)`. VideoMixin must come first in the
    bases list (see the runtime `_QWidgetBase` note above), and `super().__init__(parent)` below
    then cooperatively forwards to the concrete Qt widget's constructor."""

    _frame_available = pyqtSignal()   # emitted from the decoder's thread, delivered on the GUI thread
    stream_size_changed = pyqtSignal(int, int)   # the frames turned out to be another size than expected
    stats_changed = pyqtSignal(str)

    def __init__(self, stream_session: StreamSession, handler, width: int, height: int, frame_init: F, show_stats: bool = False, parent=None):
        super().__init__(parent)

        self._stream_session = stream_session
        self._handler = handler
        self._subscription = None
        self._frame: F = frame_init
        self._size: tuple[int, int] = (width, height)
        self._warned = False

        self._stats = FrameStats(interval=1.0)   # the stats box shows a new measurement once a second
        self.stats_visible = show_stats
        self._overlay_text: str | None = None
        self._stats_box: StatsOverlay = StatsOverlay(self)

    def set_stats_visible(self, visible: bool) -> None:
        self.stats_visible = visible
        if visible:
            self._show_stats(self._stats.summary())

    def _show_stats(self, summary: str) -> None:
        """Show `summary` over the video, or nothing if it is None."""
        if summary == self._overlay_text:
            return
        else:
            self._stats_box.set_text(summary)
        self._overlay_text = summary

    @abstractmethod
    def _frame_pulled(self) -> None:
        ...

    @abstractmethod
    def _get_frame_size(self) -> tuple[int, int]:
        ...  # (frame.width, frame.height)

    def _adopt_stream_size(self) -> None:
        """Called when `self._frame` no longer fits what the handler decodes, because the stream's
        actual size changed. Subclasses that pre-allocate a fixed-size `out` buffer should override
        this to reallocate it to match and emit `stream_size_changed`; by default the frame is just
        dropped and the next one tried again."""
        if not self._warned:
            self._warned = True
            _logger.warning("Dropping unusable frames")

    def resizeEvent(self, a0: QResizeEvent | None) -> None:  # pyright: ignore[reportIncompatibleMethodOverride]
        super().resizeEvent(a0)
        self._stats_box.reposition()

    def eventFilter(self, a0: QObject | None, a1: QEvent | None) -> bool:
        """Keeps the stats box with the window this widget is in, which is what it is filtering."""
        if a1 is not None:
            kind = a1.type()
            if kind in (QEvent.Type.Move, QEvent.Type.Resize):
                self._stats_box.reposition()
            elif kind == QEvent.Type.Hide:
                self._stats_box.hide()
            elif kind == QEvent.Type.Show and self._overlay_text is not None:
                self._stats_box.reposition()
                self._stats_box.show()
        return super().eventFilter(a0, a1)
            
    def _pull(self) -> None:
        """if self._released:
            return"""
        try:
            started = time.perf_counter()
            if self._handler.get_frame(self._frame) is None:
                return
            self._frame_pulled()
        except ValueError:
            self._adopt_stream_size()
            return
        except RuntimeError:
            if not self._warned:
                self._warned = True
                _logger.warning("Dropping unusable frames", exc_info=True)
            return
        done = time.perf_counter()

        size = self._get_frame_size()
        if size != self._size:
            first = self._size is None
            self._size = size
            if not first:
                self.stream_size_changed.emit(*size)   # a PS4 asked for 1080p downgrades to 720p once connected

        self._stats.add(done - started)
        self._stats.tick()
        if self.stats_visible:
            self._show_stats(self._stats.summary())
    
    def start(self) -> None:
        """Start drawing the session's frames. The widget must be shown already, for its native window to be there;
        raises RuntimeError if the session can't be drawn (it does not use the Vulkan decoder, ...)."""
        if (window := self.window()) is not None:
            window.installEventFilter(self)
        self._frame_available.connect(self._pull)
        self._subscription = self._stream_session.on_frame_available().subscribe(lambda _: self._frame_available.emit())

    def release(self) -> None:
        """Stop listening for frames and free the GPU resources; the widget must not be used afterwards."""
        if self._subscription is not None:
            self._subscription.unsubscribe()
            self._subscription = None
        if self._stats_box:
            if (window := self.window()) is not None:
                window.removeEventFilter(self)
            self._stats_box.deleteLater()
        del self._frame # gives the decoder's frame back



T = TypeVar("T", bound=VideoMixin)


class BaseView(QMainWindow, Generic[T]):
    """A window around a GpuVideoWidget. F shows or hides the frame rate and the time needed per frame
    (getting the frame from the decoder and converting it, plus painting it). With `keep_aspect_ratio`
    the window keeps the video's aspect ratio while it is resized, so there are no bars."""

    closeRequested = pyqtSignal()

    def __init__(self, session: Session, widget_cls: type[T], show_stats: bool = False, keep_aspect_ratio: bool = False):
        super().__init__()
        self.setWindowTitle("Live Image Stream")
        profile = session.stream_session.get_video_profile()
        self.video: T = widget_cls(session.stream_session, session.frame_handler, profile.width, profile.height, show_stats)
        self.video.stream_size_changed.connect(self._set_video_size)
        self.setCentralWidget(self.video)
        self._aspect = profile.width / profile.height
        self._aspect_lock = AspectRatioLock(
            lambda: self._aspect) if keep_aspect_ratio else None
        self.resize(profile.width, profile.height)

    def showEvent(self, a0: QShowEvent | None) -> None:
        super().showEvent(a0)
        if self._aspect_lock is not None:
            self._aspect_lock.attach(int(self.winId()))

    def _set_video_size(self, width: int, height: int) -> None:
        self._aspect = width / height
        if self._aspect_lock is not None and not (self.isMaximized() or self.isFullScreen()):
            self.resize(self.width(), round(self.width() / self._aspect))

    def keyPressEvent(self, a0: QKeyEvent | None) -> None:
        if a0 is None:
            super().keyPressEvent(a0)
            return

        if a0.key() == Qt.Key.Key_F:
            self.video.set_stats_visible(not self.video.stats_visible)
        elif a0.key() == Qt.Key.Key_A:
            if self._aspect_lock is None:
                self._aspect_lock = AspectRatioLock(lambda: self._aspect)
                self._aspect_lock.attach(int(self.winId()))
                self.resize(self.width(), round(self.width() / self._aspect))
            else:
                self._aspect_lock.remove()
                self._aspect_lock = None
        else:
            super().keyPressEvent(a0)

    def start(self) -> None:
        """Start drawing; the window must be shown already."""
        self.video.start()
    
    def show(self) -> None:
        super().show()
        try:
            self.start()  # needs the native window that showing it made
        except Exception as e:
            self.video.release()
            self.hide()
            raise

    def closeEvent(self, a0: QCloseEvent | None) -> None:
        if self._aspect_lock is not None:
            self._aspect_lock.remove()
        self.video.release()
        self.closeRequested.emit()
        super().closeEvent(a0)
