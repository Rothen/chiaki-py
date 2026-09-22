import time
from typing import Callable


class FrameStats:
    """The frame rate, and the time needed per frame, measured over consecutive windows of at least `interval` seconds.

    The time needed per frame is how long it takes to get a frame plus how long it takes to paint it: report
    each with add() as frames are handled, and call tick() once per frame shown.
    """

    def __init__(self, interval: float = 0.5, clock: Callable[[], float] = time.perf_counter):
        self.interval = interval
        self._clock = clock
        self._start: float | None = None
        self._frames = 0
        self._total = 0.0
        self._count = 0
        self.fps: float | None = None
        self.ms: float | None = None

    def add(self, seconds: float) -> None:
        """Record how long painting a frame took."""
        self._total += seconds
        self._count += 1

    def clear_work(self) -> None:
        """Forget the times measured so far, e.g. because how they are measured has changed."""
        self._total = 0.0
        self._count = 0
        self.ms = None

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
            if self._count:
                self.ms = self._total / self._count * 1000.0
            self._total = 0.0
            self._count = 0
            self._start, self._frames = now, 0
        return self.fps

    def summary(self) -> str:
        """The latest measurement as two lines of text, e.g. "59.9 FPS" and "2.0 ms/frame (get 1.5 + paint 0.5)"
        (None until there is one)."""
        if self.fps is None or self.ms is None:
            return ""
        return f"{self.fps:.1f} FPS\n{self.ms:.1f} ms/frame"
