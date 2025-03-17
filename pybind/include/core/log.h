#ifndef CHIAKI_PY_CORE_LOG_H
#define CHIAKI_PY_CORE_LOG_H

#include "core/struct_wrapper.h"
#include <chiaki/log.h>

#include <string>
#include <functional>

#include <pybind11/pybind11.h>

namespace py = pybind11;

void init_core_log(py::module &m);

class ChiakiLogWrapper : public StructWrapper<ChiakiLog>
{
private:
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
        wrapped().level_mask = level_mask;
        wrapped().cb = LogCallback;
        wrapped().user = user;
    }

    void set_level_mask(uint32_t level_mask)
    {
        wrapped().level_mask = level_mask;
    }
    uint32_t get_level_mask() { return wrapped().level_mask; }

    void get_cb(std::function<void(ChiakiLogLevel, std::string, void *)> cb) { callback = cb; }
    std::function<void(ChiakiLogLevel, std::string, void *)> set_cb() { return callback; }

    void set_user(void *user) { wrapped().user = user; }
    void *get_user() { return wrapped().user; }
};

#endif // CHIAKI_PY_CORE_LOG_H