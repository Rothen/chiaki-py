#include "event_source.h"

#include <pybind11/pybind11.h>

namespace py = pybind11;

void init_event_source(py::module &m)
{
    py::class_<EventSource>(m, "EventSource")
        .def(py::init<>())
        .def("set_on_next", &EventSource::set_on_next)
        .def("set_on_error", [](EventSource &self, py::function py_callback) {
            self.set_on_error([py_callback](std::exception_ptr e_ptr) {
                try {
                    if (e_ptr)
                    {
                        std::rethrow_exception(e_ptr);
                    }
                }
                catch (const std::exception &e)
                {
                    py_callback(py::value_error(e.what()));
                }
            });
        })
        .def("set_on_completed", &EventSource::set_on_completed)
        .def("on_next", &EventSource::on_next)
        .def("on_error", [](EventSource &self, const std::string &error_message)
             {
                 self.on_error(py::value_error(error_message));
             })
        .def("on_completed", &EventSource::on_completed);
}