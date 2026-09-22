import logging
import time
import warnings
from typing import TYPE_CHECKING, TypeVar, Generic, cast
from abc import ABC, abstractmethod
import traceback

from OpenGL import GL
from OpenGL.GL.shaders import compileProgram, compileShader
from PyQt6.QtCore import Qt, pyqtSignal
from PyQt6.QtGui import (QSurfaceFormat, QKeyEvent, QShowEvent, QHideEvent, QCloseEvent,
                          QResizeEvent, QMoveEvent)
from PyQt6.QtOpenGLWidgets import QOpenGLWidget
from PyQt6.QtWidgets import QWidget, QLabel, QMainWindow

with warnings.catch_warnings():
    # cupy warns on Windows when there is no system CUDA Toolkit, though the pip wheels bring what it needs.
    warnings.filterwarnings(
        "ignore", message="CUDA path could not be detected")
    import cupy as cp

from chiaki_py.gui.stream.aspect_ratio import AspectRatioLock
from chiaki_py.gui.stream.stats_overlay import StatsOverlay
from ..threads.frame_thread import FrameThread
from ..threads.fps_thread import FpsThread

_logger = logging.getLogger(__name__)

class Renderer(ABC):
    ...


F = TypeVar("F")


if TYPE_CHECKING:
    _QWidgetBase = QWidget
else:
    _QWidgetBase = object


class VideoMixin(Generic[F], _QWidgetBase):
    """Shared behaviour for a video widget, mixed into a concrete widget class:
    `class ConcreteVideoWidget(VideoMixin[F], QOpenGLWidget)`. VideoMixin must come first in the
    bases list (see the runtime `_QWidgetBase` note above), and `super().__init__(parent)` below
    then cooperatively forwards to the concrete Qt widget's constructor.

    Frames arrive from a FrameProducer running on its own thread (its `new_frame` signal, connected
    in start()): it does the decoding/converting work, so the GUI thread only has to paint what it is
    handed. `handler` is otherwise unused here - concrete subclasses that need it directly (e.g. to
    build an initial placeholder frame) take it as a constructor argument of their own."""

    stream_size_changed = pyqtSignal(int, int)   # the frames turned out to be another size than expected

    def __init__(self, frame_thread: FrameThread[F], fps_thread: FpsThread, parent=None):
        super().__init__(parent)

        self._frame_thread = frame_thread
        self._fps_thread = fps_thread
        self._frame: F = frame_thread.frame_init
        self._size: tuple[int, int] = frame_thread.size

    @abstractmethod
    def _render(self) -> None:
        ...

    @abstractmethod
    def _get_frame_size(self) -> tuple[int, int]:
        ...  # (frame.width, frame.height)

    def _on_frame(self, frame: F) -> None:
        self._fps_thread.tick_total()
        """Slot for FrameProducer.new_frame: `frame` replaces the one currently shown. `get_time` is how
        long the producer took to get/convert it (seconds); added to how long painting it takes here for
        the combined per-frame time shown in the stats overlay."""
        self._frame = frame
        try:
            self._fps_thread.tick_render()
            self._render()
            self._fps_thread.tock_render()
        except (ValueError, RuntimeError):
            _logger.warning("Dropping unusable frame", exc_info=True)
            return

        size = self._get_frame_size()
        if size != self._size:
            self._size = size
            self.stream_size_changed.emit(*size)   # a PS4 asked for 1080p downgrades to 720p once connected
        self._fps_thread.tock_total()
    
    def start(self) -> None:
        """Start drawing the session's frames. The widget must be shown already, for its native window to be there;
        raises RuntimeError if the session can't be drawn (it does not use the Vulkan decoder, ...)."""
        self._frame_thread.new_frame.connect(self._on_frame)

    def release(self) -> None:
        """Stop listening for frames and free the GPU resources; the widget must not be used afterwards."""
        try:
            self._frame_thread.new_frame.disconnect(self._on_frame)
        except TypeError:
            pass
        del self._frame


T = TypeVar("T", bound=VideoMixin)


class BaseView(QMainWindow, Generic[T]):
    """A window around a GpuVideoWidget. F shows or hides the frame rate and the time needed per frame
    (getting the frame from the decoder and converting it, plus painting it), drawn by a StatsOverlay
    kept over the video's top-right corner. With `keep_aspect_ratio` the window keeps the video's aspect
    ratio while it is resized, so there are no bars."""

    closeRequested = pyqtSignal()

    def __init__(self, video_mixin: T, frame_producer, fps_thread: FpsThread, show_stats: bool = False,
                 keep_aspect_ratio: bool = False):
        super().__init__()
        self.setWindowTitle("Live Image Stream")
        self.video: T = video_mixin
        self.video.stream_size_changed.connect(self._set_video_size)
        self.setCentralWidget(self.video)
        self._aspect = frame_producer.width / frame_producer.height
        self._aspect_lock = AspectRatioLock(lambda: self._aspect) if keep_aspect_ratio else None
        self.resize(frame_producer.width, frame_producer.height)

        self._fps_thread = fps_thread
        self.stats_visible = show_stats
        self._overlay_text: str | None = None
        self._stats_box = StatsOverlay(self.video)

    def set_stats_visible(self, visible: bool) -> None:
        self.stats_visible = visible
        if visible and self._overlay_text is not None:
            self._stats_box.set_text(self._overlay_text)
        else:
            self._stats_box.hide()

    def _on_fps(self, fps: float, pull_time: float, render_time: float, total_time: float) -> None:
        self._overlay_text = (f"FPS: {fps:.2f}\nPull Time: {pull_time:.2f} ms\n"
                               f"Render Time: {render_time:.2f} ms\nTotal Time: {total_time:.2f} ms")
        if self.stats_visible:
            self._stats_box.set_text(self._overlay_text)

    def showEvent(self, a0: QShowEvent | None) -> None:
        super().showEvent(a0)
        if self._aspect_lock is not None:
            self._aspect_lock.attach(int(self.winId()))
        if self.stats_visible and self._overlay_text is not None:
            self._stats_box.reposition()
            self._stats_box.show()

    def hideEvent(self, a0: QHideEvent | None) -> None:
        super().hideEvent(a0)
        self._stats_box.hide()

    def resizeEvent(self, a0: QResizeEvent | None) -> None:
        super().resizeEvent(a0)
        self._stats_box.reposition()

    def moveEvent(self, a0: QMoveEvent | None) -> None:
        super().moveEvent(a0)
        self._stats_box.reposition()

    def _set_video_size(self, width: int, height: int) -> None:
        self._aspect = width / height
        if self._aspect_lock is not None and not (self.isMaximized() or self.isFullScreen()):
            self.resize(self.width(), round(self.width() / self._aspect))

    def keyPressEvent(self, a0: QKeyEvent | None) -> None:
        if a0 is None:
            super().keyPressEvent(a0)
            return

        if a0.key() == Qt.Key.Key_F:
            self.set_stats_visible(not self.stats_visible)
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
        self._fps_thread.new_fps.connect(self._on_fps)
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
        try:
            self._fps_thread.new_fps.disconnect(self._on_fps)
        except TypeError:
            pass
        self._stats_box.deleteLater()
        self.video.release()
        self.closeRequested.emit()
        super().closeEvent(a0)
