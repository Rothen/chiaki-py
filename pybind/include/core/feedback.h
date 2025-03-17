#ifndef CHIAKI_PY_CORE_FEEDBACK_H
#define CHIAKI_PY_CORE_FEEDBACK_H

#include "core/struct_wrapper.h"
#include "common.h"
#include "log.h"
#include "controller.h"

#include <chiaki/feedback.h>

#include <vector>

#include <pybind11/pybind11.h>

namespace py = pybind11;

void init_core_feedback(py::module &m);

class FeedbackStateWrapper : public StructWrapper<ChiakiFeedbackState>
{
public:
    using StructWrapper::StructWrapper;

    int16_t get_left_x() { return raw().left_x; }
    void set_left_x(int16_t left_x) { raw().left_x = left_x; }

    int16_t get_left_y() { return raw().left_y; }
    void set_left_y(int16_t left_y) { raw().left_y = left_y; }

    int16_t get_right_x() { return raw().right_x; }
    void set_right_x(int16_t right_x) { raw().right_x = right_x; }

    int16_t get_right_y() { return raw().right_y; }
    void set_right_y(int16_t right_y) { raw().right_y = right_y; }

    float get_gyro_x() { return raw().gyro_x; }
    void set_gyro_x(float gyro_x) { raw().gyro_x = gyro_x; }

    float get_gyro_y() { return raw().gyro_y; }
    void set_gyro_y(float gyro_y) { raw().gyro_y = gyro_y; }

    float get_gyro_z() { return raw().gyro_z; }
    void set_gyro_z(float gyro_z) { raw().gyro_z = gyro_z; }

    float get_accel_x() { return raw().accel_x; }
    void set_accel_x(float accel_x) { raw().accel_x = accel_x; }

    float get_accel_y() { return raw().accel_y; }
    void set_accel_y(float accel_y) { raw().accel_y = accel_y; }

    float get_accel_z() { return raw().accel_z; }
    void set_accel_z(float accel_z) { raw().accel_z = accel_z; }

    float get_orient_x() { return raw().orient_x; }
    void set_orient_x(float orient_x) { raw().orient_x = orient_x; }

    float get_orient_y() { return raw().orient_y; }
    void set_orient_y(float orient_y) { raw().orient_y = orient_y; }

    float get_orient_z() { return raw().orient_z; }
    void set_orient_z(float orient_z) { raw().orient_z = orient_z; }

    float get_orient_w() { return raw().orient_w; }
    void set_orient_w(float orient_w) { raw().orient_w = orient_w; }
};

class FeedbackHistoryEventWrapper : public StructWrapper<ChiakiFeedbackHistoryEvent>
{
public:
    using StructWrapper::StructWrapper;

    ChiakiErrorCode set_button(uint64_t button, uint8_t state)
    {
        return chiaki_feedback_history_event_set_button(ptr(), button, state);
    }

    void set_touchpad(bool down, uint8_t pointer_id, uint16_t x, uint16_t y)
    {
        chiaki_feedback_history_event_set_touchpad(ptr(), down, pointer_id, x, y);
    }
};

class FeedbackHistoryBufferWrapper : public StructWrapper<ChiakiFeedbackHistoryBuffer>
{
public:
    using StructWrapper::StructWrapper;

    ~FeedbackHistoryBufferWrapper() { chiaki_feedback_history_buffer_fini(ptr()); }

    ChiakiErrorCode init(size_t size)
    {
        return chiaki_feedback_history_buffer_init(ptr(), size);
    }

    ChiakiErrorCode format(std::vector<uint8_t> buf)
    {
        size_t buf_size = buf.size();
        return chiaki_feedback_history_buffer_format(ptr(), buf.data(), &buf_size);
    }

    void push(FeedbackHistoryEventWrapper &event) {
        chiaki_feedback_history_buffer_push(ptr(), event.ptr());
    }
};

#endif // CHIAKI_PY_CORE_FEEDBACK_H