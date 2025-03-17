#ifndef CHIAKI_PY_CORE_LOG_H
#define CHIAKI_PY_CORE_LOG_H

#include "core/struct_wrapper.h"
#include <chiaki/log.h>

#include <string>
#include <functional>

#include <pybind11/pybind11.h>

namespace py = pybind11;

void init_core_log(py::module &m);

typedef std::function<void(ChiakiLogLevel, std::string, void *)> ChiakiLogCbFunc;

class LogWrapper : public StructWrapper<ChiakiLog>
{
private:
    ChiakiLogLevel level;
    ChiakiLogCbFunc callback;

    static void LogCallback(ChiakiLogLevel level, const char *msg, void *user)
    {
        auto *wrapper = static_cast<LogWrapper *>(user);
        if (wrapper->callback)
        {
            wrapper->callback(level, msg, user);
        }
    }

public:
    using StructWrapper::StructWrapper;

    LogWrapper(ChiakiLogLevel level, ChiakiLogCbFunc cb, void *user) : StructWrapper(), level(level), callback(cb)
    {
        chiaki_log_init(wrapped_ptr(), CHIAKI_LOG_ALL & ~level, LogCallback, user);
    }

    // Static methods
    static char level_char(ChiakiLogLevel level) { return chiaki_log_level_char(level); }

    // Getters / Setters
    void set_level(ChiakiLogLevel level) { this->level = level; chiaki_log_set_level(wrapped_ptr(), CHIAKI_LOG_ALL & ~level); }
    ChiakiLogLevel get_level() { return level; }

    void set_cb(ChiakiLogCbFunc cb) { callback = cb; }
    ChiakiLogCbFunc get_cb() { return callback; }

    void set_user(void *user) { wrapped().user = user; }
    void *get_user() { return wrapped().user; }

    // Chiaki log functions
    void cb_print(ChiakiLogLevel level, const std::string &msg, void *user) { chiaki_log_cb_print(level, msg.data(), user); }

    void log(ChiakiLogLevel level, const std::string &msg) { chiaki_log(wrapped_ptr(), level, "%s", msg.data()); }

    void log_hexdump(ChiakiLogLevel level, const std::vector<uint8_t> &buf) { chiaki_log_hexdump(wrapped_ptr(), level, buf.data(), buf.size()); }

    void log_hexdump_raw(ChiakiLogLevel level, const std::vector<uint8_t> &buf) { chiaki_log_hexdump_raw(wrapped_ptr(), level, buf.data(), buf.size()); }

    void debug(const std::string &msg) { CHIAKI_LOGD(wrapped_ptr(), "%s", msg.data()); }

    void verbose(const std::string &msg) { CHIAKI_LOGV(wrapped_ptr(), "%s", msg.data()); }

    void info(const std::string &msg) { CHIAKI_LOGI(wrapped_ptr(), "%s", msg.data()); }

    void warning(const std::string &msg) { CHIAKI_LOGW(wrapped_ptr(), "%s", msg.data()); }

    void error(const std::string &msg) { CHIAKI_LOGE(wrapped_ptr(), "%s", msg.data()); }
};

class LogSnifferWrapper : public StructWrapper<ChiakiLogSniffer>
{
private:
    ChiakiLogLevel sniff_level;

public:
    using StructWrapper::StructWrapper;

    LogSnifferWrapper(ChiakiLogLevel sniff_level, LogWrapper &forward_log) : StructWrapper(), sniff_level(sniff_level)
    {
        chiaki_log_sniffer_init(wrapped_ptr(), CHIAKI_LOG_ALL & ~sniff_level, forward_log.wrapped_ptr());
    }

    ~LogSnifferWrapper()
    {
        chiaki_log_sniffer_fini(wrapped_ptr());
    }

    // Getters / Setters
    void set_forward_log(LogWrapper &forward_log) { wrapped().forward_log = forward_log.wrapped_ptr(); }
    LogWrapper get_forward_log() { return LogWrapper(*wrapped().forward_log); }

    void set_sniff_log(LogWrapper &sniff_log) { wrapped().sniff_log = *sniff_log.wrapped_ptr(); }
    LogWrapper get_sniff_log() { return LogWrapper(wrapped().sniff_log); }

    void set_sniff_level(ChiakiLogLevel sniff_level) { this->sniff_level = sniff_level; wrapped().sniff_level_mask = CHIAKI_LOG_ALL & ~sniff_level; }
    ChiakiLogLevel get_sniff_level() { return sniff_level; }

    void set_buf(std::vector<char> &buf) { wrapped().buf = buf.data(); wrapped().buf_len = buf.size(); }
    std::vector<char> get_buf() { return std::vector<char>(wrapped().buf, wrapped().buf + wrapped().buf_len); }
};

#endif // CHIAKI_PY_CORE_LOG_H