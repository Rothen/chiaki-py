import pytest

from chiaki_py.gui.stream.aspect_ratio import (
    WMSZ_BOTTOM, WMSZ_BOTTOMLEFT, WMSZ_BOTTOMRIGHT, WMSZ_LEFT, WMSZ_RIGHT, WMSZ_TOP, WMSZ_TOPLEFT, WMSZ_TOPRIGHT,
    constrain_rect,
)

ASPECT = 16 / 9


def client_size(rect, extra_width=0, extra_height=0):
    left, top, right, bottom = rect
    return right - left - extra_width, bottom - top - extra_height


def test_dragging_a_side_edge_fixes_the_height():
    rect = constrain_rect(WMSZ_RIGHT, (0, 0, 1600, 500), 0, 0, ASPECT)

    assert rect == (0, 0, 1600, 900)


def test_dragging_the_left_edge_keeps_the_right_edge():
    rect = constrain_rect(WMSZ_LEFT, (100, 0, 1700, 500), 0, 0, ASPECT)

    assert rect == (100, 0, 1700, 900)


def test_dragging_the_bottom_edge_fixes_the_width():
    rect = constrain_rect(WMSZ_BOTTOM, (0, 0, 500, 900), 0, 0, ASPECT)

    assert rect == (0, 0, 1600, 900)


def test_dragging_the_top_edge_keeps_the_bottom_edge():
    rect = constrain_rect(WMSZ_TOP, (0, 100, 500, 1000), 0, 0, ASPECT)

    assert rect == (0, 100, 1600, 1000)


@pytest.mark.parametrize("edge", [WMSZ_TOPLEFT, WMSZ_TOPRIGHT, WMSZ_BOTTOMLEFT, WMSZ_BOTTOMRIGHT])
def test_corners_grow_the_dimension_that_is_too_small(edge):
    too_wide = constrain_rect(edge, (0, 0, 1600, 100), 0, 0, ASPECT)
    too_tall = constrain_rect(edge, (0, 0, 100, 900), 0, 0, ASPECT)

    assert client_size(too_wide) == (1600, 900)
    assert client_size(too_tall) == (1600, 900)


@pytest.mark.parametrize("edge, fixed_corner", [
    (WMSZ_TOPLEFT, "bottomright"),
    (WMSZ_TOPRIGHT, "bottomleft"),
    (WMSZ_BOTTOMLEFT, "topright"),
    (WMSZ_BOTTOMRIGHT, "topleft"),
])
def test_the_opposite_corner_stays_put(edge, fixed_corner):
    before = (100, 100, 1700, 300)
    left, top, right, bottom = constrain_rect(edge, before, 0, 0, ASPECT)

    corners = {"topleft": (left, top), "topright": (right, top), "bottomleft": (left, bottom), "bottomright": (right, bottom)}
    before_corners = {"topleft": (100, 100), "topright": (1700, 100), "bottomleft": (100, 300), "bottomright": (1700, 300)}
    assert corners[fixed_corner] == before_corners[fixed_corner]


def test_frame_size_is_left_out_of_the_ratio():
    rect = constrain_rect(WMSZ_RIGHT, (0, 0, 1616, 500), 16, 39, ASPECT)

    assert client_size(rect, 16, 39) == (1600, 900)


def test_degenerate_rect_is_left_alone():
    rect = (0, 0, 10, 10)

    assert constrain_rect(WMSZ_RIGHT, rect, 20, 20, ASPECT) == rect
