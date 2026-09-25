from __future__ import annotations
from typing import TypedDict, cast
import math

from reactivex.abc import DisposableBase
from dualsense_py.dual_sense_controller import DualSenseController
from dualsense_py.states import Accelerometer, Gyroscope, JoyStick, Orientation

from ..lib import ChiakiPySession


def _left_stick_change(joy_stick: JoyStick, stream_session: ChiakiPySession) -> None:
    stream_session.set_left(int(joy_stick.x * 1023), int(joy_stick.y * 1023))


def _right_stick_change(joy_stick: JoyStick, stream_session: ChiakiPySession) -> None:
    stream_session.set_right(int(joy_stick.x * 1023), int(joy_stick.y * 1023))


def _accelerometer_change(accelerometer: Accelerometer, stream_session: ChiakiPySession) -> None:
    stream_session.set_accelerometer(accelerometer.x, accelerometer.y, accelerometer.z)


def _gyroscope_change(gyroscope: Gyroscope, stream_session: ChiakiPySession) -> None:
    stream_session.set_gyroscope(gyroscope.x, gyroscope.y, gyroscope.z)


def _orientation_change(orientation: Orientation, stream_session: ChiakiPySession) -> None:
    cy = math.cos(math.radians(orientation.yaw) * 0.5)
    sy = math.sin(math.radians(orientation.yaw) * 0.5)
    cp = math.cos(math.radians(orientation.pitch) * 0.5)
    sp = math.sin(math.radians(orientation.pitch) * 0.5)
    cr = math.cos(math.radians(orientation.roll) * 0.5)
    sr = math.sin(math.radians(orientation.roll) * 0.5)

    w = cr * cp * cy + sr * sp * sy
    x = sr * cp * cy - cr * sp * sy
    y = cr * sp * cy + sr * cp * sy
    z = cr * cp * sy - sr * sp * cy

    stream_session.set_orientation(x, y, z, w)


class ControllerSubscriptions(TypedDict):
    cross_pressed: DisposableBase
    cross_released: DisposableBase
    circle_pressed: DisposableBase
    circle_released: DisposableBase
    square_pressed: DisposableBase
    square_released: DisposableBase
    triangle_pressed: DisposableBase
    triangle_released: DisposableBase
    dpad_left_pressed: DisposableBase
    dpad_left_released: DisposableBase
    dpad_right_pressed: DisposableBase
    dpad_right_released: DisposableBase
    dpad_up_pressed: DisposableBase
    dpad_up_released: DisposableBase
    dpad_down_pressed: DisposableBase
    dpad_down_released: DisposableBase
    l1_pressed: DisposableBase
    l1_released: DisposableBase
    r1_pressed: DisposableBase
    r1_released: DisposableBase
    l3_pressed: DisposableBase
    l3_released: DisposableBase
    r3_pressed: DisposableBase
    r3_released: DisposableBase
    options_pressed: DisposableBase
    options_released: DisposableBase
    share_pressed: DisposableBase
    share_released: DisposableBase
    touch_pressed: DisposableBase
    touch_released: DisposableBase
    ps_pressed: DisposableBase
    ps_released: DisposableBase
    l2_trigger_changed: DisposableBase
    r2_trigger_changed: DisposableBase
    left_joy_stick_changed: DisposableBase
    right_joy_stick_changed: DisposableBase
    accelerometer_changed: DisposableBase
    gyroscope_changed: DisposableBase
    orientation_changed: DisposableBase


