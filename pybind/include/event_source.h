#ifndef CHIAKY_PY_EVENT_SOURCE_H
#define CHIAKY_PY_EVENT_SOURCE_H

#include <functional>
#include <exception>
#include <vector>
#include <algorithm>

#include <chiaki/discovery.h>

#include <pybind11/pybind11.h>
#include <pybind11/functional.h>
#include <pybind11/stl.h>
#include <stdexcept>

namespace py = pybind11;

void init_event_source(py::module &m);

template <typename T>
class EventSource
{
public:
    struct Subscription
    {
        std::function<void(const py::object &)> on_next;
        std::function<void(const py::object &)> on_error;
        std::function<void()> on_completed;
        bool active = true;
        EventSource<T> *parent = nullptr;

        void unsubscribe()
        {
            active = false;

            if (parent)
            {
                parent->cleanup_subscribers();
            }
        }
    };
    
    EventSource() {}

    void on_next(const T &value) const
    {
        for (auto &sub : subscribers)
        {
            if (sub.active && sub.on_next)
                sub.on_next(py::cast(value));
        }
    }

    void on_error(const std::exception &exception)
    {
        py::object err = Exception(exception.what());
        for (auto &sub : subscribers)
        {
            if (sub.active && sub.on_error)
                sub.on_error(err);
        }
    }

    void on_completed()
    {
        for (auto &sub : subscribers)
        {
            if (sub.active && sub.on_completed)
                sub.on_completed();
        }
        subscribers.clear();
    }

    Subscription &subscribe(
        std::function<void(const py::object &)> on_next,
        std::function<void(const py::object &)> on_error = py::none(),
        std::function<void()> on_completed = py::none())
    {
        subscribers.emplace_back(Subscription{on_next, on_error, on_completed, true, this});
        this->on_error(std::exception("Error: No error handler provided."));
        return subscribers.back();
    }

    static void cb_wrapper(chiaki_discovery_host_t *host, unsigned long long timestamp, void *user_data)
    {
        if (user_data)
        {
            auto *event_source = static_cast<EventSource<DiscoveryServiceEvent> *>(user_data);
            T event{};
            event.map_cb(host, timestamp, user_data); // ✅ Add missing argument
            event_source->on_next(event);
        }
    }

    template <typename CallbackType>
    CallbackType cb_to_event() const
    {
        return &cb_wrapper;
    }

private:
    std::vector<Subscription> subscribers;
    py::object Exception = py::module_::import("builtins").attr("Exception");

    void cleanup_subscribers()
    {
        subscribers.erase(
            std::remove_if(subscribers.begin(), subscribers.end(),
                           [](const Subscription &sub)
                           { return !sub.active; }),
            subscribers.end());
    }
};

#endif // CHIAKY_PY_EVENT_SOURCE_H