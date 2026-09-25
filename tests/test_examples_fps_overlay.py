"""The frame-rate counter and overlay shared by the examples (examples/fps_overlay.py)."""

import sys
from pathlib import Path

import numpy as np
import pytest

sys.path.insert(0, str(Path(__file__).parents[1] / "examples"))
from fps_overlay import OVERLAY_MARGIN, FpsCounter, draw_text_top_right, rasterize_text  # noqa: E402


class Clock:
    def __init__(self):
        self.now = 0.0

    def __call__(self):
        return self.now


class TestFpsCounter:
    def test_no_measurement_before_a_window_has_passed(self):
        clock = Clock()
        counter = FpsCounter(interval=0.5, clock=clock)

        assert counter.tick() is None
        clock.now = 0.1
        assert counter.tick() is None

    def test_measures_frames_per_second_over_the_window(self):
        clock = Clock()
        counter = FpsCounter(interval=0.5, clock=clock)
        counter.tick()                       # starts the clock at 0.0
        for i in range(1, 31):               # 30 frames over 0.5s
            clock.now = i / 60
            result = counter.tick()

        assert result == pytest.approx(60.0)

    def test_time_before_the_first_frame_is_not_counted(self):
        clock = Clock()
        clock.now = 10.0                     # e.g. waiting for the first frame
        counter = FpsCounter(interval=1.0, clock=clock)
        counter.tick()
        for i in range(1, 31):
            clock.now = 10.0 + i / 30
            result = counter.tick()

        assert result == pytest.approx(30.0)

    def test_keeps_the_last_measurement_during_the_next_window(self):
        clock = Clock()
        counter = FpsCounter(interval=1.0, clock=clock)
        counter.tick()
        clock.now = 1.0
        first = counter.tick()
        clock.now = 1.5

        assert counter.tick() == first == pytest.approx(1.0)


def test_rasterized_text_is_rgba():
    image = rasterize_text("60.0 FPS")

    assert image.ndim == 3 and image.shape[2] == 4
    assert image.dtype == np.uint8


GRAY = 200  # a background the translucent dark box visibly changes


def test_text_is_drawn_in_the_top_right_corner():
    image = np.full((200, 400, 3), GRAY, np.uint8)
    draw_text_top_right(image, "60.0 FPS")

    changed = (image != GRAY).any(axis=2)
    changed_rows, changed_cols = np.nonzero(changed)
    assert changed_cols.max() == 400 - OVERLAY_MARGIN - 1
    assert changed_rows.min() == OVERLAY_MARGIN
    assert not changed[150:, :].any()      # the lower part is untouched
    assert not changed[:, :150].any()      # and so is the left


def test_too_small_image_is_left_alone():
    image = np.full((10, 10, 3), GRAY, np.uint8)

    draw_text_top_right(image, "60.0 FPS")

    assert (image == GRAY).all()
