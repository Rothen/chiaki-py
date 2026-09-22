"""The frame-rate box of the stream viewers whose video is drawn by something other than Qt, as a window on top of it."""

from PyQt6.QtCore import QPoint, Qt
from PyQt6.QtGui import QColor, QFont, QPainter, QPaintEvent
from PyQt6.QtWidgets import QLabel, QWidget

# The same look as the box in stream_display.qml: white bold text right-aligned on black at 150/255 opacity
FONT_PIXEL_SIZE = 16
PADDING_X = 8
PADDING_Y = 6
MARGIN = 12
BACKGROUND = QColor(0, 0, 0, 150)


class StatsOverlay(QLabel):
    """A frameless, translucent window that shows text in the top-right corner of `video`.

    A child widget can't be put on top of a native window that something else draws into, so this is a window
    of its own, owned by the video's window (which keeps it above that window and minimizes it along with it).
    It takes no clicks and no focus. The video's widget has to call `reposition` whenever the video moves or
    changes size, and hide or show it when its window is hidden or shown.
    """

    def __init__(self, video: QWidget):
        super().__init__(video.window())
        self._video = video
        self.setWindowFlags(Qt.WindowType.Tool | Qt.WindowType.FramelessWindowHint
                            | Qt.WindowType.WindowTransparentForInput | Qt.WindowType.WindowDoesNotAcceptFocus)
        self.setAttribute(Qt.WidgetAttribute.WA_TranslucentBackground)
        self.setAttribute(Qt.WidgetAttribute.WA_ShowWithoutActivating)
        self.setAlignment(Qt.AlignmentFlag.AlignRight | Qt.AlignmentFlag.AlignVCenter)
        self.setContentsMargins(PADDING_X, PADDING_Y, PADDING_X, PADDING_Y)
        font = QFont()
        font.setPixelSize(FONT_PIXEL_SIZE)
        font.setBold(True)
        self.setFont(font)
        self.setStyleSheet("color: white;")

    def set_text(self, text: str) -> None:
        """Show `text` (lines separated by newlines), if the video is on screen."""
        self.setText(text)
        self.adjustSize()
        self.reposition()
        if self._video.isVisible():
            self.show()

    def reposition(self) -> None:
        top_right = self._video.mapToGlobal(QPoint(self._video.width(), 0))
        self.move(top_right.x() - self.width() - MARGIN, top_right.y() + MARGIN)

    def paintEvent(self, a0: QPaintEvent | None) -> None:
        painter = QPainter(self)
        painter.fillRect(self.rect(), BACKGROUND)
        painter.end()
        super().paintEvent(a0)
