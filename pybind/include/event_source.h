#ifndef CHIAKY_PY_EVENT_SOURCE_H
#define CHIAKY_PY_EVENT_SOURCE_H

#include <functional>
#include <exception>

#include <pybind11/pybind11.h>

namespace py = pybind11;

void init_event_source(py::module &m);

class EventSource
{
private:
    std::function<void(int)> on_next_callback;
    std::function<void(std::exception_ptr)> on_error_callback;
    std::function<void()> on_completed_callback;

public:
    void set_on_next(std::function<void(int)> on_next_cb)
    {
        on_next_callback = on_next_cb;
    }

    void set_on_error(std::function<void(std::exception_ptr)> on_error_cb)
    {
        on_error_callback = on_error_cb;
    }

    void set_on_completed(std::function<void()> on_completed_cb)
    {
        on_completed_callback = on_completed_cb;
    }

    void on_next(int value)
    {
        if (on_next_callback)
        {
            on_next_callback(value);
        }
    }

    void on_error(const std::exception &e)
    {
        if (on_error_callback)
        {
            on_error_callback(std::make_exception_ptr(e));
        }
    }

    void on_completed()
    {
        if (on_completed_callback)
        {
            on_completed_callback();
        }
    }
};

#endif // CHIAKY_PY_EVENT_SOURCE_H