from .in_report import InReport, BytesLike


# ??? byte 31
# ??? byte 40
# ??? bytes 43-51
class Usb01InReport(InReport):
    def __init__(self, data: BytesLike = bytearray(100)):
        super().__init__({
            "axes_0": 0, "axes_1": 1, "axes_2": 2, "axes_3": 3, "axes_4": 4, "axes_5": 5,
            "seq_num": 6,
            "buttons_0": 7, "buttons_1": 8, "buttons_2": 9, "buttons_3": 10,
            "timestamp_0": 11, "timestamp_1": 12, "timestamp_2": 13, "timestamp_3": 14,
            "gyro_x_0": 15, "gyro_x_1": 16, "gyro_y_0": 17, "gyro_y_1": 18, "gyro_z_0": 19, "gyro_z_1": 20,
            "accel_x_0": 21, "accel_x_1": 22, "accel_y_0": 23, "accel_y_1": 24, "accel_z_0": 25, "accel_z_1": 26,
            "sensor_timestamp_0": 27, "sensor_timestamp_1": 28, "sensor_timestamp_2": 29, "sensor_timestamp_3": 30,
            "touch_1_0": 32, "touch_1_1": 33, "touch_1_2": 34, "touch_1_3": 35,
            "touch_2_0": 36, "touch_2_1": 37, "touch_2_2": 38, "touch_2_3": 39,
            "right_trigger_feedback": 41, "left_trigger_feedback": 42,
            "battery_0": 52, "battery_1": 53,
            "unknown_0": 31, "unknown_1": 40, "unknown_2": 43, "unknown_3": 44, "unknown_4": 45, "unknown_5": 46,
            "unknown_6": 47, "unknown_7": 48, "unknown_8": 49, "unknown_9": 50, "unknown_10": 51
        }, data=data)
