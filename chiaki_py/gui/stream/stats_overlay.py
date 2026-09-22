"""The frame-rate box of the stream viewers whose video is drawn by something other than Qt, as a window on top of it."""

from PyQt6.QtCore import QEvent, QObject, QPoint, Qt
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
    It takes no clicks and no focus, and tracks that window's move/resize/show/hide by itself (an event filter
    installed on it), so its owner only has to call `set_text` and `set_visible`.
    """

    def __init__(self, video: QWidget):
        super().__init__(video.window())
        self._video = video
        self._text: str | None = None
        self.visible = False   # whether showing stats has been requested; actually shown once there is text and the video is on screen
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

        if (window := video.window()) is not None:
            window.installEventFilter(self)

    def set_text(self, text: str) -> None:
        """Show `text` (lines separated by newlines), if stats are visible and the video is on screen."""
        self._text = text
        self.setText(text)
        self.adjustSize()
        if self.visible:
            self.reposition()
            if self._video.isVisible():
                self.show()

    def set_visible(self, visible: bool) -> None:
        """Show or hide the overlay; showing it does nothing until there is text to show."""
        self.visible = visible
        if visible and self._text is not None:
            self.reposition()
            if self._video.isVisible():
                self.show()
        else:
            self.hide()

    def reposition(self) -> None:
        top_right = self._video.mapToGlobal(QPoint(self._video.width(), 0))
        self.move(top_right.x() - self.width() - MARGIN, top_right.y() + MARGIN)

    def eventFilter(self, a0: QObject | None, a1: QEvent | None) -> bool:
        """Keeps this overlay positioned over the video's window, and hidden/shown along with it."""
        if a1 is not None:
            kind = a1.type()
            if kind in (QEvent.Type.Move, QEvent.Type.Resize):
                self.reposition()
            elif kind == QEvent.Type.Hide:
                self.hide()
            elif kind == QEvent.Type.Show and self.visible and self._text is not None:
                self.reposition()
                self.show()
        return super().eventFilter(a0, a1)

    def release(self) -> None:
        """Stop tracking the video's window and free the overlay; must not be used afterwards."""
        if (window := self._video.window()) is not None:
            window.removeEventFilter(self)
        self.deleteLater()

    def paintEvent(self, a0: QPaintEvent | None) -> None:
        painter = QPainter(self)
        painter.fillRect(self.rect(), BACKGROUND)
        painter.end()
        super().paintEvent(a0)
