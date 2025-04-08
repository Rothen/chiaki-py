#include "event_source.h"

#include <chiaki/session.h>

#include <pybind11/pybind11.h>

namespace py = pybind11;

void init_event_source(py::module &m)
{

    py::class_<EventSource<int>::Subscription>(m, "Subscription")
        .def("unsubscribe", &EventSource<int>::Subscription::unsubscribe);

    py::class_<EventSource<ChiakiQuitReason>::Subscription>(m, "ChiakiQuitReasonEventSourceSubscription")
        .def("unsubscribe", &EventSource<ChiakiQuitReason>::Subscription::unsubscribe);

    py::class_<EventSource<bool>::Subscription>(m, "BoolEventSourceSubscription")
        .def("unsubscribe", &EventSource<bool>::Subscription::unsubscribe);

    py::class_<EventSource<double>::Subscription>(m, "DoubleEventSourceSubscription")
        .def("unsubscribe", &EventSource<double>::Subscription::unsubscribe);

    py::class_<EventSource<std::string>::Subscription>(m, "StringEventSourceSubscription")
        .def("unsubscribe", &EventSource<std::string>::Subscription::unsubscribe);

    py::class_<EventSource<ChiakiQuitReason>>(m, "ChiakiQuitReasonEventSource")
        .def("subscribe", &EventSource<ChiakiQuitReason>::subscribe,
             py::arg("on_next"),
             py::arg("on_error") = py::none(),
             py::arg("on_completed") = py::none(), py::return_value_policy::reference);

    py::class_<EventSource<int>>(m, "EventSource")
        .def("subscribe", &EventSource<int>::subscribe,
             py::arg("on_next"),
             py::arg("on_error") = py::none(),
             py::arg("on_completed") = py::none());

    py::class_<EventSource<bool>>(m, "BoolEventSource")
        .def("subscribe", &EventSource<bool>::subscribe,
             py::arg("on_next"),
             py::arg("on_error") = py::none(),
             py::arg("on_completed") = py::none(), py::return_value_policy::reference);

    py::class_<EventSource<double>>(m, "DoubleEventSource")
        .def("subscribe", &EventSource<double>::subscribe,
             py::arg("on_next"),
             py::arg("on_error") = py::none(),
             py::arg("on_completed") = py::none(), py::return_value_policy::reference);

    py::class_<EventSource<std::string>>(m, "StringEventSource")
        .def("subscribe", &EventSource<std::string>::subscribe,
             py::arg("on_next"),
             py::arg("on_error") = py::none(),
             py::arg("on_completed") = py::none(), py::return_value_policy::reference);
}