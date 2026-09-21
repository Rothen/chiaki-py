import time
from typing import Callable


class FpsCounter:
    """Frames per second and time per frame, measured over consecutive windows of at least `interval` seconds."""

    def __init__(self, interval: float = 0.5, clock: Callable[[], float] = time.perf_counter):
        self.interval = interval
        self._clock = clock
        self._start: float | None = None
        self._frames = 0
        self.fps: float | None = None

    @property
    def frame_time_ms(self) -> float | None:
        """The average time between two frames, in milliseconds (None until the first measurement)."""
        return None if not self.fps else 1000.0 / self.fps

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
            self._start, self._frames = now, 0
        return self.fps

    def summary(self) -> str | None:
        """The latest measurement as two lines of text, e.g. "59.9 FPS" and "16.7 ms/frame" (None until there is one)."""
        if self.fps is None or self.frame_time_ms is None:
            return None
        return f"{self.fps:.1f} FPS\n{self.frame_time_ms:.1f} ms/frame"
