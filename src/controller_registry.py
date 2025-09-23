import math
from dualsense.dual_sense_controller import DualSenseController
from dualsense.states import JoyStick, Accelerometer, Gyroscope, Orientation
from chiaki_py import StreamSession

        
def __left_stick_change(self, joy_stick: JoyStick):
    self.stream_session.set_left(int(joy_stick.x * 1023), int(joy_stick.y * 1023))

def __right_stick_change(self, joy_stick: JoyStick):
    self.stream_session.set_right(int(joy_stick.x * 1023), int(joy_stick.y * 1023))

def __accelerometer_change(self, accelerometer: Accelerometer):
    self.stream_session.set_accelerometer(accelerometer.x, accelerometer.y, accelerometer.z)

def __gyroscope_change(self, gyroscope: Gyroscope):
    self.stream_session.set_gyroscope(gyroscope.x, gyroscope.y, gyroscope.z)

def __orientation_change(self, orientation: Orientation):
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

    self.stream_session.set_orientation(x, y, z, w)

def register_controller(controller: DualSenseController, stream_session: StreamSession) -> None:
    """Registers a DualSense controller."""
    controller.open()

    controller.cross_pressed(stream_session.press_cross)
    controller.cross_released(stream_session.release_cross)

    controller.circle_pressed(stream_session.press_circle)
    controller.circle_released(stream_session.release_circle)

    controller.square_pressed(stream_session.press_square)
    controller.square_released(stream_session.release_square)
    
    controller.triangle_pressed(stream_session.press_triangle)
    controller.triangle_released(stream_session.release_triangle)
    
    controller.dpad_left_pressed(stream_session.press_left)
    controller.dpad_left_released(stream_session.release_left)

    controller.dpad_right_pressed(stream_session.press_right)
    controller.dpad_right_released(stream_session.release_right)

    controller.dpad_up_pressed(stream_session.press_up)
    controller.dpad_up_released(stream_session.release_up)

    controller.dpad_down_pressed(stream_session.press_down)
    controller.dpad_down_released(stream_session.release_down)

    controller.l1_pressed(stream_session.press_l1)
    controller.l1_released(stream_session.release_l1)

    controller.r1_pressed(stream_session.press_r1)
    controller.r1_released(stream_session.release_r1)

    controller.l3_pressed(stream_session.press_l3)
    controller.l3_released(stream_session.release_l3)

    controller.r3_pressed(stream_session.press_r3)
    controller.r3_released(stream_session.release_r3)

    controller.options_pressed(stream_session.press_options)
    controller.options_released(stream_session.release_touchpad)

    controller.share_pressed(stream_session.press_create)
    controller.share_released(stream_session.release_touchpad)

    controller.touch_pressed(stream_session.press_touchpad)
    controller.touch_released(stream_session.release_touchpad)

    controller.ps_pressed(stream_session.press_ps)
    controller.ps_released(stream_session.release_ps)
    
    controller.l2_trigger_changed(lambda value: stream_session.set_l2(int(value * 255)))
    controller.r2_trigger_changed(lambda value: stream_session.set_r2(int(value * 255)))

    controller.left_joy_stick_changed(lambda joy_stick: __left_stick_change(joy_stick))
    controller.right_joy_stick_changed(lambda joy_stick: __right_stick_change(joy_stick))
    
    controller.accelerometer_changed(lambda accelerometer: __accelerometer_change(accelerometer))
    
    controller.gyroscope_changed(lambda gyroscope: __gyroscope_change(gyroscope))

    controller.orientation_changed(lambda orientation: __orientation_change(orientation))