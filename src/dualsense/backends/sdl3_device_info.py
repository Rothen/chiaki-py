# type: ignore
from __future__ import annotations
import math

from sdl3 import *

from .device_infos import DeviceInfo
from .out_report import Usb01OutReport
from ..readable_value import ButtonValue
from ..states import (
    Accelerometer,
    Battery,
    Gyroscope,
    JoyStick,
    Orientation,
    TouchFinger,
    TriggerFeedback
)

deadzone: float = 0.1


class SDL3DeviceInfo(DeviceInfo[LP_SDL_JoystickID, LP_SDL_JoystickID]):

    __slots__ = (
        "__sdl_opened_gamepad",
        "__button_map",
        "__last_gyro_measurement"
    )

    def __init__(self, device_id: LP_SDL_JoystickID):
        super().__init__(device_id)
        self.__sdl_opened_gamepad: LP_SDL_Gamepad | None = None
        self.__button_map: list[ButtonValue] = [
            self._cross,
            self._circle,
            self._square,
            self._triangle,
            self._share,
            self._ps,
            self._options,
            self._l3,
            self._r3,
            self._l1,
            self._r1,
            self._dpad_up,
            self._dpad_down,
            self._dpad_left,
            self._dpad_right,
            self._mikrophone,
            ButtonValue(), # Filler for undefined button
            ButtonValue(), # Filler for undefined button
            ButtonValue(), # Filler for undefined button
            ButtonValue(), # Filler for undefined button
            self._touch
        ]
        self.__last_gyro_measurement: float = 0.0

    def open(self):
        self.__sdl_opened_gamepad = SDL_OpenGamepad(self._orig_device_info)
        SDL_SetGamepadSensorEnabled(self.__sdl_opened_gamepad, SDL_SENSOR_ACCEL, True)
        SDL_SetGamepadSensorEnabled(self.__sdl_opened_gamepad, SDL_SENSOR_GYRO, True)

    def close(self):
        if self.__sdl_opened_gamepad is None:
            return

        self.set_led(0, 0, 0)
        SDL_CloseGamepad(self.__sdl_opened_gamepad)

    def _read(self):
        if self.__sdl_opened_gamepad is None:
            return

        event: SDL_Event = SDL_Event()
        while SDL_PollEvent(event) != 0:
            # print(SDL_GetGamepadTypeForID(event.gdevice.which), SDL_GAMEPAD_TYPE_PS5)
            self.__map_event(event)

    def write(self):
        """
        Write data to the device.
        """
        pass

    def __map_event(self, event: SDL_Event) -> None:
        if event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            self.__button_map[event.gbutton.button].force_value(True)
        elif event.type == SDL_EVENT_GAMEPAD_BUTTON_UP:
            self.__button_map[event.gbutton.button].force_value(False)
        elif event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION:
            self._map_axis_motion_event(event.gaxis)
        elif event.type == SDL_EVENT_GAMEPAD_SENSOR_UPDATE:
            self._map_sensor_update_event(event.gsensor)
        elif event.type == SDL_EVENT_GAMEPAD_TOUCHPAD_UP:
            pass
        elif event.type == SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN:
            pass
        elif event.type == SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION:
            pass

    def _map_axis_motion_event(self, axis_event: SDL_GamepadAxisEvent) -> None:
        new_value = ((axis_event.value + 32768) / 65535.0) * 2 - 1
        if abs(new_value) < deadzone:
            new_value = 0.0

        if axis_event.axis == SDL_GAMEPAD_AXIS_LEFTX:
            if self.left_joy_stick.value.x == 0.0 and new_value == 0.0:
                return
            self._left_joy_stick.force_value(JoyStick(new_value, self._left_joy_stick.value.y))
        elif axis_event.axis == SDL_GAMEPAD_AXIS_LEFTY:
            if self.left_joy_stick.value.y == 0.0 and new_value == 0.0:
                return
            self._left_joy_stick.force_value(JoyStick(self._left_joy_stick.value.x, new_value))
        elif axis_event.axis == SDL_GAMEPAD_AXIS_RIGHTX:
            if self.right_joy_stick.value.x == 0.0 and new_value == 0.0:
                return
            self._right_joy_stick.force_value(JoyStick(new_value, self._right_joy_stick.value.y))
        elif axis_event.axis == SDL_GAMEPAD_AXIS_RIGHTY:
            if self.right_joy_stick.value.y == 0.0 and new_value == 0.0:
                return
            self._right_joy_stick.force_value(JoyStick(self._right_joy_stick.value.x, new_value))
        elif axis_event.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
            self._l2_trigger.force_value(axis_event.value / 32767.0)
            self._l2.force_value(self._l2_trigger.value == 1.0)
        elif axis_event.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:
            self._r2_trigger.force_value(axis_event.value / 32767.0)
            self._r2.force_value(self._r2_trigger.value == 1.0)

    def _map_sensor_update_event(self, sensor_event: SDL_GamepadSensorEvent) -> None:
        if sensor_event.sensor == SDL_SENSOR_ACCEL:
            self._accelerometer.force_value(Accelerometer(sensor_event.data[0], sensor_event.data[1], sensor_event.data[2]))
        elif sensor_event.sensor == SDL_SENSOR_GYRO:
            self._gyroscope.force_value(Gyroscope(sensor_event.data[0], sensor_event.data[1], sensor_event.data[2]))

            alpha: float = 0.98
            roll_acc = math.atan2(self._accelerometer.value.y, self._accelerometer.value.z)
            pitch_acc = math.atan2(-self._accelerometer.value.x,
                                math.sqrt(self._accelerometer.value.y**2 + self._accelerometer.value.z**2))

            dt = (self.__last_gyro_measurement - sensor_event.timestamp) / 1000000.0
            self.__last_gyro_measurement = sensor_event.timestamp

            roll = alpha * (self._orientation.value.roll + self._gyroscope.value.x * dt) + \
                (1 - alpha) * roll_acc
            pitch = alpha * (self._orientation.value.pitch + self._gyroscope.value.y * dt) + \
                (1 - alpha) * pitch_acc
            yaw = self._orientation.value.yaw + self._gyroscope.value.z * dt  # yaw only from gyro
            self._orientation.force_value(Orientation(
                pitch=pitch,
                roll=roll,
                yaw=yaw
            ))

    def set_led(self, r: int, g: int, b: int) -> bool:
        return SDL_SetGamepadLED(self.__sdl_opened_gamepad, r, g, b)

    def test(self) -> None:
        out_report = Usb01OutReport()
        out_report.microphone_led = 0x01
        data = bytes(out_report.data())
        effect_data = data
        buffer = ctypes.create_string_buffer(effect_data)
        SDL_SendGamepadEffect(self.__sdl_opened_gamepad, buffer, len(effect_data))
