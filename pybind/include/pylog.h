#ifndef CHIAKI_PY_PYLOG_H
#define CHIAKI_PY_PYLOG_H

#include <cstdarg>
#include <optional>

#include <pybind11/pybind11.h>
#include <chiaki/log.h>

namespace py = pybind11;

// Look up the Python loggers the callbacks below write to. Call once from PYBIND11_MODULE.
void init_pylog();

// A ChiakiLogCbFunc that hands chiaki-ng's messages to the `chiaki_py.lib` Python logger. Safe to
// call from any thread; falls back to chiaki_log_cb_print while Python is shutting down.
void chiaki_log_cb_python(ChiakiLogLevel level, const char *msg, void *user);

// Initializes log to hand its messages to chiaki_log_cb_python. level_mask is checked by chiaki-ng before a
// message is even formatted, the `chiaki_py.lib` logger's level after that; the default leaves out VERBOSE.
// Pass a narrower mask only where the messages are frequent enough for formatting them to cost something.
inline void chiaki_log_init_python(ChiakiLog *log, uint32_t level_mask = CHIAKI_LOG_ALL & ~CHIAKI_LOG_VERBOSE)
{
    chiaki_log_init(log, level_mask, chiaki_log_cb_python, nullptr);
}

// The same for libplacebo's messages (pl_log_level passed as int), to `chiaki_py.lib.placebo`.
void placebo_log_python(int pl_level, const char *msg);

// An av_log callback (see av_log_set_callback, which init_pylog() installs) that hands FFmpeg's messages to
// `chiaki_py.lib.ffmpeg`. Messages above av_log_get_level() (AV_LOG_INFO by default) are dropped first.
void ffmpeg_log_python(void *avcl, int av_level, const char *fmt, va_list vl);

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
