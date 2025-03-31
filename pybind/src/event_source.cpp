#include "event_source.h"

#include <pybind11/pybind11.h>

namespace py = pybind11;

void init_event_source(py::module &m)
{
    py::class_<EventSource<int>>(m, "EventSource")
        .def(py::init<>())
        .def("subscribe", &EventSource<int>::subscribe,
             py::arg("on_next"),
             py::arg("on_error") = py::none(),
             py::arg("on_completed") = py::none());

    py::class_<EventSource<int>::Subscription>(m, "Subscription")
        .def("unsubscribe", &EventSource<int>::Subscription::unsubscribe);
}