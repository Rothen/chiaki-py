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

    py::class_<Backend>(m, "Backend")
        .def(py::init<Settings *>(), py::arg("settings"))
        .def("register_host", &Backend::registerHost,
             py::arg("host"),
             py::arg("psn_id"),
             py::arg("pin"),
             py::arg("cpin"),
             py::arg("broadcast"),
             py::arg("target"), py::return_value_policy::reference);
}