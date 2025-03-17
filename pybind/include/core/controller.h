#ifndef CHIAKI_PY_CORE_CONTROLLER_H
#define CHIAKI_PY_CORE_CONTROLLER_H

#include "core/struct_wrapper.h"
#include "core/common.h"

#include <chiaki/controller.h>

#include <vector>

#include <pybind11/pybind11.h>

namespace py = pybind11;

void init_core_controller(py::module &m);

class ControllerTouchWrapper : public StructWrapper<ChiakiControllerTouch>
{
public:
    using StructWrapper::StructWrapper;

    ControllerTouchWrapper(uint16_t x, uint16_t y, int8_t id) : StructWrapper()
    {
        raw().x = x;
        raw().y = y;
        raw().id = id;
    }

    uint16_t get_x() { return raw().x; }
    void set_x(uint16_t x) { raw().x = x; }

    uint16_t get_y() { return raw().y; }
    void set_y(uint16_t y) { raw().y = y; }

    int8_t get_id() { return raw().id; }
    void set_id(int8_t id) { raw().id = id; }
};

class ControllerStateWrapper : public StructWrapper<ChiakiControllerState>
{
public:
    using StructWrapper::StructWrapper;

    uint32_t get_buttons() { return raw().buttons; }
    void set_buttons(uint32_t buttons) { raw().buttons = buttons; }

    uint8_t get_l2_state() { return raw().l2_state; }
    void set_l2_state(uint8_t l2_state) { raw().l2_state = l2_state; }

    uint8_t get_r2_state() { return raw().r2_state; }
    void set_r2_state(uint8_t r2_state) { raw().r2_state = r2_state; }

    int16_t get_left_x() { return raw().left_x; }
    void set_left_x(int16_t left_x) { raw().left_x = left_x; }

    int16_t get_left_y() { return raw().left_y; }
    void set_left_y(int16_t left_y) { raw().left_y = left_y; }

    int16_t get_right_x() { return raw().right_x; }
    void set_right_x(int16_t right_x) { raw().right_x = right_x; }

    int16_t get_right_y() { return raw().right_y; }
    void set_right_y(int16_t right_y) { raw().right_y = right_y; }

    uint8_t get_touch_id_next() { return raw().touch_id_next; }
    void set_touch_id_next(uint8_t touch_id_next) { raw().touch_id_next = touch_id_next; }

    std::vector<ControllerTouchWrapper> get_touches()
    {
        std::vector<ControllerTouchWrapper> touches;
        for (size_t i = 0; i < CHIAKI_CONTROLLER_TOUCHES_MAX; i++)
        {
            touches.push_back(raw().touches[i]);
        }
        return touches;
    }
    void set_touches(const std::vector<ControllerTouchWrapper> &touches)
    {
        for (size_t i = 0; i < touches.size(); i++)
        {
            raw().touches[i] = touches[i].raw();
        }
    }

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

    void set_idle() { chiaki_controller_state_set_idle(ptr()); }
    int8_t start_touch(uint16_t x, uint16_t y) { return chiaki_controller_state_start_touch(ptr(), x, y); }
    void stop_touch(uint8_t id) { return chiaki_controller_state_stop_touch(ptr(), id); }
    void set_touch_pos(uint8_t id, uint16_t x, uint16_t y) { return chiaki_controller_state_set_touch_pos(ptr(), id, x, y); }
    bool equals(ControllerStateWrapper &other) { return chiaki_controller_state_equals(ptr(), other.ptr()); }
    void or_others(ControllerStateWrapper &a, ControllerStateWrapper &b) { return chiaki_controller_state_or(ptr(), a.ptr(), b.ptr()); }
};

#endif // CHIAKI_PY_CORE_CONTROLLER_H