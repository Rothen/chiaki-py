#ifndef CHIAKI_PY_BACKEND_H
#define CHIAKI_PY_BACKEND_H

#include "settings.h"
#include "streamsession.h"
#include "timer.h"
#include "host.h"
#include "discovery_manager.h"
#include "utils.h"
#include "core/common.h"
#include "event_source.h"

#include <chiaki/discovery.h>

#include <mutex>
#include <variant>
#include <vector>
#include <string>
#include <functional>
#include <any>
#include <algorithm>
#include <cstdlib>
#include <future>

void init_backend(py::module &m);

#define PSN_DEVICES_TRIES 2
#define MAX_PSN_RECONNECT_TRIES 6
#define PSN_INTERNET_WAIT_SECONDS 5
#define WAKEUP_PSN_IGNORE_SECONDS 10
#define WAKEUP_WAIT_SECONDS 25

static std::mutex chiaki_log_mutex;
static ChiakiLog *chiaki_log_ctx = nullptr;

using HostVariantMap = std::variant<HostMAC, HiddenHost, RegisteredHost, ManualHost, PsnHost>;
using FailedCallback = std::function<void(int32_t)>;
using SuccessCallback = std::function<void(ChiakiRegistEvent *)>;

class Regist
{
public:
    Regist(uint32_t log_mask) {
        chiaki_log_init(&chiaki_log, log_mask, &Regist::log_cb, this);
    }

    void start(const ChiakiRegistInfo &regist_info, uint32_t log_mask)
    {
        chiaki_regist_start(&chiaki_regist, &chiaki_log, &regist_info, &Regist::regist_cb, this);
    }

    void setSuccessCallback(SuccessCallback successCallback)
    {
        this->successCallback = successCallback;
    }

    void setFailedCallback(FailedCallback failedCallback)
    {
        this->failedCallback = failedCallback;
    }

private:
    SuccessCallback successCallback;
    FailedCallback failedCallback;

    static void log_cb(ChiakiLogLevel level, const char *msg, void *user)
    {
        chiaki_log_cb_print(level, msg, user);
    }

    static void regist_cb(ChiakiRegistEvent *event, void *user)
    {
        auto *self = static_cast<Regist *>(user);
        if (event->type == CHIAKI_REGIST_EVENT_TYPE_FINISHED_FAILED)
        {
            if (self->failedCallback)
            {
                self->failedCallback(CHIAKI_REGIST_EVENT_TYPE_FINISHED_FAILED);
            }
        }
        else
        {
            if (self->successCallback)
            {
                self->successCallback(event);
            }
        }
    }

    ChiakiLog chiaki_log;
    ChiakiRegist chiaki_regist;
};

enum class PsnConnectState
{
    NotStarted,
    WaitingForInternet,
    InitiatingConnection,
    LinkingConsole,
    RegisteringConsole,
    RegistrationFinished,
    DataConnectionStart,
    DataConnectionFinished,
    ConnectFailed,
    ConnectFailedStart,
    ConnectFailedConsoleUnreachable,
};

class Backend
{
public:
    Backend(Settings *settings) : settings(settings), regist(settings->GetLogLevelMask())
    {
        discovery_manager.SetSettings(settings);
        wakeup_start_timer = new Timer();
    }

    ~Backend()
    {
        if (session)
        {
            std::lock_guard<std::mutex> lock(chiaki_log_mutex);
            chiaki_log_ctx = nullptr;
            delete session;
            session = nullptr;
        }
    }

