from __future__ import annotations
import time
from abc import abstractmethod
from typing import Generic, TypeVar, Final


from ..readable_value import ReadableValue, ButtonValue
from ..states import (
    Accelerometer,
    Battery,
    Gyroscope,
    JoyStick,
    Orientation,
    TouchFinger,
    TriggerFeedback
)
ID = TypeVar("ID")
T = TypeVar("T")


class DeviceInfo(Generic[ID, T]):
    """
    Class to hold device information.
    """
    
    def __init__(self, id: ID, orig_device_info: T):
        self.id: ID = id
        self.orig_device_info: T = orig_device_info

    @abstractmethod
    def open(self):
        """
        Open a connection to the device at the specified path.
        """

    @abstractmethod
    def close(self):
        """
        Close the connection to the device.
        """

    @abstractmethod
    def _read(self):
        """
        Read data from the device.
        """

    @abstractmethod
    def write(self):
        """
        Write data to the device.
        """

    @abstractmethod
    def set_led(self, r: int, g: int, b: int) -> None:
        """
        Set the LED color of the device.
        """

    __slots__ = (
        "_orig_device_info",
        "_read_time",
        "_last_read_time",
        "_square",
        "_cross",
        "_circle",
        "_triangle",
        "_dpad_up",
        "_dpad_right",
        "_dpad_down",
        "_dpad_left",
        "_l1",
        "_r1",
        "_l2",
        "_r2",
        "_share",
        "_options",
        "_l3",
        "_r3",
        "_ps",
        "_touch",
        "_mikrophone",
        "_left_joy_stick",
        "_right_joy_stick",
        "_l2_trigger",
        "_r2_trigger",
        "_accelerometer",
        "_gyroscope",
        "_battery",
        "_touch_finger_1",
        "_touch_finger_2",
        "_left_trigger_feedback",
        "_right_trigger_feedback",
        "_orientation",
    )

    def __init__(self, orig_device_info: T):
        self._orig_device_info: Final[T] = orig_device_info
        self._read_time: float = 0.0
        self._last_read_time: float = 0.0

        self._square: Final[ButtonValue] = ButtonValue()
        self._cross: Final[ButtonValue] = ButtonValue()
        self._circle: Final[ButtonValue] = ButtonValue()
        self._triangle: Final[ButtonValue] = ButtonValue()
        self._dpad_up: Final[ButtonValue] = ButtonValue()
        self._dpad_right: Final[ButtonValue] = ButtonValue()
        self._dpad_down: Final[ButtonValue] = ButtonValue()
        self._dpad_left: Final[ButtonValue] = ButtonValue()
        self._l1: Final[ButtonValue] = ButtonValue()
        self._r1: Final[ButtonValue] = ButtonValue()
        self._l2: Final[ButtonValue] = ButtonValue()
        self._r2: Final[ButtonValue] = ButtonValue()
        self._share: Final[ButtonValue] = ButtonValue()
        self._options: Final[ButtonValue] = ButtonValue()
        self._l3: Final[ButtonValue] = ButtonValue()
        self._r3: Final[ButtonValue] = ButtonValue()
        self._ps: Final[ButtonValue] = ButtonValue()
        self._touch: Final[ButtonValue] = ButtonValue()
        self._mikrophone: Final[ButtonValue] = ButtonValue()
        self._left_joy_stick: Final[ReadableValue[JoyStick]] = ReadableValue[JoyStick](JoyStick())
        self._right_joy_stick: Final[ReadableValue[JoyStick]] = ReadableValue[JoyStick](JoyStick())
        self._l2_trigger: Final[ReadableValue[float]] = ReadableValue[float](0.0)
        self._r2_trigger: Final[ReadableValue[float]] = ReadableValue[float](0.0)
        self._accelerometer: Final[ReadableValue[Accelerometer]] = ReadableValue[Accelerometer](Accelerometer())
        self._gyroscope: Final[ReadableValue[Gyroscope]] = ReadableValue[Gyroscope](Gyroscope())
        self._battery: Final[ReadableValue[Battery]] = ReadableValue[Battery](Battery())
        self._touch_finger_1: Final[ReadableValue[TouchFinger]] = ReadableValue[TouchFinger](TouchFinger())
        self._touch_finger_2: Final[ReadableValue[TouchFinger]] = ReadableValue[TouchFinger](TouchFinger())
        self._left_trigger_feedback: Final[ReadableValue[TriggerFeedback]] = ReadableValue[TriggerFeedback](TriggerFeedback())
        self._right_trigger_feedback: Final[ReadableValue[TriggerFeedback]] = ReadableValue[TriggerFeedback](TriggerFeedback())
        self._orientation: Final[ReadableValue[Orientation]] = ReadableValue[Orientation](Orientation())

    def before_start(self):
        """
        Initialize the backend with the device information.
        """
        pass

    def read(self):
        """
        Read data from the device.
        """
        self._read_time = time.perf_counter() * 1000.0
        self._read()
        self._last_read_time = self._read_time

    @property
    def square(self) -> ButtonValue:
        return self._square

    @property
    def cross(self) -> ButtonValue:
        return self._cross

    @property
    def circle(self) -> ButtonValue:
        return self._circle

    @property
    def triangle(self) -> ButtonValue:
        return self._triangle

    @property
    def dpad_up(self) -> ButtonValue:
        return self._dpad_up

    @property
    def dpad_right(self) -> ButtonValue:
        return self._dpad_right

    @property
    def dpad_down(self) -> ButtonValue:
        return self._dpad_down

    @property
    def dpad_left(self) -> ButtonValue:
        return self._dpad_left

    @property
    def l1(self) -> ButtonValue:
        return self._l1

    @property
    def r1(self) -> ButtonValue:
        return self._r1

    @property
    def l2(self) -> ButtonValue:
        return self._l2

    @property
    def r2(self) -> ButtonValue:
        return self._r2

    @property
    def share(self) -> ButtonValue:
        return self._share

    @property
    def options(self) -> ButtonValue:
        return self._options

    @property
    def l3(self) -> ButtonValue:
        return self._l3

    @property
    def r3(self) -> ButtonValue:
        return self._r3

    @property
    def ps(self) -> ButtonValue:
        return self._ps

    @property
    def touch(self) -> ButtonValue:
        return self._touch

    @property
    def mikrophone(self) -> ButtonValue:
        return self._mikrophone

    @property
    def left_joy_stick(self) -> ReadableValue[JoyStick]:
        return self._left_joy_stick

    @property
    def right_joy_stick(self) -> ReadableValue[JoyStick]:
        return self._right_joy_stick

    @property
    def l2_trigger(self) -> ReadableValue[float]:
        return self._l2_trigger

    @property
    def r2_trigger(self) -> ReadableValue[float]:
        return self._r2_trigger

    @property
    def accelerometer(self) -> ReadableValue[Accelerometer]:
        return self._accelerometer

    @property
    def gyroscope(self) -> ReadableValue[Gyroscope]:
        return self._gyroscope

    @property
    def battery(self) -> ReadableValue[Battery]:
        return self._battery

    @property
    def touch_finger_1(self) -> ReadableValue[TouchFinger]:
        return self._touch_finger_1

    @property
    def touch_finger_2(self) -> ReadableValue[TouchFinger]:
        return self._touch_finger_2

    @property
    def left_trigger_feedback(self) -> ReadableValue[TriggerFeedback]:
        return self._left_trigger_feedback

    @property
    def right_trigger_feedback(self) -> ReadableValue[TriggerFeedback]:
        return self._right_trigger_feedback

    @property
    def orientation(self) -> ReadableValue[Orientation]:
        return self._orientation
