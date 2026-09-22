#include "backend.h"
#include "settings.h"
#include "streamsession.h"
#include "timer.h"
#include "host.h"
#include "discovery_manager.h"

#include <chiaki/discovery.h>

#include <variant>
#include <vector>
#include <string>
#include <functional>

void init_backend(py::module &m)
{
    m.attr("PSN_DEVICES_TRIES") = PSN_DEVICES_TRIES;
    m.attr("MAX_PSN_RECONNECT_TRIES") = MAX_PSN_RECONNECT_TRIES;
    m.attr("PSN_INTERNET_WAIT_SECONDS") = PSN_INTERNET_WAIT_SECONDS;
    m.attr("WAKEUP_PSN_IGNORE_SECONDS") = WAKEUP_PSN_IGNORE_SECONDS;
    m.attr("WAKEUP_WAIT_SECONDS") = WAKEUP_WAIT_SECONDS;

    py::enum_<PsnConnectState>(m, "PsnConnectState",
        "Stages of connecting to a console over PSN (remote play over the internet, via holepunching) "
        "rather than the local network. Exported for forward compatibility; no bound method currently "
        "returns one, since the PSN remote-connect path (core/remote/holepunch.h) isn't wired up yet.")
        .value("NotStarted", PsnConnectState::NotStarted)
        .value("WaitingForInternet", PsnConnectState::WaitingForInternet)
        .value("InitiatingConnection", PsnConnectState::InitiatingConnection)
        .value("LinkingConsole", PsnConnectState::LinkingConsole)
        .value("RegisteringConsole", PsnConnectState::RegisteringConsole)
        .value("RegistrationFinished", PsnConnectState::RegistrationFinished)
        .value("DataConnectionStart", PsnConnectState::DataConnectionStart)
        .value("DataConnectionFinished", PsnConnectState::DataConnectionFinished)
        .value("ConnectFailed", PsnConnectState::ConnectFailed)
        .value("ConnectFailedStart", PsnConnectState::ConnectFailedStart)
        .value("ConnectFailedConsoleUnreachable", PsnConnectState::ConnectFailedConsoleUnreachable)
        .export_values();

    py::class_<EventSource<ChiakiRegistEvent *>::Subscription>(m, "RegistEventSourceSubscription",
        "Returned by RegistEventSource.subscribe(); call unsubscribe() to stop receiving callbacks.")
        .def("unsubscribe", &EventSource<ChiakiRegistEvent *>::Subscription::unsubscribe);

    py::class_<EventSource<ChiakiRegistEvent *>>(m, "RegistEventSource",
        "What Backend.register_host_async() returns: a one-shot stream of registration progress that "
        "calls `on_next` with the final RegistEvent once registration succeeds, `on_error` if it fails, "
        "and `on_completed` in either case.")
        .def("subscribe", &EventSource<ChiakiRegistEvent *>::subscribe,
             py::arg("on_next"),
             py::arg("on_error") = py::none(),
             py::arg("on_completed") = py::none(), py::return_value_policy::reference,
             "Register callbacks for this registration attempt and return a subscription that can be "
             "unsubscribe()'d. Registration only actually starts once the first subscriber attaches.");

    py::class_<ChiakiRegisteredHost>(m, "RegisteredHost",
        "The result of a successful registration, as delivered through RegistEvent.registered_host by "
        "Backend.register_host_async(). Backend.register_host() (the blocking variant) returns the "
        "equivalent information as a RegistResult instead.")
        .def_readonly("target", &ChiakiRegisteredHost::target)
        .def_readonly("ap_ssid", &ChiakiRegisteredHost::ap_ssid)
        .def_readonly("ap_bssid", &ChiakiRegisteredHost::ap_bssid)
        .def_readonly("ap_key", &ChiakiRegisteredHost::ap_key)
        .def_readonly("ap_name", &ChiakiRegisteredHost::ap_name)
        .def_readonly("server_mac", &ChiakiRegisteredHost::server_mac)
        .def_readonly("server_nickname", &ChiakiRegisteredHost::server_nickname)
        .def_readonly("rp_regist_key", &ChiakiRegisteredHost::rp_regist_key)
        .def_readonly("rp_key_type", &ChiakiRegisteredHost::rp_key_type)
        .def_readonly("rp_key", &ChiakiRegisteredHost::rp_key)
        .def_readonly("console_pin", &ChiakiRegisteredHost::console_pin)
        .def("__repr__",
             [](const ChiakiRegisteredHost &host)
             {
                 std::ostringstream mac_ss;
                 mac_ss << std::hex << std::setfill('0');
                 for (int i = 0; i < 6; ++i)
                 {
                     mac_ss << std::setw(2) << static_cast<int>(host.server_mac[i]);
                     if (i < 5)
                         mac_ss << ":";
                 }

                 return "<RegisteredHost server_nickname='" + std::string(host.server_nickname) +
                        "' ap_name='" + std::string(host.ap_name) +
                        "' ap_ssid='" + std::string(host.ap_ssid) +
                        "' server_mac='" + mac_ss.str() +
                        "' console_pin=" + std::to_string(host.console_pin) +
                        ">";
             });

    py::enum_<ChiakiRegistEventType>(m, "RegistEventType",
        "How a Backend.register_host_async() attempt ended: FINISHED_SUCCESS (registered_host is set), "
        "FINISHED_FAILED (wrong PIN, console unreachable, ...) or FINISHED_CANCELED.")
        .value("FINISHED_CANCELED", ChiakiRegistEventType::CHIAKI_REGIST_EVENT_TYPE_FINISHED_CANCELED)
        .value("FINISHED_FAILED", ChiakiRegistEventType::CHIAKI_REGIST_EVENT_TYPE_FINISHED_FAILED)
        .value("FINISHED_SUCCESS", ChiakiRegistEventType::CHIAKI_REGIST_EVENT_TYPE_FINISHED_SUCCESS)
        .export_values();

    py::class_<ChiakiRegistEvent>(m, "RegistEvent",
        "Delivered to Backend.register_host_async()'s `on_next` callback once registration finishes; "
        "`type` says how (see RegistEventType), and `registered_host` carries the result if it succeeded.")
        .def_readonly("type", &ChiakiRegistEvent::type)
        .def_readonly("registered_host", &ChiakiRegistEvent::registered_host)
        .def("__repr__",
             [](const ChiakiRegistEvent &e)
             {
                 std::ostringstream ss;
                 ss << "<RegistEvent type=" << e.type
                    << " registered_host=" << e.registered_host << ">";
                 return ss.str();
             });

    py::class_<RegistResult>(m, "RegistResult",
        "What Backend.register_host() (the blocking variant) returns on success: the same fields as "
        "RegisteredHost, copied out of the underlying RegistEvent once registration completes.")
        .def_readonly("type", &RegistResult::type)
        .def_readonly("target", &RegistResult::target)
        .def_readonly("ap_ssid", &RegistResult::ap_ssid)
        .def_readonly("ap_bssid", &RegistResult::ap_bssid)
        .def_readonly("ap_key", &RegistResult::ap_key)
        .def_readonly("ap_name", &RegistResult::ap_name)
        .def_property_readonly("server_mac", [](const RegistResult &r) {
            std::ostringstream oss;
            for (size_t i = 0; i < sizeof(r.server_mac); ++i)
            {
                oss << std::hex << std::setw(2) << std::setfill('0')
                    << static_cast<int>(r.server_mac[i]);
                if (i < sizeof(r.server_mac) - 1)
                    oss << ":";
            }
            return oss.str();
        })
        .def_readonly("server_nickname", &RegistResult::server_nickname)
        .def_readonly("rp_regist_key", &RegistResult::rp_regist_key)
        .def_readonly("rp_key_type", &RegistResult::rp_key_type)
        .def_property_readonly("rp_key", [](const RegistResult &r) {
            return py::bytes(reinterpret_cast<const char *>(r.rp_key), sizeof(r.rp_key));
        })
        .def_readonly("console_pin", &RegistResult::console_pin)
        .def("__repr__", [](const RegistResult &r)
             {
                py::object type_obj = py::cast(r.type);
                py::object target_obj = py::cast(r.target);

                std::ostringstream oss;
                oss << "<RegistResult"
                    << " type=" << py::str(type_obj)
                    << " target=" << py::str(target_obj)
                    << " ap_ssid='" << r.ap_ssid << "'"
                    << " ap_bssid='" << r.ap_bssid << "'"
                    << " ap_key='" << r.ap_key << "'"
                    << " ap_name='" << r.ap_name << "'"
                    << " server_mac=" << std::hex
                    << std::setfill('0')
                    << std::setw(2) << static_cast<int>(r.server_mac[0]) << ":"
                    << std::setw(2) << static_cast<int>(r.server_mac[1]) << ":"
                    << std::setw(2) << static_cast<int>(r.server_mac[2]) << ":"
                    << std::setw(2) << static_cast<int>(r.server_mac[3]) << ":"
                    << std::setw(2) << static_cast<int>(r.server_mac[4]) << ":"
                    << std::setw(2) << static_cast<int>(r.server_mac[5]) << std::dec
                    << " server_nickname='" << r.server_nickname << "'"
                    << " rp_regist_key='" << py::bytes(reinterpret_cast<const char *>(r.rp_regist_key), CHIAKI_SESSION_AUTH_SIZE).cast<std::string>() << "'"
                    << " rp_key='"
                    << ([](const uint8_t *key) {
                        std::ostringstream oss;
                        for (int i = 0; i < 0x10; ++i)
                            oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(key[i]);
                        return oss.str();
                    })(r.rp_key)
                    << " rp_key_type=" << r.rp_key_type
                    << " console_pin=" << r.console_pin
                    << ">";
                return oss.str();
            });

    py::class_<Backend>(m, "Backend",
        "Registers with a console: the one-time PIN pairing that gets back the registration key "
        "StreamSession later needs to connect. `chiaki_py.register_host()` wraps register_host() with "
        "a pythonic result type (HostRegistration) and is the easier way to call this from Python.")
        .def(py::init<Settings *>(), py::arg("settings"))
        .def("register_host_async", &Backend::registerHostAsync,
             py::arg("host"),
             py::arg("psn_id"),
             py::arg("pin"),
             py::arg("cpin"),
             py::arg("broadcast"),
             py::arg("target"), py::return_value_policy::reference,
             "Start registering with `host` and return a RegistEventSource that reports the outcome "
             "once subscribed to, instead of blocking. `psn_id` is the PSN account-ID (base64) for a "
             "PS5 or a PS4 in 'PS4 8.0' mode, or the online ID for an older PS4; `pin` is the one-time "
             "PIN shown on the console's registration screen, `cpin` its login PIN if it has one "
             "(otherwise empty). Raises RuntimeError immediately if `psn_id` is not valid base64 of the "
             "expected length for a PS5/PS4-8.0 `target`.")
        .def("register_host", &Backend::registerHost,
             py::arg("host"),
             py::arg("psn_id"),
             py::arg("pin"),
             py::arg("cpin"),
             py::arg("broadcast"),
             py::arg("target"), py::return_value_policy::copy,
             "Like register_host_async(), but blocks until registration finishes and returns the "
             "RegistResult directly, or raises RuntimeError on failure.");
}