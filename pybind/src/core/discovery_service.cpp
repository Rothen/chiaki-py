#include "core/discovery_service.h"

void init_core_discovery_service(py::module &m)
{
    py::class_<OptionsWrapper>(m, "Options")
        .def_property("hosts_max", &OptionsWrapper::get_hosts_max, &OptionsWrapper::set_hosts_max)
        .def_property("host_drop_pings", &OptionsWrapper::get_host_drop_pings, &OptionsWrapper::set_host_drop_pings)
        .def_property("ping_ms", &OptionsWrapper::get_ping_ms, &OptionsWrapper::set_ping_ms)
        .def_property("ping_initial_ms", &OptionsWrapper::get_ping_initial_ms, &OptionsWrapper::set_ping_initial_ms)
        .def_property("send_addr", &OptionsWrapper::get_send_addr, &OptionsWrapper::set_send_addr)
        .def_property("send_addr_size", &OptionsWrapper::get_send_addr_size, &OptionsWrapper::set_send_addr_size)
        .def_property("broadcast_addrs", &OptionsWrapper::get_broadcast_addrs, &OptionsWrapper::set_broadcast_addrs)
        .def_property("broadcast_num", &OptionsWrapper::get_broadcast_num, &OptionsWrapper::set_broadcast_num)
        .def_property("send_host", &OptionsWrapper::get_send_host, &OptionsWrapper::set_send_host);
}