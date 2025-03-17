#include "core/feedback.h"

namespace py = pybind11;

void init_core_feedback(py::module &m)
{

    py::class_<FeedbackStateWrapper>(m, "FeedbackState")
        .def(py::init())
        .def_property("left_x", &FeedbackStateWrapper::get_left_x, &FeedbackStateWrapper::set_left_x, "The left x position.")
        .def_property("left_y", &FeedbackStateWrapper::get_left_y, &FeedbackStateWrapper::set_left_y, "The left y position.")
        .def_property("right_x", &FeedbackStateWrapper::get_right_x, &FeedbackStateWrapper::set_right_x, "The right x position.")
        .def_property("right_y", &FeedbackStateWrapper::get_right_y, &FeedbackStateWrapper::set_right_y, "The right y position.")
        .def_property("gyro_x", &FeedbackStateWrapper::get_gyro_x, &FeedbackStateWrapper::set_gyro_x, "The x gyro.")
        .def_property("gyro_y", &FeedbackStateWrapper::get_gyro_y, &FeedbackStateWrapper::set_gyro_y, "The y gyro.")
        .def_property("gyro_z", &FeedbackStateWrapper::get_gyro_z, &FeedbackStateWrapper::set_gyro_z, "The z gyro.")
        .def_property("accel_x", &FeedbackStateWrapper::get_accel_x, &FeedbackStateWrapper::set_accel_x, "The x acceleration.")
        .def_property("accel_y", &FeedbackStateWrapper::get_accel_y, &FeedbackStateWrapper::set_accel_y, "The y acceleration.")
        .def_property("accel_z", &FeedbackStateWrapper::get_accel_z, &FeedbackStateWrapper::set_accel_z, "The z acceleration.")
        .def_property("orient_x", &FeedbackStateWrapper::get_orient_x, &FeedbackStateWrapper::set_orient_x, "The x orientation.")
        .def_property("orient_y", &FeedbackStateWrapper::get_orient_y, &FeedbackStateWrapper::set_orient_y, "The y orientation.")
        .def_property("orient_z", &FeedbackStateWrapper::get_orient_z, &FeedbackStateWrapper::set_orient_z, "The z orientation.")
        .def_property("orient_w", &FeedbackStateWrapper::get_orient_w, &FeedbackStateWrapper::set_orient_w, "The w orientation.");

    m.attr("FEEDBACK_STATE_BUF_SIZE_MAX") = CHIAKI_FEEDBACK_STATE_BUF_SIZE_MAX;
    m.attr("FEEDBACK_STATE_BUF_SIZE_V9") = CHIAKI_FEEDBACK_STATE_BUF_SIZE_V9;

    m.def("format_v9", [](std::vector<uint8_t> &buf, ChiakiFeedbackState &state) {
        chiaki_feedback_state_format_v9(buf.data(), &state);
    }, py::arg("buf"), py::arg("state"), "Format the feedback state to the buffer.");
    
    m.attr("FEEDBACK_STATE_BUF_SIZE_V12") = CHIAKI_FEEDBACK_STATE_BUF_SIZE_V12;

    m.def("format_v12", [](std::vector<uint8_t> &buf, ChiakiFeedbackState &state) {
        chiaki_feedback_state_format_v12(buf.data(), &state);
    }, py::arg("buf"), py::arg("state"), "Format the feedback state to the buffer.");

    m.attr("HISTORY_EVENT_SIZE_MAX") = CHIAKI_HISTORY_EVENT_SIZE_MAX;

    py::class_<FeedbackHistoryEventWrapper>(m, "FeedbackHistoryEvent")
        .def(py::init())
        .def("set_button", &FeedbackHistoryEventWrapper::set_button,
             py::arg("button"),
             py::arg("state"),
             "Set the button.")
        .def("set_touchpad", &FeedbackHistoryEventWrapper::set_touchpad,
             py::arg("down"),
             py::arg("pointer_id"),
             py::arg("x"),
             py::arg("y"),
             "Set the touchpad.");

    py::class_<FeedbackHistoryBufferWrapper>(m, "FeedbackHistoryBuffer")
        .def(py::init())
        .def("init", &FeedbackHistoryBufferWrapper::init, py::arg("size"), "Initialize the buffer.")
        .def("format", &FeedbackHistoryBufferWrapper::format, py::arg("buf"), "Format the buffer.")
        .def("push", &FeedbackHistoryBufferWrapper::push, py::arg("event"), "Push the event.");
}