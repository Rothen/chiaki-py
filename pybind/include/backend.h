#ifndef CHIAKI_PY_BACKEND_H
#define CHIAKI_PY_BACKEND_H

#include "chiakipysession.h"
#include "timer.h"
#include "host.h"
#include "discovery_manager.h"
#include "utils.h"
#include "event_source.h"
#include "pylog.h"

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
    Regist() {
        chiaki_log_init_python(&chiaki_log);
    }

    void start(const ChiakiRegistInfo &regist_info)
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

// What Backend.register_host() (the blocking variant) returns on success: the same fields as
// RegisteredHost, copied out of the underlying RegistEvent once registration completes.
struct RegistResult
{
    ChiakiRegistEventType type;
    ChiakiTarget target;
    std::string ap_ssid;
    std::string ap_bssid;
    std::string ap_key;
    std::string ap_name;
    // The console's MAC address, as 'aa:bb:cc:dd:ee:ff'.
    std::string server_mac;
    std::string server_nickname;
    std::string rp_regist_key;
    uint32_t rp_key_type;
    uint8_t rp_key[0x10]; // Bound as bytes by generate_bindings.py.
    uint32_t console_pin;

    RegistResult() = default;
    explicit RegistResult(const ChiakiRegistEvent &event);
};

// Stages of connecting to a console over PSN (remote play over the internet, via holepunching)
// rather than the local network. Exported for forward compatibility; no bound method currently
// returns one, since the PSN remote-connect path isn't wired up yet.
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

// Registers with a console: the one-time PIN pairing that gets back the registration key
// ChiakiPySession later needs to connect. `chiaki_py.register_host()` wraps register_host() with
// a pythonic result type (HostRegistration) and is the easier way to call this from Python.
class Backend
{
public:
    Backend() : regist()
    {
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

    // Start registering with `host` and return a RegistEventSource that reports the outcome
    // once subscribed to, instead of blocking. `psn_id` is the PSN account-ID (base64) for a
    // PS5 or a PS4 in 'PS4 8.0' mode, or the online ID for an older PS4; `pin` is the one-time
    // PIN shown on the console's registration screen, `cpin` its login PIN if it has one
    // (otherwise empty). Raises RuntimeError immediately if `psn_id` is not valid base64 of the
    // expected length for a PS5/PS4-8.0 `target`.
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
            regist.start(info);
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

    // Like register_host_async(), but blocks until registration finishes and returns the
    // RegistResult directly, or raises RuntimeError on failure.
    RegistResult registerHost(const std::string &host, const std::string &psn_id, const std::string &pin, const std::string &cpin, bool broadcast, ChiakiTarget target)
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

        std::promise<RegistResult> promise;
        std::future<RegistResult> future = promise.get_future();

        regist.setSuccessCallback([this, &promise](ChiakiRegistEvent *event) {
            result = RegistResult(*event);
            promise.set_value(result);
        });

        regist.setFailedCallback([&promise](int32_t error_code)
        {
            promise.set_exception(std::make_exception_ptr(std::runtime_error("Failed to register host")));
        });

        // Registration runs on chiaki's thread, which takes the GIL to log: wait for it without holding it.
        GilReleaseIfHeld release;
        regist.start(info);
        future.get();
        return result;
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

    ChiakiPySession *session = {};
    Timer *wakeup_start_timer = {};
    DiscoveryManager discovery_manager;
    Regist regist;
    EventSource<ChiakiRegistEvent *> event_source;
    std::vector<std::string> waking_sleeping_nicknames;
    std::string wakeup_nickname = "";
    RegistResult result;
    // bool wakeup_start = false;
};

#endif // CHIAKI_PY_BACKEND_H