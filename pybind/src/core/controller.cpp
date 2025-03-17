#include "core/controller.h"

namespace py = pybind11;

void init_core_controller(py::module &m)
{
    py::enum_<ChiakiControllerButton>(m, "Button")
        .value("CROSS", CHIAKI_CONTROLLER_BUTTON_CROSS)
        .value("MOON", CHIAKI_CONTROLLER_BUTTON_MOON)
        .value("BOX", CHIAKI_CONTROLLER_BUTTON_BOX)
        .value("PYRAMID", CHIAKI_CONTROLLER_BUTTON_PYRAMID)
        .value("DPAD_LEFT", CHIAKI_CONTROLLER_BUTTON_DPAD_LEFT)
        .value("DPAD_RIGHT", CHIAKI_CONTROLLER_BUTTON_DPAD_RIGHT)
        .value("DPAD_UP", CHIAKI_CONTROLLER_BUTTON_DPAD_UP)
        .value("DPAD_DOWN", CHIAKI_CONTROLLER_BUTTON_DPAD_DOWN)
        .value("L1", CHIAKI_CONTROLLER_BUTTON_L1)
        .value("R1", CHIAKI_CONTROLLER_BUTTON_R1)
        .value("L3", CHIAKI_CONTROLLER_BUTTON_L3)
        .value("R3", CHIAKI_CONTROLLER_BUTTON_R3)
        .value("OPTIONS", CHIAKI_CONTROLLER_BUTTON_OPTIONS)
        .value("SHARE", CHIAKI_CONTROLLER_BUTTON_SHARE)
        .value("TOUCHPAD", CHIAKI_CONTROLLER_BUTTON_TOUCHPAD)
        .value("PS", CHIAKI_CONTROLLER_BUTTON_PS);

    m.attr("BUTTONS_COUNT") = CHIAKI_CONTROLLER_BUTTONS_COUNT;

    py::enum_<ChiakiControllerAnalogButton>(m, "AnalogButton")
        .value("L2", CHIAKI_CONTROLLER_ANALOG_BUTTON_L2)
        .value("R2", CHIAKI_CONTROLLER_ANALOG_BUTTON_R2);

    py::class_<ControllerTouchWrapper>(m, "ControllerTouch")
        .def(py::init<uint16_t, uint16_t, int8_t>(),
             py::arg("x"),
             py::arg("y"),
             py::arg("id"))
        .def_property("x", &ControllerTouchWrapper::get_x, &ControllerTouchWrapper::set_x, "The x position.")
        .def_property("y", &ControllerTouchWrapper::get_y, &ControllerTouchWrapper::set_y, "The y position.")
        .def_property("y", &ControllerTouchWrapper::get_id, &ControllerTouchWrapper::set_id, "The id.");

    m.attr("CONTROLLER_TOUCHES_MAX") = CHIAKI_CONTROLLER_TOUCHES_MAX;

    py::class_<ControllerStateWrapper>(m, "ControllerState")
        .def(py::init())
        .def_property("buttons", &ControllerStateWrapper::get_buttons, &ControllerStateWrapper::set_buttons, "The buttons.")
        .def_property("l2_state", &ControllerStateWrapper::get_l2_state, &ControllerStateWrapper::set_l2_state, "The L2 state.")
        .def_property("r2_state", &ControllerStateWrapper::get_r2_state, &ControllerStateWrapper::set_r2_state, "The R2 state.")
        .def_property("left_x", &ControllerStateWrapper::get_left_x, &ControllerStateWrapper::set_left_x, "The left x position.")
        .def_property("left_y", &ControllerStateWrapper::get_left_y, &ControllerStateWrapper::set_left_y, "The left y position.")
        .def_property("right_x", &ControllerStateWrapper::get_right_x, &ControllerStateWrapper::set_right_x, "The right x position.")
        .def_property("right_y", &ControllerStateWrapper::get_right_y, &ControllerStateWrapper::set_right_y, "The right y position.")
        .def_property("touch_id_next", &ControllerStateWrapper::get_touch_id_next, &ControllerStateWrapper::set_touch_id_next, "The next touch id.")
        .def_property("touches", &ControllerStateWrapper::get_touches, &ControllerStateWrapper::set_touches, "The touches.")
        .def_property("gyro_x", &ControllerStateWrapper::get_gyro_x, &ControllerStateWrapper::set_gyro_x, "The x gyro.")
        .def_property("gyro_y", &ControllerStateWrapper::get_gyro_y, &ControllerStateWrapper::set_gyro_y, "The y gyro.")
        .def_property("gyro_z", &ControllerStateWrapper::get_gyro_z, &ControllerStateWrapper::set_gyro_z, "The z gyro.")
        .def_property("accel_x", &ControllerStateWrapper::get_accel_x, &ControllerStateWrapper::set_accel_x, "The x acceleration.")
        .def_property("accel_y", &ControllerStateWrapper::get_accel_y, &ControllerStateWrapper::set_accel_y, "The y acceleration.")
        .def_property("accel_z", &ControllerStateWrapper::get_accel_z, &ControllerStateWrapper::set_accel_z, "The z acceleration.")
        .def_property("orient_x", &ControllerStateWrapper::get_orient_x, &ControllerStateWrapper::set_orient_x, "The x orientation.")
        .def_property("orient_y", &ControllerStateWrapper::get_orient_y, &ControllerStateWrapper::set_orient_y, "The y orientation.")
        .def_property("orient_z", &ControllerStateWrapper::get_orient_z, &ControllerStateWrapper::set_orient_z, "The z orientation.")
        .def_property("orient_w", &ControllerStateWrapper::get_orient_w, &ControllerStateWrapper::set_orient_w, "The w orientation.")
        .def("set_idle", &ControllerStateWrapper::set_idle, "Set the controller state to idle.")
        .def("start_touch", &ControllerStateWrapper::start_touch, py::arg("x"), py::arg("y"), "Start a touch.")
        .def("stop_touch", &ControllerStateWrapper::stop_touch, py::arg("id"), "Stop a touch.")
        .def("set_touch_pos", &ControllerStateWrapper::set_touch_pos, py::arg("id"), py::arg("x"), py::arg("y"), "Set the touch position.")
        .def("equals", &ControllerStateWrapper::equals, py::arg("other"), "Check if two controller states are equal.")
        .def("or_others", &ControllerStateWrapper::or_others, py::arg("a"), py::arg("b"), "Or two controller states.");
}