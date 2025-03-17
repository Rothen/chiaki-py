#include "core/log.h"
#include <functional>

#include <pybind11/functional.h>
#include <pybind11/stl.h>

namespace py = pybind11;

void init_core_log(py::module &m)
{
    py::enum_<ChiakiLogLevel>(m, "LogLevel")
        .value("DEBUG", CHIAKI_LOG_DEBUG)
        .value("VERBOSE", CHIAKI_LOG_VERBOSE)
        .value("INFO", CHIAKI_LOG_INFO)
        .value("WARNING", CHIAKI_LOG_WARNING)
        .value("ERROR", CHIAKI_LOG_ERROR)
        .export_values();

    m.attr("CHIAKI_LOG_ALL") = CHIAKI_LOG_ALL;

    m.def("chiaki_log_level_char", &chiaki_log_level_char, py::arg("level"), "Get the log level char.");

    py::class_<LogWrapper>(m, "Log")
        .def(py::init<ChiakiLogLevel, ChiakiLogCbFunc, void *>(),
             py::arg("level") = CHIAKI_LOG_INFO,
             py::arg("cb") = nullptr,
             py::arg("user") = nullptr)
        .def_property("level", &LogWrapper::get_level, &LogWrapper::set_level)
        .def_property("cb", &LogWrapper::get_cb, &LogWrapper::set_cb)
        .def_property("user", &LogWrapper::get_user, &LogWrapper::set_user)
        .def("cb_print", &LogWrapper::cb_print, py::arg("level"), py::arg("msg"), py::arg("user"), "Chiaki log callback print.")
        .def("log", &LogWrapper::log, py::arg("level"), py::arg("msg"), "Chiaki log.")
        .def("log_hexdump", &LogWrapper::log_hexdump, py::arg("level"), py::arg("buf"), "Chiaki log hexdump.")
        .def("log_hexdump_raw", &LogWrapper::log_hexdump_raw, py::arg("level"), py::arg("buf"), "Chiaki log hexdump raw.")
        .def_static("level_char", &LogWrapper::level_char, py::arg("level"), "Get the log level char.")
        .def("debug", &LogWrapper::debug, py::arg("message"), "Chiaki log debug.")
        .def("verbose", &LogWrapper::verbose, py::arg("message"), "Chiaki log verbose.")
        .def("info", &LogWrapper::info, py::arg("message"), "Chiaki log info.")
        .def("warning", &LogWrapper::warning, py::arg("message"), "Chiaki log warning.")
        .def("error", &LogWrapper::error, py::arg("message"), "Chiaki log error.");

    py::class_<LogSnifferWrapper>(m, "LogSniffer")
        .def(py::init<ChiakiLogLevel, LogWrapper &>(),
             py::arg("level") = CHIAKI_LOG_INFO,
             py::arg("forward_log") = LogWrapper())
        .def_property("forward_log", &LogSnifferWrapper::set_forward_log, &LogSnifferWrapper::get_forward_log)
        .def_property("sniff_log", &LogSnifferWrapper::set_sniff_log, &LogSnifferWrapper::get_sniff_log)
        .def_property("sniff_level", &LogSnifferWrapper::set_sniff_level, &LogSnifferWrapper::get_sniff_level)
        .def_property("buf", &LogSnifferWrapper::set_buf, &LogSnifferWrapper::get_buf);
}