from __future__ import annotations

from abc import ABC
from typing import Final, TypeAlias

_IndexDict = dict[str, int]
BytesLike: TypeAlias = bytearray

DPAD_UP_BIT_MASK: Final[int] = 0b0000
DPAD_RIGHT_BIT_MASK: Final[int] = 0b0010
DPAD_DOWN_BIT_MASK: Final[int] = 0b0100
DPAD_LEFT_BIT_MASK: Final[int] = 0b0110
DPAD_NE_BIT_MASK: Final[int] = 0b0001
DPAD_SE_BIT_MASK: Final[int] = 0b0011
DPAD_SW_BIT_MASK: Final[int] = 0b0101
DPAD_NW_BIT_MASK: Final[int] = 0b0111


class InReport(ABC):
    _OFFSET: Final[int] = 1

    SQUARE_BIT: int = 4
    CROSS_BIT: int = 5
    CIRCLE_BIT: int = 6
    TRIANGLE_BIT: int = 7

    # buttons 1
    L1_BIT: int = 0
    R1_BIT: int = 1
    L2_BIT: int = 2
    R2_BIT: int = 3
    SHARE_BIT: int = 4
    OPTIONS_BIT: int = 5
    L3_BIT: int = 6
    R3_BIT: int = 7

    # buttons 2
    PS_BIT: int = 0
    TOUCH_BIT: int = 1
    MIKROPHONE_BIT: int = 2

    DPAD_MAPPING: list[list[int]] = [
        [DPAD_UP_BIT_MASK, DPAD_NE_BIT_MASK, DPAD_NW_BIT_MASK],
        [DPAD_RIGHT_BIT_MASK, DPAD_NE_BIT_MASK, DPAD_SE_BIT_MASK],
        [DPAD_DOWN_BIT_MASK, DPAD_SE_BIT_MASK, DPAD_SW_BIT_MASK],
        [DPAD_LEFT_BIT_MASK, DPAD_NW_BIT_MASK, DPAD_SW_BIT_MASK]
    ]

    @property
    def data(self) -> BytesLike:
        return self._data

    def __init__(self, index_dict: _IndexDict, data: BytesLike):
        self._index_dict: Final[_IndexDict] = index_dict
        self._data: BytesLike = data

    def update(self, data: BytesLike) -> None:
        self._data = data

    def _get_uint8(self, key: str) -> int:
        return self._data[InReport._OFFSET + self._index_dict.get(key)]

    def _set_uint8(self, key: str, value: int) -> None:
        self._data[InReport._OFFSET + self._index_dict.get(key)] = value

    # ########################################## GET ##########################################
    @property
    def unknown_0(self) -> int:
        return self._get_uint8('unknown_0')
    
    @property
    def unknown_1(self) -> int: # Something to do with the touchpad
        return self._get_uint8('unknown_1')
    
    @property
    def unknown_2(self) -> int:
        return self._get_uint8('unknown_2')
    
    @property
    def unknown_3(self) -> int:
        return self._get_uint8('unknown_3')
    
    @property
    def unknown_4(self) -> int:
        return self._get_uint8('unknown_4')
    
    @property
    def unknown_5(self) -> int:
        return self._get_uint8('unknown_5')
    
    @property
    def unknown_6(self) -> int:
        return self._get_uint8('unknown_6')
    
    @property
    def unknown_7(self) -> int:
        return self._get_uint8('unknown_7')
    
    @property
    def unknown_8(self) -> int:
        return self._get_uint8('unknown_8')
    
    @property
    def unknown_9(self) -> int:
        return self._get_uint8('unknown_9')
    
    @property
    def unknown_10(self) -> int:
        return self._get_uint8('unknown_10')

    @property
    def axes_0(self) -> int:
        return self._get_uint8('axes_0')

    @property
    def axes_1(self) -> int:
        return self._get_uint8('axes_1')

    @property
    def axes_2(self) -> int:
        return self._get_uint8('axes_2')

    @property
    def axes_3(self) -> int:
        return self._get_uint8('axes_3')

    @property
    def axes_4(self) -> int:
        return self._get_uint8('axes_4')

    @property
    def axes_5(self) -> int:
        return self._get_uint8('axes_5')

    @property
    def seq_num(self) -> int:
        return self._get_uint8('seq_num')

    @property
    def buttons_0(self) -> int:
        return self._get_uint8('buttons_0')

    @property
    def buttons_1(self) -> int:
        return self._get_uint8('buttons_1')

    @property
    def buttons_2(self) -> int:
        return self._get_uint8('buttons_2')

    @property
    def buttons_3(self) -> int:
        return self._get_uint8('buttons_3')

    @property
    def timestamp_0(self) -> int:
        return self._get_uint8('timestamp_0')

    @property
    def timestamp_1(self) -> int:
        return self._get_uint8('timestamp_1')

    @property
    def timestamp_2(self) -> int:
        return self._get_uint8('timestamp_2')

    @property
    def timestamp_3(self) -> int:
        return self._get_uint8('timestamp_3')

    @property
    def gyro_x_0(self) -> int:
        return self._get_uint8('gyro_x_0')

    @property
    def gyro_x_1(self) -> int:
        return self._get_uint8('gyro_x_1')

    @property
    def gyro_y_0(self) -> int:
        return self._get_uint8('gyro_y_0')

    @property
    def gyro_y_1(self) -> int:
        return self._get_uint8('gyro_y_1')

    @property
    def gyro_z_0(self) -> int:
        return self._get_uint8('gyro_z_0')

    @property
    def gyro_z_1(self) -> int:
        return self._get_uint8('gyro_z_1')

    @property
    def accel_x_0(self) -> int:
        return self._get_uint8('accel_x_0')

    @property
    def accel_x_1(self) -> int:
        return self._get_uint8('accel_x_1')

    @property
    def accel_y_0(self) -> int:
        return self._get_uint8('accel_y_0')

    @property
    def accel_y_1(self) -> int:
        return self._get_uint8('accel_y_1')

    @property
    def accel_z_0(self) -> int:
        return self._get_uint8('accel_z_0')

    @property
    def accel_z_1(self) -> int:
        return self._get_uint8('accel_z_1')

    @property
    def sensor_timestamp_0(self) -> int:
        return self._get_uint8('sensor_timestamp_0')

    @property
    def sensor_timestamp_1(self) -> int:
        return self._get_uint8('sensor_timestamp_1')

    @property
    def sensor_timestamp_2(self) -> int:
        return self._get_uint8('sensor_timestamp_2')

    @property
    def sensor_timestamp_3(self) -> int:
        return self._get_uint8('sensor_timestamp_3')

    @property
    def touch_1_0(self) -> int:
        return self._get_uint8('touch_1_0')

    @property
    def touch_1_1(self) -> int:
        return self._get_uint8('touch_1_1')

    @property
    def touch_1_2(self) -> int:
        return self._get_uint8('touch_1_2')

    @property
    def touch_1_3(self) -> int:
        return self._get_uint8('touch_1_3')

    @property
    def touch_2_0(self) -> int:
        return self._get_uint8('touch_2_0')

    @property
    def touch_2_1(self) -> int:
        return self._get_uint8('touch_2_1')

    @property
    def touch_2_2(self) -> int:
        return self._get_uint8('touch_2_2')

    @property
    def touch_2_3(self) -> int:
        return self._get_uint8('touch_2_3')

    @property
    def right_trigger_feedback(self) -> int:
        return self._get_uint8('right_trigger_feedback')

    @property
    def left_trigger_feedback(self) -> int:
        return self._get_uint8('left_trigger_feedback')

    @property
    def battery_0(self) -> int:
        return self._get_uint8('battery_0')

    @property
    def battery_1(self) -> int:
        return self._get_uint8('battery_1')
