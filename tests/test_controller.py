import math
from types import SimpleNamespace

import pytest

from chiaki_py.controller import attach_controller, detach_controller
from chiaki_py.controller.dualsense import ControllerSubscriptions


class Recorder:
    """A stand-in for ChiakiPySession that records every method call as (name, args)."""

    def __init__(self):
        self.calls = []

    def __getattr__(self, name):
        def method(*args):
            self.calls.append((name, args))
        return method


class FakeDisposable:
    def __init__(self):
        self.disposed = False

    def dispose(self):
        self.disposed = True


class FakeController:
    """Records the callback given to each of DualSenseController's `<input>_<event>(callback)` methods."""

    def __init__(self):
        self.callbacks = {}
        self.disposables = {}
        self.opened = self.closed = False

    def open(self):
        self.opened = True

    def close(self):
        self.closed = True

    def __getattr__(self, name):
        def register(callback):
            self.callbacks[name] = callback
            self.disposables[name] = FakeDisposable()
            return self.disposables[name]
        return register


@pytest.fixture
def attached():
    controller, session = FakeController(), Recorder()
    subscriptions = attach_controller(controller, session)
    return controller, session, subscriptions


def test_opens_the_controller_and_subscribes_to_every_input(attached):
    controller, _, subscriptions = attached

    assert controller.opened
    assert set(subscriptions) == set(ControllerSubscriptions.__annotations__)
    assert set(controller.callbacks) == set(subscriptions)


@pytest.mark.parametrize("event, session_method", [
    ("cross_pressed", "press_cross"),
    ("circle_released", "release_circle"),
    ("dpad_up_pressed", "press_up"),
    ("share_pressed", "press_create"),
    ("touch_released", "release_touchpad"),
    ("ps_pressed", "press_ps"),
])
def test_buttons_map_to_session_inputs(attached, event, session_method):
    controller, session, _ = attached

    controller.callbacks[event]()

    assert session.calls == [(session_method, ())]


@pytest.mark.parametrize("value, expected", [(0.0, 0), (0.5, 127), (1.0, 255)])
def test_triggers_are_scaled_to_a_byte(attached, value, expected):
    controller, session, _ = attached

    controller.callbacks["l2_trigger_changed"](value)
    controller.callbacks["r2_trigger_changed"](value)

    assert session.calls == [("set_l2", (expected,)), ("set_r2", (expected,))]


def test_sticks_are_scaled(attached):
    controller, session, _ = attached

    controller.callbacks["left_joy_stick_changed"](SimpleNamespace(x=1.0, y=-0.5))
    controller.callbacks["right_joy_stick_changed"](SimpleNamespace(x=0.0, y=0.25))

    assert session.calls == [("set_left", (1023, -511)), ("set_right", (0, 255))]


def test_motion_sensors_are_passed_through(attached):
    controller, session, _ = attached

    controller.callbacks["accelerometer_changed"](SimpleNamespace(x=0.1, y=0.2, z=9.8))
    controller.callbacks["gyroscope_changed"](SimpleNamespace(x=1.0, y=2.0, z=3.0))

    assert session.calls == [("set_accelerometer", (0.1, 0.2, 9.8)), ("set_gyroscope", (1.0, 2.0, 3.0))]


@pytest.mark.parametrize("roll, pitch, yaw, expected_xyzw", [
    (0, 0, 0, (0, 0, 0, 1)),
    (90, 0, 0, (math.sqrt(0.5), 0, 0, math.sqrt(0.5))),
    (0, 90, 0, (0, math.sqrt(0.5), 0, math.sqrt(0.5))),
    (0, 0, 90, (0, 0, math.sqrt(0.5), math.sqrt(0.5))),
    (0, 0, 180, (0, 0, 1, 0)),
])
def test_orientation_becomes_a_quaternion(attached, roll, pitch, yaw, expected_xyzw):
    controller, session, _ = attached

    controller.callbacks["orientation_changed"](SimpleNamespace(roll=roll, pitch=pitch, yaw=yaw))

    (name, xyzw), = session.calls
    assert name == "set_orientation"
    assert xyzw == pytest.approx(expected_xyzw, abs=1e-9)


def test_detach_disposes_releases_everything_and_closes(attached):
    controller, session, subscriptions = attached

    detach_controller(controller, session, subscriptions)

    assert all(d.disposed for d in controller.disposables.values())
    names = [name for name, _ in session.calls]
    assert {"release_cross", "release_circle", "release_square", "release_triangle", "release_ps"} <= set(names)
    assert ("set_left", (0, 0)) in session.calls
    assert ("set_right", (0, 0)) in session.calls
    assert names[-1] == "send_feedback_state"
    assert controller.closed
