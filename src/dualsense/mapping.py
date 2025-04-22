import math

from .states import (
    Accelerometer,
    Battery,
    Gyroscope,
    JoyStick,
    Orientation,
    TouchFinger,
    TriggerFeedback
)


def uint8_value_mapping(value: int, mappings: list[list[int]]) -> list[bool]:
    res = [False for _ in range(len(mappings))]

    for i, mapping in enumerate(mappings):
        res[i] = value in mapping

    return res


def uint8_bit_to_bool(value: int, bit: int) -> bool:
    return bool(value & (1 << bit))


def uint8_to_trigger_feedback_active(value: int) -> bool:
    return bool(value & 0x10)


def uint8_to_trigger_feedback_value(value: int) -> int:
    return value & 0xff


def uint8_to_touch_finger_active(value: int) -> bool:
    return not (value & 0x80)


def uint8_to_touch_finger_id(value: int) -> int:
    return value & 0x7F


def uint16_to_touch_finger_x(value_a: int, value_b: int) -> int:
    return ((value_b & 0x0F) << 8) | value_a


def uint16_to_signed_int_value(value_a: int, value_b: int) -> int:
    accel_value: int = ((value_b << 8) | value_a)
    if accel_value > 0x7FFF:
        accel_value -= 0x10000
    return accel_value


def uint16_to_touch_finger_y(value_a: int, value_b: int) -> int:
    return (value_b << 4) | ((value_a & 0xF0) >> 4)


def uint8_to_battery_level_percentage(value: int) -> float:
    batt_level_raw: int = value & 0x0f
    if batt_level_raw > 8:
        batt_level_raw = 8
    batt_level: float = batt_level_raw / 8
    return batt_level * 100


def uint8_to_battery_full(value: int) -> bool:
    return not not (value & 0x20)


def uint8_to_battery_charging(value: int) -> bool:
    return not not (value & 0x08)

def uint8_to_float(value: int, range: tuple[float, float] = (0, 255)) -> float:
    return round((value / 255) * (range[1] - range[0]) + range[0], 4)


def create_accelerometer(x_0: int, x_1: int, y_0: int, y_1: int, z_0: int, z_1: int) -> Accelerometer:
    return Accelerometer(
        x=uint16_to_signed_int_value(x_0, x_1),
        y=uint16_to_signed_int_value(y_0, y_1),
        z=uint16_to_signed_int_value(z_0, z_1)
    )


def create_gyroscope(x_0: int, x_1: int, y_0: int, y_1: int, z_0: int, z_1: int) -> Gyroscope:
    return Gyroscope(
        x=uint16_to_signed_int_value(x_0, x_1),
        y=uint16_to_signed_int_value(y_0, y_1),
        z=uint16_to_signed_int_value(z_0, z_1)
    )


def create_trigger_feedback(trigger_feedback: int) -> TriggerFeedback:
    return TriggerFeedback(
        active=uint8_to_trigger_feedback_active(trigger_feedback),
        value=uint8_to_trigger_feedback_value(trigger_feedback)
    )


def create_touch_finger(touch_0: int, touch_1: int, touch_2: int, touch_3: int) -> TouchFinger:
    return TouchFinger(
        active=uint8_to_touch_finger_active(touch_0),
        id=uint8_to_touch_finger_id(touch_0),
        x=uint16_to_touch_finger_x(touch_1, touch_2),
        y=uint16_to_touch_finger_y(touch_2, touch_3),
    )


def create_orientation(orientation: Orientation, accelerometer: Accelerometer, gyroscope: Gyroscope, dt: float) -> Orientation:
    alpha: float = 0.98
    roll_acc = math.atan2(accelerometer.y, accelerometer.z)
    pitch_acc = math.atan2(-accelerometer.x,
                           math.sqrt(accelerometer.y**2 + accelerometer.z**2))

    roll = alpha * (orientation.roll + gyroscope.x * dt) + \
        (1 - alpha) * roll_acc
    pitch = alpha * (orientation.pitch + gyroscope.y * dt) + \
        (1 - alpha) * pitch_acc
    yaw = orientation.yaw + gyroscope.z * dt  # yaw only from gyro
    return Orientation(
        pitch=pitch,
        roll=roll,
        yaw=yaw
    )


def create_battery(battery_0: int, battery_1: int) -> Battery:
    return Battery(
        level_percentage=uint8_to_battery_level_percentage(battery_0),
        full=uint8_to_battery_full(battery_0),
        charging=uint8_to_battery_charging(battery_1),
    )


def create_joy_stick(x: int, y: int) -> JoyStick:
    return JoyStick(
        x=uint8_to_float(x, (-1, 1)),
        y=uint8_to_float(y, (-1, 1))
    )
