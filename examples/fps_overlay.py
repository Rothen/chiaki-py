"""A frame-rate counter and text drawn in the top-right corner of a frame, shared by the
examples. The GPU examples draw the text as an OpenGL overlay (see cuda_gl.py) and the
CPU ones straight onto the image (draw_text_top_right); both use the same look.

Needs: pip install opencv-python
"""

import time
from typing import Callable

import cv2
import numpy as np


class FpsCounter:
    """Frames per second, measured over consecutive windows of at least `interval` seconds."""

    def __init__(self, interval: float = 0.5, clock: Callable[[], float] = time.perf_counter):
        self.interval = interval
        self._clock = clock
        self._start: float | None = None
        self._frames = 0
        self.fps: float | None = None

    def tick(self) -> float | None:
        """Call once per frame shown. Returns the latest measurement, or None until one window has passed."""
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


OVERLAY_MARGIN = 12   # pixels between the text box and the corner of the frame, at a scale of 1

def rasterize_text(text: str, scale: float = 1.0) -> np.ndarray:
    """`text` as a (height, width, 4) RGBA image: white on a translucent dark box, so it reads on any picture."""
    font = cv2.FONT_HERSHEY_SIMPLEX
    font_scale = 0.8 * scale
    thickness = max(1, round(2 * scale))
    (text_width, text_height), baseline = cv2.getTextSize(text, font, font_scale, thickness)
    pad = round(6 * scale)
    image = np.zeros((text_height + baseline + 2 * pad, text_width + 2 * pad, 4), np.uint8)
    cv2.rectangle(image, (0, 0), (image.shape[1] - 1, image.shape[0] - 1), (0, 0, 0, 150), -1)
    cv2.putText(image, text, (pad, pad + text_height), font, font_scale, (255, 255, 255, 255), thickness, cv2.LINE_AA)
    return image


def draw_text_top_right(image: np.ndarray, text: str, scale: float = 1.0) -> None:
    """Draw `text` in the top-right corner of a (H, W, 3) uint8 image, in place (RGB or BGR alike)."""
    overlay = rasterize_text(text, scale)
    height, width = overlay.shape[:2]
    margin = round(OVERLAY_MARGIN * scale)
    x0, y0 = image.shape[1] - margin - width, margin
    if x0 < 0 or y0 + height > image.shape[0]:
        return                                            # the frame is too small to hold it
    alpha = overlay[..., 3:4].astype(np.float32) / 255.0
    region = image[y0:y0 + height, x0:x0 + width]
    region[:] = region * (1.0 - alpha) + overlay[..., :3] * alpha + 0.5