    EventSource<ChiakiRegistEvent *> &registerHostAsync(const std::string &host, const std::string &psn_id, const std::string &pin, const std::string &cpin, bool broadcast, ChiakiTarget target)
    {
        ChiakiRegistInfo info = {};

        info.host = host.data();
        info.target = target;
        info.broadcast = broadcast;
        info.pin = static_cast<uint32_t>(std::stoul(pin));
        info.console_pin = (cpin.size() > 0) ? static_cast<uint32_t>(std::stoul(cpin)) : 0;
        info.holepunch_info = nullptr;
        info.rudp = nullptr;
        std::string psn_idb;
        if (target == CHIAKI_TARGET_PS4_8)
        {
            psn_idb = psn_id;
            info.psn_online_id = psn_idb.data();
        }
        else
        {
            std::vector<uint8_t> account_id = fromBase64(psn_id);
            if (account_id.size() != CHIAKI_PSN_ACCOUNT_ID_SIZE)
            {
                throw std::runtime_error("Invalid Account-ID: The PSN Account-ID must be exactly " + std::to_string(CHIAKI_PSN_ACCOUNT_ID_SIZE) + " bytes encoded as base64.");
            }
            info.psn_online_id = nullptr;
            memcpy(info.psn_account_id, account_id.data(), CHIAKI_PSN_ACCOUNT_ID_SIZE);
        }

        event_source = EventSource<ChiakiRegistEvent *>();

        event_source.set_on_subscribe([this, info]() mutable {
            regist.start(info, settings->GetLogLevelMask());
        });

        regist.setSuccessCallback([this](ChiakiRegistEvent *event) {
            event_source.next(event);
            event_source.completed();
        });

        regist.setFailedCallback([this](int32_t error_code) {
            event_source.error(error_code, "Failed to register host");
            event_source.completed();
        });

        return event_source;
    }

    ChiakiRegistEvent *registerHost(const std::string &host, const std::string &psn_id, const std::string &pin, const std::string &cpin, bool broadcast, ChiakiTarget target)
    {
        ChiakiRegistInfo info = {};

        info.host = host.data();
        info.target = target;
        info.broadcast = broadcast;
        info.pin = static_cast<uint32_t>(std::stoul(pin));
        info.console_pin = (cpin.size() > 0) ? static_cast<uint32_t>(std::stoul(cpin)) : 0;
        info.holepunch_info = nullptr;
        info.rudp = nullptr;
        std::string psn_idb;
        if (target == CHIAKI_TARGET_PS4_8)
        {
            psn_idb = psn_id;
            info.psn_online_id = psn_idb.data();
        }
        else
        {
            std::vector<uint8_t> account_id = fromBase64(psn_id);
            if (account_id.size() != CHIAKI_PSN_ACCOUNT_ID_SIZE)
            {
                throw std::runtime_error("Invalid Account-ID: The PSN Account-ID must be exactly " + std::to_string(CHIAKI_PSN_ACCOUNT_ID_SIZE) + " bytes encoded as base64.");
            }
            info.psn_online_id = nullptr;
            memcpy(info.psn_account_id, account_id.data(), CHIAKI_PSN_ACCOUNT_ID_SIZE);
        }

        std::promise<ChiakiRegistEvent *> promise;
        std::future<ChiakiRegistEvent *> future = promise.get_future();

        regist.setSuccessCallback([&promise](ChiakiRegistEvent *event)
        {
            promise.set_value(event);
        });

        regist.setFailedCallback([&promise](int32_t error_code)
        {
            promise.set_exception(std::make_exception_ptr(std::runtime_error("Failed to register host")));
        });

        regist.start(info, settings->GetLogLevelMask());
        return future.get();
    }

private:
    bool sendWakeup(const std::string &host, const std::string &regist_key, bool ps5)
    {
        try
        {
            discovery_manager.SendWakeup(host, regist_key, ps5);
            return true;
        }
        catch (const Exception &e)
        {
            // emit error(tr("Wakeup failed"), tr("Failed to send Wakeup packet:\n%1").arg(e.what()));
            return false;
        }
    }
    // void updateDiscoveryHosts();

    Settings *settings = {};
    StreamSession *session = {};
    Timer *wakeup_start_timer = {};
    DiscoveryManager discovery_manager;
    Regist regist;
    EventSource<ChiakiRegistEvent *> event_source;
    std::vector<std::string> waking_sleeping_nicknames;
    std::string wakeup_nickname = "";
    // bool wakeup_start = false;
};

#endif // CHIAKI_PY_BACKEND_H