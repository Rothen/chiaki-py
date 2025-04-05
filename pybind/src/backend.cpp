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

    py::enum_<PsnConnectState>(m, "PsnConnectState")
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

    py::class_<EventSource<ChiakiRegistEvent *>::Subscription>(m, "RegistEventSourceSubscription")
        .def("unsubscribe", &EventSource<ChiakiRegistEvent *>::Subscription::unsubscribe);

    py::class_<EventSource<ChiakiRegistEvent *>>(m, "RegistEventSource")
        .def("subscribe", &EventSource<ChiakiRegistEvent *>::subscribe,
             py::arg("on_next"),
             py::arg("on_error") = py::none(),
             py::arg("on_completed") = py::none(), py::return_value_policy::reference);

    py::class_<ChiakiRegisteredHost>(m, "RegisteredHost")
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

    py::enum_<ChiakiRegistEventType>(m, "RegistEventType")
        .value("FINISHED_CANCELED", ChiakiRegistEventType::CHIAKI_REGIST_EVENT_TYPE_FINISHED_CANCELED)
        .value("FINISHED_FAILED", ChiakiRegistEventType::CHIAKI_REGIST_EVENT_TYPE_FINISHED_FAILED)
        .value("FINISHED_SUCCESS", ChiakiRegistEventType::CHIAKI_REGIST_EVENT_TYPE_FINISHED_SUCCESS)
        .export_values();

    py::class_<ChiakiRegistEvent>(m, "RegistEvent")
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

    py::class_<RegistResult>(m, "RegistResult")
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
            std::ostringstream oss;
            for (size_t i = 0; i < sizeof(r.rp_key); ++i)
            {
                oss << std::hex << std::setw(2) << std::setfill('0')
                    << static_cast<int>(r.rp_key[i]);
            }
            return oss.str();
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

    py::class_<Backend>(m, "Backend")
        .def(py::init<Settings *>(), py::arg("settings"))
        .def("register_host_async", &Backend::registerHostAsync,
             py::arg("host"),
             py::arg("psn_id"),
             py::arg("pin"),
             py::arg("cpin"),
             py::arg("broadcast"),
             py::arg("target"), py::return_value_policy::reference)
        .def("register_host", &Backend::registerHost,
             py::arg("host"),
             py::arg("psn_id"),
             py::arg("pin"),
             py::arg("cpin"),
             py::arg("broadcast"),
             py::arg("target"), py::return_value_policy::reference);
}