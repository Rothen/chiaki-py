import time
from typing import Callable


class FrameStats:
    """The frame rate, and the time needed per frame, measured over consecutive windows of at least `interval` seconds.

    The time needed per frame is how long it takes to get a frame plus how long it takes to paint it: report
    each with add_get() and add_paint() as frames are handled, and call tick() once per frame shown.
    """

    def __init__(self, interval: float = 0.5, clock: Callable[[], float] = time.perf_counter):
        self.interval = interval
        self._clock = clock
        self._start: float | None = None
        self._frames = 0
        self._get_total = self._paint_total = 0.0
        self._get_count = self._paint_count = 0
        self.fps: float | None = None
        self.get_ms: float | None = None
        self.paint_ms: float | None = None

    @property
    def frame_time_ms(self) -> float | None:
        """Time to get plus time to paint a frame, in milliseconds (None until both have been measured)."""
        if self.get_ms is None or self.paint_ms is None:
            return None
        return self.get_ms + self.paint_ms

    def add_get(self, seconds: float) -> None:
        """Record how long getting a frame took."""
        self._get_total += seconds
        self._get_count += 1

    def add_paint(self, seconds: float) -> None:
        """Record how long painting a frame took."""
        self._paint_total += seconds
        self._paint_count += 1

    def clear_work(self) -> None:
        """Forget the times measured so far, e.g. because how they are measured has changed."""
        self._get_total = self._paint_total = 0.0
        self._get_count = self._paint_count = 0
        self.get_ms = self.paint_ms = None

    def tick(self) -> float | None:
        """Call once per frame shown. Returns the latest frame rate, or None until one window has passed."""
        now = self._clock()
        if self._start is None:
            self._start = now      # the first frame only starts the clock, so time spent waiting for it isn't counted
            return self.fps
        self._frames += 1
        elapsed = now - self._start
        if elapsed >= self.interval:
            self.fps = self._frames / elapsed
            if self._get_count:
                self.get_ms = self._get_total / self._get_count * 1000.0
            if self._paint_count:
                self.paint_ms = self._paint_total / self._paint_count * 1000.0
            self._get_total = self._paint_total = 0.0
            self._get_count = self._paint_count = 0
            self._start, self._frames = now, 0
        return self.fps

    def summary(self) -> str | None:
        """The latest measurement as two lines of text, e.g. "59.9 FPS" and "2.0 ms/frame (get 1.5 + paint 0.5)"
        (None until there is one)."""
        frame_time = self.frame_time_ms
        if self.fps is None or frame_time is None or self.get_ms is None or self.paint_ms is None:
            return None
        return f"{self.fps:.1f} FPS\n{frame_time:.1f} ms/frame (get {self.get_ms:.1f} + paint {self.paint_ms:.1f})"
