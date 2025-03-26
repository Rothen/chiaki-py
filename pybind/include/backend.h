#ifndef CHIAKI_PY_BACKEND_H
#define CHIAKI_PY_BACKEND_H

#include "settings.h"
#include "streamsession.h"
#include "timer.h"
#include "host.h"
#include "discovery_manager.h"
#include "utils.h"
#include "core/common.h"

#include <chiaki/discovery.h>

#include <mutex>
#include <variant>
#include <vector>
#include <string>
#include <functional>
#include <any>
#include <algorithm>
#include <cstdlib>

void init_backend(py::module &m);

#define PSN_DEVICES_TRIES 2
#define MAX_PSN_RECONNECT_TRIES 6
#define PSN_INTERNET_WAIT_SECONDS 5
#define WAKEUP_PSN_IGNORE_SECONDS 10
#define WAKEUP_WAIT_SECONDS 25

static std::mutex chiaki_log_mutex;
static ChiakiLog *chiaki_log_ctx = nullptr;

using HostVariantMap = std::variant<HostMAC, HiddenHost, RegisteredHost, ManualHost, PsnHost>;

class Regist
{
public:
    Regist(const ChiakiRegistInfo &regist_info, uint32_t log_mask)
    {
        // Constructor logic here, such as initializing `chiaki_log` and `chiaki_regist`
    }

    void log(ChiakiLogLevel level, const std::string &msg)
    {
        if (log_callback_)
        {
            log_callback_(level, msg);
        }
    }

    void failed()
    {
        if (failed_callback_)
        {
            failed_callback_();
        }
    }

    void success(const RegisteredHost &host)
    {
        if (success_callback_)
        {
            success_callback_(host);
        }
    }

    void setLogCallback(std::function<void(ChiakiLogLevel, const std::string &)> callback)
    {
        log_callback_ = callback;
    }

    void setFailedCallback(std::function<void()> callback)
    {
        failed_callback_ = callback;
    }

    void setSuccessCallback(std::function<void(const RegisteredHost &)> callback)
    {
        success_callback_ = callback;
    }

private:
    std::function<void(ChiakiLogLevel, const std::string &)> log_callback_;
    std::function<void()> failed_callback_;
    std::function<void(const RegisteredHost &)> success_callback_;

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

    Backend(Settings *settings) : settings(settings)
    {
        const char *uri = "org.streetpea.chiaking";

        setConnectState(PsnConnectState::NotStarted);

        /*connect(settings, &Settings::RegisteredHostsUpdated, this, &QmlBackend::hostsChanged);
        connect(settings, &Settings::HiddenHostsUpdated, this, &QmlBackend::hiddenHostsChanged);
        connect(settings, &Settings::ManualHostsUpdated, this, &QmlBackend::hostsChanged);
        connect(settings, &Settings::CurrentProfileChanged, this, &QmlBackend::profileChanged);
        connect(&discovery_manager, &DiscoveryManager::HostsUpdated, this, &QmlBackend::updateDiscoveryHosts);*/
        discovery_manager.SetSettings(settings);
        setDiscoveryEnabled(true);

        wakeup_start_timer = new Timer();
        // wakeup_start_timer->setSingleShot(true);
        // wakeup_start_timer->
        // connect(wakeup_start_timer, &QTimer::timeout, this, [this] { emit wakeupStartFailed(); });
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

    StreamSession *qmlSession() const { return session; }

    bool discoveryEnabled() const { return discovery_manager.GetActive(); };
    void setDiscoveryEnabled(bool enabled) { discovery_manager.SetActive(enabled); /*emit*/ discoveryEnabledChanged(); }

    std::vector<std::map<std::string, std::any>> hosts() const
    {
        std::vector<std::map<std::string, std::any>> out;
        std::vector<std::string> discovered_nicknames;
        std::vector<ManualHost> discovered_manual_hosts;
        size_t registered_discovered_ps4s = 0;

        for (const auto &host : discovery_manager.GetHosts())
        {
            std::map<std::string, std::any> m;
            HostMAC host_mac = host.GetHostMAC();
            bool registered = settings->GetRegisteredHostRegistered(host_mac);
            
            m["discovered"] = true;
            m["manual"] = false;
            m["name"] = host->host_name;
            std::string duid("");
            m["duid"] = duid;
            m["address"] = host->host_addr;
            m["ps5"] = host->ps5;
            m["mac"] = host_mac.ToString();
            m["state"] = chiaki_discovery_host_state_string(host->state);
            m["app"] = host->running_app_name;
            m["titleId"] = host->running_app_titleid;
            m["registered"] = registered;
            m["display"] = true;
            discovered_nicknames.push_back(host->host_name);
            out.push_back(m);
            if (!host->ps5 && registered)
            {
                registered_discovered_ps4s++;
            }
        }
        return out;
    }

    void finishAutoRegister(const ChiakiRegisteredHost &host)
    {   
        std::string nickname(host.server_nickname);
        if(!regist_dialog_server.discovery_host.ps5 && regist_dialog_server.discovery_host.host_name != nickname)
        {
            // error(tr("PS4 Console Not Main"), tr("Can't proceed...%1 is not your main PS4 console in PSN").arg(regist_dialog_server.discovery_host.host_name));
            return;
        }
        setConnectState(PsnConnectState::RegistrationFinished);
    }

    // void createSession(const StreamSessionConnectInfo &connect_info);

    // bool closeRequested();

    // void setIsAppActive();

    // void profileChanged();

    // void addManualHost(int index, const std::string &address);
    bool registerHost(const std::string &host, const std::string &psn_id, const std::string &pin, const std::string &cpin, bool broadcast, ChiakiTarget target, const std::function<void(/*optional args*/)> &callback)
    {
        ChiakiRegistInfo info = {};

        info.host = host.data();
        info.target = target;
        info.broadcast = broadcast;
        info.pin = static_cast<uint32_t>(std::stoul(pin));
        info.console_pin = static_cast<uint32_t>(std::stoul(cpin));
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
                // emit error(tr("Invalid Account-ID"), tr("The PSN Account-ID must be exactly %1 bytes encoded as base64.").arg(CHIAKI_PSN_ACCOUNT_ID_SIZE));
                return false;
            }
            info.psn_online_id = nullptr;
            memcpy(info.psn_account_id, account_id.data(), CHIAKI_PSN_ACCOUNT_ID_SIZE);
        }

