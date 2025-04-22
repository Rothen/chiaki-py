from .states import (
    Accelerometer,
    Battery,
    Gyroscope,
    Orientation,
    TouchFinger,
    TriggerFeedback
)


def bool_changed(before: bool, after: bool) -> bool:
    return before != after


def int_changed(before: int, after: int) -> bool:
    return before != after


def float_changed(before: float, after: float) -> bool:
    return before != after


def accelerometer_changed(before: Accelerometer, after: Accelerometer) -> bool:
    return (
        before.x != after.x or
        before.y != after.y or
        before.z != after.z
    )


def battery_changed(before: Battery, after: Battery) -> bool:
    return (
        before.level_percentage != after.level_percentage or
        before.full != after.full or
        before.charging != after.charging
    )
    

def gyroscope_changed(before: Gyroscope, after: Gyroscope) -> bool:
    return (
        before.x != after.x or
        before.y != after.y or
        before.z != after.z
    )


def orientation_changed(before: Orientation, after: Orientation) -> bool:
    return (
        before.pitch != after.pitch or
        before.roll != after.roll or
        before.yaw != after.yaw
    )
    

def touch_finger_changed(before: TouchFinger, after: TouchFinger) -> bool:
    return (
        before.active != after.active or
        before.id != after.id or
        before.x != after.x or
        before.y != after.y
    )


def trigger_feedback_changed(before: TriggerFeedback, after: TriggerFeedback) -> bool:
    return before.active != after.active or before.value != after.value
