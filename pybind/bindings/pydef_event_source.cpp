// Bindings of the EventSource<T> instances, one Python class each. Written by hand, unlike the pydef_*.cpp
// files generate_bindings.py writes: litgen would name them after their C++ template arguments.
// Their stubs are in chiaki_py/lib/chiaki_py/__init__.pyi, outside the generated sections.

#include "pydef.h"
#include "event_source.h"

#include <chiaki/regist.h>
#include <chiaki/session.h>

namespace
{
template <typename T>
void bind_event_source(py::module_ &m, const char *name, const char *subscription_name,
                       py::return_value_policy subscribe_policy = py::return_value_policy::reference,
                       const char *doc = "", const char *subscription_doc = "", const char *subscribe_doc = "")
{
    py::class_<typename EventSource<T>::Subscription>(m, subscription_name, subscription_doc)
        .def("unsubscribe", &EventSource<T>::Subscription::unsubscribe);

    py::class_<EventSource<T>>(m, name, doc)
        .def("subscribe", &EventSource<T>::subscribe,
             py::arg("on_next"),
             py::arg("on_error") = py::none(),
             py::arg("on_completed") = py::none(), subscribe_policy,
             subscribe_doc);
}
}

void py_init_event_source(py::module_ &m)
{
    bind_event_source<int>(m, "EventSource", "Subscription", py::return_value_policy::automatic);
    bind_event_source<ChiakiQuitReason>(m, "ChiakiQuitReasonEventSource", "ChiakiQuitReasonEventSourceSubscription");
    bind_event_source<bool>(m, "BoolEventSource", "BoolEventSourceSubscription");
    bind_event_source<double>(m, "DoubleEventSource", "DoubleEventSourceSubscription");
    bind_event_source<std::string>(m, "StringEventSource", "StringEventSourceSubscription");
    bind_event_source<const ChiakiRegisteredHost &>(m, "RegisteredHostEventSource", "RegisteredHostEventSourceSubscription");
    bind_event_source<ChiakiRegistEvent *>(m, "RegistEventSource", "RegistEventSourceSubscription",
        py::return_value_policy::reference,
        "What Backend.register_host_async() returns: a one-shot stream of registration progress that "
        "calls `on_next` with the final RegistEvent once registration succeeds, `on_error` if it fails, "
        "and `on_completed` in either case.",
        "Returned by RegistEventSource.subscribe(); call unsubscribe() to stop receiving callbacks.",
        "Register callbacks for this registration attempt and return a subscription that can be "
        "unsubscribe()'d. Registration only actually starts once the first subscriber attaches.");
}