        Regist regist(info, settings->GetLogLevelMask());

        // Set the callback for logging
        regist.setLogCallback([](ChiakiLogLevel level, const std::string &msg) {
            std::string logMessage = "[" + std::string(1, chiaki_log_level_char(level)) + "] " + msg;
            std::cout << logMessage << std::endl;
        });

        // Set the callback for failed
        regist.setFailedCallback([&]() {
            std::cout << "Failed callback triggered" << std::endl;
        });

        // Set the callback for success
        regist.setSuccessCallback([&](const RegisteredHost &rhost) {
            std::cout << "Success callback triggered with host" << std::endl;
            // Additional logic related to `host`

            // settings->AddRegisteredHost(rhost);
            if(regist_dialog_server.discovered == false)
            {
                ManualHost manual_host = regist_dialog_server.manual_host;
                if(manual_host.GetHost().empty())
                    manual_host.SetHost(host);
                manual_host.Register(rhost);
                // settings->SetManualHost(manual_host);
            }
        });

        return true;
    }
    
    // Signals
    void connectStateChanged() { };
    void discoveryEnabledChanged() { };
    void hostsChanged() { };
    void hiddenHostsChanged() { };
    void wakeupStartInitiated() { };
    void wakeupStartFailed() { };

    void sessionPinDialogRequested() { };
    void sessionStopDialogRequested() { };
    void registDialogRequested(const std::string &host, bool ps5, const std::string &duid) { };

    void setConnectState(PsnConnectState connect_state)
    {
        connectStateChanged();
    }

private:
    struct DisplayServer
    {
        bool valid = false;

        DiscoveryHost discovery_host;
        ManualHost manual_host;
        bool discovered;

        PsnHost psn_host;
        std::string duid;
        RegisteredHost registered_host;
        bool registered;

        std::string GetHostAddr() const { return discovered ? discovery_host.host_addr : manual_host.GetHost(); }
        bool IsPS5() const { return discovered ? discovery_host.ps5 : (registered ? chiaki_target_is_ps5(registered_host.GetTarget()) : true); }
    };

    bool sendWakeup(const DisplayServer &server)
    {
        if (!server.registered)
            return false;
        return sendWakeup(server.GetHostAddr(), server.registered_host.GetRPRegistKey(), server.IsPS5());
    }

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
    std::vector<std::string> waking_sleeping_nicknames;
    DisplayServer regist_dialog_server;
    std::string wakeup_nickname = "";
    bool wakeup_start = false;
};

#endif // CHIAKI_PY_BACKEND_H