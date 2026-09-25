#include "pylog.h"

#include <cstdio>
#include <cstring>

namespace
{
    // Owned references, deliberately never released: a static py::object would be decref'd after the
    // interpreter is gone.
    PyObject *chiaki_logger = nullptr;
    PyObject *placebo_logger = nullptr;

    bool python_is_finalizing()
    {
#if PY_VERSION_HEX >= 0x030D0000
        return Py_IsFinalizing();
#else
        return _Py_IsFinalizing();
#endif
    }

    // chiaki-ng's VERBOSE (ranked between its INFO and DEBUG) and libplacebo's TRACE both become DEBUG:
    // a library shouldn't add levels of its own to `logging`. Settings.set_log_level()/set_log_verbose()
    // and CHIAKI_PY_PLACEBO_LOG still tell them apart.
    int python_level(ChiakiLogLevel level)
    {
        switch (level)
        {
        case CHIAKI_LOG_ERROR:
            return 40;
        case CHIAKI_LOG_WARNING:
            return 30;
        case CHIAKI_LOG_INFO:
            return 20;
        default:
            return 10;
        }
    }

    int python_level_placebo(int pl_level)
    {
        static const int levels[] = {0, 50, 40, 30, 20, 10, 10}; // PL_LOG_NONE ... PL_LOG_TRACE
        return pl_level >= 0 && pl_level <= 6 ? levels[pl_level] : 10;
    }

    // Returns false if the message could not be handed to Python, which is then up to the caller to print.
    bool log_to_python(PyObject *logger, int level, const char *msg)
    {
        if (!logger || !Py_IsInitialized() || python_is_finalizing())
            return false;

        // chiaki-ng's own messages sometimes end in a newline; the logging handlers add their own.
        size_t length = std::strlen(msg);
        while (length > 0 && (msg[length - 1] == '\n' || msg[length - 1] == '\r'))
            length--;

        py::gil_scoped_acquire gil;
        try
        {
            py::handle handle(logger);
            if (!handle.attr("isEnabledFor")(level).cast<bool>())
                return true;
            // Not py::str(msg, length): that throws on the odd message that isn't valid UTF-8.
            auto text = py::reinterpret_steal<py::object>(PyUnicode_DecodeUTF8(msg, static_cast<Py_ssize_t>(length), "replace"));
            if (!text)
                throw py::error_already_set();
            handle.attr("log")(level, text);
        }
        catch (py::error_already_set &e)
        {
            e.discard_as_unraisable("chiaki_py log callback");
        }
        return true;
    }
} // namespace

void init_pylog()
{
    py::module_ logging = py::module_::import("logging");
    chiaki_logger = logging.attr("getLogger")("chiaki_py.lib").release().ptr();
    placebo_logger = logging.attr("getLogger")("chiaki_py.lib.placebo").release().ptr();
}

void chiaki_log_cb_python(ChiakiLogLevel level, const char *msg, void *user)
{
    if (!log_to_python(chiaki_logger, python_level(level), msg))
        chiaki_log_cb_print(level, msg, user);
}

void placebo_log_python(int pl_level, const char *msg)
{
    if (!log_to_python(placebo_logger, python_level_placebo(pl_level), msg))
        std::fprintf(stderr, "[libplacebo] %s\n", msg);
}
