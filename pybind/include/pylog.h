#ifndef CHIAKI_PY_PYLOG_H
#define CHIAKI_PY_PYLOG_H

#include <optional>

#include <pybind11/pybind11.h>
#include <chiaki/log.h>

namespace py = pybind11;

// Look up the Python loggers the callbacks below write to. Call once from PYBIND11_MODULE.
void init_pylog();

// A ChiakiLogCbFunc that hands chiaki-ng's messages to the `chiaki_py.lib` Python logger. Safe to
// call from any thread; falls back to chiaki_log_cb_print while Python is shutting down.
void chiaki_log_cb_python(ChiakiLogLevel level, const char *msg, void *user);

// The same for libplacebo's messages (pl_log_level passed as int), to `chiaki_py.lib.placebo`.
void placebo_log_python(int pl_level, const char *msg);

// Releases the GIL for its lifetime if the calling thread holds it, and does nothing otherwise.
// For code that waits for chiaki's threads (joins them) and may be reached with or without the GIL,
// such as destructors: those threads take the GIL to log, so waiting with it held would deadlock.
class GilReleaseIfHeld
{
public:
    GilReleaseIfHeld()
    {
        if (Py_IsInitialized() && PyGILState_Check())
            release.emplace();
    }

private:
    std::optional<py::gil_scoped_release> release;
};

#endif // CHIAKI_PY_PYLOG_H
