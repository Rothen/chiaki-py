import time
from typing import Final, Any
from threading import Thread, Event

from reactivex.abc import DisposableBase

from .backends import DeviceInfo
from .readable_value import ChangeCallable, ButtonPressedCallable, ButtonReleasedCallable
from .states import (
    Accelerometer,
    Battery,
    Gyroscope,
    JoyStick,
    Orientation,
    TouchFinger,
    TriggerFeedback,
)


class DualSenseController:
    __slots__ = (
        "_device_info",
        "_read_thread",
        "_exit_event",
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
        "__read_time",
        "__loop_time"
    )

    @property
    def read_time(self) -> float:
        return self.__read_time

    @property
    def loop_time(self) -> float:
        return self.__loop_time

    def __init__(self, device_info: DeviceInfo[Any, Any]) -> None:
        self._device_info: Final[DeviceInfo[Any, Any]] = device_info
        self._read_thread: Final[Thread] = Thread(target=self._read_loop, daemon=True)
        self._exit_event: Final[Event] = Event()

        self.__loop_time: float = -1.0

    def open(self):
        self._device_info.open()
        self._read_thread.start()

    def close(self):
        self._exit_event.set()
        self._read_thread.join()

    def _read_loop(self) -> None:
        self._device_info.before_start()

        while not self._exit_event.is_set():
            start = time.perf_counter()
            self.__read_time = (time.perf_counter() - start) * 1000.0
            self._device_info.read()
            self.__loop_time = (time.perf_counter() - start) * 1000.0

        self._device_info.close()

    def square_pressed(self, callback: ButtonPressedCallable) -> DisposableBase:
        return self._device_info.square.pressed(callback)

    def cross_pressed(self, callback: ButtonPressedCallable) -> DisposableBase:
        return self._device_info.cross.pressed(callback)

    def circle_pressed(self, callback: ButtonPressedCallable) -> DisposableBase:
        return self._device_info.circle.pressed(callback)

    def triangle_pressed(self, callback: ButtonPressedCallable) -> DisposableBase:
        return self._device_info.triangle.pressed(callback)

    def dpad_up_pressed(self, callback: ButtonPressedCallable) -> DisposableBase:
        return self._device_info.dpad_up.pressed(callback)

    def dpad_right_pressed(self, callback: ButtonPressedCallable) -> DisposableBase:
        return self._device_info.dpad_right.pressed(callback)

    def dpad_down_pressed(self, callback: ButtonPressedCallable) -> DisposableBase:
        return self._device_info.dpad_down.pressed(callback)

    def dpad_left_pressed(self, callback: ButtonPressedCallable) -> DisposableBase:
        return self._device_info.dpad_left.pressed(callback)

    def l1_pressed(self, callback: ButtonPressedCallable) -> DisposableBase:
        return self._device_info.l1.pressed(callback)

    def r1_pressed(self, callback: ButtonPressedCallable) -> DisposableBase:
        return self._device_info.r1.pressed(callback)

    def l2_pressed(self, callback: ButtonPressedCallable) -> DisposableBase:
        return self._device_info.l2.pressed(callback)

    def r2_pressed(self, callback: ButtonPressedCallable) -> DisposableBase:
        return self._device_info.r2.pressed(callback)

    def share_pressed(self, callback: ButtonPressedCallable) -> DisposableBase:
        return self._device_info.share.pressed(callback)

    def options_pressed(self, callback: ButtonPressedCallable) -> DisposableBase:
        return self._device_info.options.pressed(callback)

    def l3_pressed(self, callback: ButtonPressedCallable) -> DisposableBase:
        return self._device_info.l3.pressed(callback)

    def r3_pressed(self, callback: ButtonPressedCallable) -> DisposableBase:
        return self._device_info.r3.pressed(callback)

    def ps_pressed(self, callback: ButtonPressedCallable) -> DisposableBase:
        return self._device_info.ps.pressed(callback)

    def touch_pressed(self, callback: ButtonPressedCallable) -> DisposableBase:
        return self._device_info.touch.pressed(callback)

    def mikrophone_pressed(self, callback: ButtonPressedCallable) -> DisposableBase:
        return self._device_info.mikrophone.pressed(callback)

    def square_released(self, callback: ButtonReleasedCallable) -> DisposableBase:
        return self._device_info.square.released(callback)

    def cross_released(self, callback: ButtonReleasedCallable) -> DisposableBase:
        return self._device_info.cross.released(callback)

    def circle_released(self, callback: ButtonReleasedCallable) -> DisposableBase:
        return self._device_info.circle.released(callback)

    def triangle_released(self, callback: ButtonReleasedCallable) -> DisposableBase:
        return self._device_info.triangle.released(callback)

    def dpad_up_released(self, callback: ButtonReleasedCallable) -> DisposableBase:
        return self._device_info.dpad_up.released(callback)

    def dpad_right_released(self, callback: ButtonReleasedCallable) -> DisposableBase:
        return self._device_info.dpad_right.released(callback)

    def dpad_down_released(self, callback: ButtonReleasedCallable) -> DisposableBase:
        return self._device_info.dpad_down.released(callback)

    def dpad_left_released(self, callback: ButtonReleasedCallable) -> DisposableBase:
        return self._device_info.dpad_left.released(callback)

    def l1_released(self, callback: ButtonReleasedCallable) -> DisposableBase:
        return self._device_info.l1.released(callback)

    def r1_released(self, callback: ButtonReleasedCallable) -> DisposableBase:
        return self._device_info.r1.released(callback)

    def l2_released(self, callback: ButtonReleasedCallable) -> DisposableBase:
        return self._device_info.l2.released(callback)

    def r2_released(self, callback: ButtonReleasedCallable) -> DisposableBase:
        return self._device_info.r2.released(callback)

    def share_released(self, callback: ButtonReleasedCallable) -> DisposableBase:
        return self._device_info.share.released(callback)

    def options_released(self, callback: ButtonReleasedCallable) -> DisposableBase:
        return self._device_info.options.released(callback)

    def l3_released(self, callback: ButtonReleasedCallable) -> DisposableBase:
        return self._device_info.l3.released(callback)

    def r3_released(self, callback: ButtonReleasedCallable) -> DisposableBase:
        return self._device_info.r3.released(callback)

    def ps_released(self, callback: ButtonReleasedCallable) -> DisposableBase:
        return self._device_info.ps.released(callback)

    def touch_released(self, callback: ButtonReleasedCallable) -> DisposableBase:
        return self._device_info.touch.released(callback)

    def mikrophone_released(self, callback: ButtonReleasedCallable) -> DisposableBase:
        return self._device_info.mikrophone.released(callback)

    def left_joy_stick_changed(self, callback: ChangeCallable[JoyStick]) -> DisposableBase:
        return self._device_info.left_joy_stick.subscribe(callback)

    def right_joy_stick_changed(self, callback: ChangeCallable[JoyStick]) -> DisposableBase:
        return self._device_info.right_joy_stick.subscribe(callback)

    def l2_trigger_changed(self, callback: ChangeCallable[float]) -> DisposableBase:
        return self._device_info.l2_trigger.subscribe(callback)

    def r2_trigger_changed(self, callback: ChangeCallable[float]) -> DisposableBase:
        return self._device_info.r2_trigger.subscribe(callback)

    def accelerometer_changed(self, callback: ChangeCallable[Accelerometer]) -> DisposableBase:
        return self._device_info.accelerometer.subscribe(callback)

    def gyroscope_changed(self, callback: ChangeCallable[Gyroscope]) -> DisposableBase:
        return self._device_info.gyroscope.subscribe(callback)

    def battery_changed(self, callback: ChangeCallable[Battery]) -> DisposableBase:
        return self._device_info.battery.subscribe(callback)

    def touch_finger_1_changed(self, callback: ChangeCallable[TouchFinger]) -> DisposableBase:
        return self._device_info.touch_finger_1.subscribe(callback)

    def touch_finger_2_changed(self, callback: ChangeCallable[TouchFinger]) -> DisposableBase:
        return self._device_info.touch_finger_2.subscribe(callback)

    def left_trigger_feedback_changed(self, callback: ChangeCallable[TriggerFeedback]) -> DisposableBase:
        return self._device_info.left_trigger_feedback.subscribe(callback)

    def right_trigger_feedback_changed(self, callback: ChangeCallable[TriggerFeedback]) -> DisposableBase:
        return self._device_info.right_trigger_feedback.subscribe(callback)

    def orientation_changed(self, callback: ChangeCallable[Orientation]) -> DisposableBase:
        return self._device_info.orientation.subscribe(callback)

    def set_led(self, red: int, green: int, blue: int) -> None:
        self._device_info.set_led(red, green, blue)
