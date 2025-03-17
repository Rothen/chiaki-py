#include "core/log.h"
#include <functional>

#include <pybind11/pybind11.h>
#include <pybind11/functional.h>
#include <pybind11/stl.h>

#include <chiaki/log.h>

namespace py = pybind11;

class ChiakiLogWrapper
{
private:
    ChiakiLog log;
    std::function<void(ChiakiLogLevel, std::string, void *)> callback;

    // C-style callback to call Python function
    static void LogCallback(ChiakiLogLevel level, const char *msg, void *user)
    {
        auto *wrapper = static_cast<ChiakiLogWrapper *>(user);
        if (wrapper->callback)
        {
            wrapper->callback(level, msg, user);
        }
    }

public:
    ChiakiLogWrapper(uint32_t level_mask, std::function<void(ChiakiLogLevel, std::string, void *)> cb, void *user) : callback(cb)
    {
        log.level_mask = level_mask;
        log.cb = LogCallback;
        log.user = user;
    }

    ChiakiLog *raw() { return &log; }

    void set_level_mask(uint32_t level_mask)
    {
        log.level_mask = level_mask;
    }
    uint32_t get_level_mask() { return log.level_mask; }

    void get_cb(std::function<void(ChiakiLogLevel, std::string, void *)> cb) { callback = cb; }
    std::function<void(ChiakiLogLevel, std::string, void *)> set_cb() { return callback; }

    void set_user(void *user) { log.user = user; }
    void *get_user() { return log.user; }
};

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

    py::class_<ChiakiLogWrapper>(m, "ChiakiLog")
        .def(py::init<uint32_t, std::function<void(ChiakiLogLevel, std::string, void *)>, void *>(),
             py::arg("level_mask") = CHIAKI_LOG_ALL & ~CHIAKI_LOG_INFO,
             py::arg("cb") = nullptr,
             py::arg("user") = nullptr)
        .def_property("level_mask", &ChiakiLogWrapper::get_level_mask, &ChiakiLogWrapper::set_level_mask)
        .def_property("cb", &ChiakiLogWrapper::get_cb, &ChiakiLogWrapper::set_cb)
        .def_property("user", &ChiakiLogWrapper::get_user, &ChiakiLogWrapper::set_user);

    m.def("chiaki_log_init", [](ChiakiLogWrapper &log, uint32_t level_mask, ChiakiLogCb cb, void *user)
        {
            chiaki_log_init(log.raw(), level_mask, cb, user);
        },
        py::arg("log"),
        py::arg("level_mask") = CHIAKI_LOG_ALL & ~CHIAKI_LOG_INFO,
        py::arg("cb") = nullptr,
        py::arg("user") = nullptr,
        "Initialize the log.");
    
    m.def("chiaki_log_set_level", [](ChiakiLogWrapper &log, uint32_t level_mask)
        {
            chiaki_log_set_level(log.raw(), level_mask);
        },
        py::arg("log"),
        py::arg("level_mask"),
        "Set the log level."
    );

    m.def("chiaki_log_cb_print", &chiaki_log_cb_print,
        py::arg("level"),
        py::arg("msg"),
        py::arg("user"),
        "Logs a message using chiaki_log_cb_print."
    );

    m.def("chiaki_log", [](ChiakiLogWrapper &log, ChiakiLogLevel log_level, const std::string &fmt)
        {
            chiaki_log(log.raw(), log_level, "%s", fmt.c_str());
        }, 
        py::arg("log"),
        py::arg("level"),
        py::arg("fmt"),
        "Logs a message using chiaki_log."
    );

    m.def("chiaki_log_hexdump", [](ChiakiLogWrapper &log, ChiakiLogLevel log_level, const uint8_t *buf, size_t buf_size)
        {
            chiaki_log_hexdump(log.raw(), log_level, buf, buf_size);
        },
        py::arg("log"),
        py::arg("level"),
        py::arg("buf"),
        py::arg("buf_size"),
        "Logs a hexdump using chiaki_log_hexdump."
    );

    m.def("chiaki_log_hexdump_raw", [](ChiakiLogWrapper &log, ChiakiLogLevel log_level, const uint8_t *buf, size_t buf_size)
        {
            chiaki_log_hexdump_raw(log.raw(), log_level, buf, buf_size);
        },
        py::arg("log"),
        py::arg("level"),
        py::arg("buf"),
        py::arg("buf_size"),
        "Logs a hexdump using chiaki_log_hexdump_raw."
    );
}