def attach_controller(controller: DualSenseController, stream_session: ChiakiPySession) -> ControllerSubscriptions:
    """Wire a DualSense controller's inputs to a ChiakiPySession's inputs."""
    controller.open()

    return {
        "cross_pressed": controller.cross_pressed(stream_session.press_cross),
        "cross_released": controller.cross_released(stream_session.release_cross),
        "circle_pressed": controller.circle_pressed(stream_session.press_circle),
        "circle_released": controller.circle_released(stream_session.release_circle),
        "square_pressed": controller.square_pressed(stream_session.press_square),
        "square_released": controller.square_released(stream_session.release_square),
        "triangle_pressed": controller.triangle_pressed(stream_session.press_triangle),
        "triangle_released": controller.triangle_released(stream_session.release_triangle),
        "dpad_left_pressed": controller.dpad_left_pressed(stream_session.press_left),
        "dpad_left_released": controller.dpad_left_released(stream_session.release_left),
        "dpad_right_pressed": controller.dpad_right_pressed(stream_session.press_right),
        "dpad_right_released": controller.dpad_right_released(stream_session.release_right),
        "dpad_up_pressed": controller.dpad_up_pressed(stream_session.press_up),
        "dpad_up_released": controller.dpad_up_released(stream_session.release_up),
        "dpad_down_pressed": controller.dpad_down_pressed(stream_session.press_down),
        "dpad_down_released": controller.dpad_down_released(stream_session.release_down),
        "l1_pressed": controller.l1_pressed(stream_session.press_l1),
        "l1_released": controller.l1_released(stream_session.release_l1),
        "r1_pressed": controller.r1_pressed(stream_session.press_r1),
        "r1_released": controller.r1_released(stream_session.release_r1),
        "l3_pressed": controller.l3_pressed(stream_session.press_l3),
        "l3_released": controller.l3_released(stream_session.release_l3),
        "r3_pressed": controller.r3_pressed(stream_session.press_r3),
        "r3_released": controller.r3_released(stream_session.release_r3),
        "options_pressed": controller.options_pressed(stream_session.press_options),
        "options_released": controller.options_released(stream_session.release_options),
        "share_pressed": controller.share_pressed(stream_session.press_create),
        "share_released": controller.share_released(stream_session.release_create),
        "touch_pressed": controller.touch_pressed(stream_session.press_touchpad),
        "touch_released": controller.touch_released(stream_session.release_touchpad),
        "ps_pressed": controller.ps_pressed(stream_session.press_ps),
        "ps_released": controller.ps_released(stream_session.release_ps),
        "l2_trigger_changed": controller.l2_trigger_changed(lambda value: stream_session.set_l2(int(value * 255))),
        "r2_trigger_changed": controller.r2_trigger_changed(lambda value: stream_session.set_r2(int(value * 255))),
        "left_joy_stick_changed": controller.left_joy_stick_changed(lambda joy_stick: _left_stick_change(joy_stick, stream_session)),
        "right_joy_stick_changed": controller.right_joy_stick_changed(lambda joy_stick: _right_stick_change(joy_stick, stream_session)),
        "accelerometer_changed": controller.accelerometer_changed(lambda accelerometer: _accelerometer_change(accelerometer, stream_session)),
        "gyroscope_changed": controller.gyroscope_changed(lambda gyroscope: _gyroscope_change(gyroscope, stream_session)),
        "orientation_changed": controller.orientation_changed(lambda orientation: _orientation_change(orientation, stream_session))
    }


def detach_controller(controller: DualSenseController, stream_session: ChiakiPySession, subscriptions: ControllerSubscriptions) -> None:
    """Wire a DualSense controller's inputs to a ChiakiPySession's inputs."""
    
    for subscription in subscriptions.values():
        cast(DisposableBase, subscription).dispose()

    stream_session.release_circle()
    stream_session.release_create()
    stream_session.release_cross()
    stream_session.release_down()
    stream_session.release_l1()
    stream_session.release_l3()
    stream_session.release_left()
    stream_session.release_options()
    stream_session.release_ps()
    stream_session.release_r1()
    stream_session.release_r3()
    stream_session.release_right()
    stream_session.release_square()
    stream_session.release_touchpad()
    stream_session.release_triangle()
    stream_session.release_up()
    stream_session.set_left(0, 0)
    stream_session.set_right(0, 0)
    stream_session.send_feedback_state()
    
    controller.close()
