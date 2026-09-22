"""CPU rendering for StreamDisplay, used when the session has a CpuFrameHandler.

Every frame is decoded and converted to RGB on the CPU into a numpy array, wrapped in a
QImage and drawn by a plain QWidget in a QMainWindow. No optional dependencies.
"""

import numpy as np
import numpy.typing as npt
from PyQt6.QtCore import Qt
from PyQt6.QtGui import QImage, QPainter, QPaintEvent
from PyQt6.QtWidgets import QWidget

from chiaki_py.gui.stream.views.base_view import VideoMixin
from ..frame_thread import FrameThread
from ..fps_thread import FpsThread


class CpuVideoWidget(VideoMixin[npt.NDArray[np.uint8]], QWidget):
    """Draws the latest frame scaled to fit the widget, letterboxed, with the frame rate and the time
    per frame in the top-right corner on request."""

    def __init__(self, frame_thread: FrameThread, fps_thread: FpsThread, show_stats: bool = False, parent=None):
        super().__init__(frame_thread, fps_thread, show_stats, parent)
        self._image: QImage | None = None
        self._data = b""

    def _frame_pulled(self) -> None:
        height, width, channels = self._frame.shape
        self._data = self._frame.tobytes()
        self._image = QImage(self._data, width, height, channels * width, QImage.Format.Format_RGB888)
        self.update()

    def _get_frame_size(self) -> tuple[int, int]:
        return (self._frame.shape[1], self._frame.shape[0])

    def paintEvent(self, a0: QPaintEvent | None) -> None:
        painter = QPainter(self)
        painter.fillRect(self.rect(), Qt.GlobalColor.black)
        if self._image is not None:
            scaled = self._image.scaled(self.size(), Qt.AspectRatioMode.KeepAspectRatio,
                                        Qt.TransformationMode.SmoothTransformation)
            painter.drawImage((self.width() - scaled.width()) // 2, (self.height() - scaled.height()) // 2, scaled)
