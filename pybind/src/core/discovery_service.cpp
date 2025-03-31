#include "core/discovery_service.h"

void init_core_discovery_service(py::module &m)
{
    py::class_<DiscoveryServiceOptionsWrapper>(m, "DiscoveryServiceOptions")
        .def_property("hosts_max", &DiscoveryServiceOptionsWrapper::get_hosts_max, &DiscoveryServiceOptionsWrapper::set_hosts_max)
        .def_property("host_drop_pings", &DiscoveryServiceOptionsWrapper::get_host_drop_pings, &DiscoveryServiceOptionsWrapper::set_host_drop_pings)
        .def_property("ping_ms", &DiscoveryServiceOptionsWrapper::get_ping_ms, &DiscoveryServiceOptionsWrapper::set_ping_ms)
        .def_property("ping_initial_ms", &DiscoveryServiceOptionsWrapper::get_ping_initial_ms, &DiscoveryServiceOptionsWrapper::set_ping_initial_ms)
        .def_property("send_addr", &DiscoveryServiceOptionsWrapper::get_send_addr, &DiscoveryServiceOptionsWrapper::set_send_addr)
        .def_property("send_addr_size", &DiscoveryServiceOptionsWrapper::get_send_addr_size, &DiscoveryServiceOptionsWrapper::set_send_addr_size)
        .def_property("broadcast_addrs", &DiscoveryServiceOptionsWrapper::get_broadcast_addrs, &DiscoveryServiceOptionsWrapper::set_broadcast_addrs)
        .def_property("broadcast_num", &DiscoveryServiceOptionsWrapper::get_broadcast_num, &DiscoveryServiceOptionsWrapper::set_broadcast_num)
        .def_property("send_host", &DiscoveryServiceOptionsWrapper::get_send_host, &DiscoveryServiceOptionsWrapper::set_send_host);

    py::class_<DiscoveryServiceHostDiscoveryInfoWrapper>(m, "DiscoveryServiceHostDiscoveryInfo")
        .def_property("last_ping_index", &DiscoveryServiceHostDiscoveryInfoWrapper::get_last_ping_index, &DiscoveryServiceHostDiscoveryInfoWrapper::set_last_ping_index);

    py::class_<DiscoveryServiceWrapper>(m, "DiscoveryService")
        .def_property("log", &DiscoveryServiceWrapper::get_log, &DiscoveryServiceWrapper::set_log)
        .def_property("options", &DiscoveryServiceWrapper::get_options, &DiscoveryServiceWrapper::set_options)
        .def_property("discovery", &DiscoveryServiceWrapper::get_discovery, &DiscoveryServiceWrapper::set_discovery)
        .def_property("ping_index", &DiscoveryServiceWrapper::get_ping_index, &DiscoveryServiceWrapper::set_ping_index)
        .def_property("hosts", &DiscoveryServiceWrapper::get_hosts, &DiscoveryServiceWrapper::set_hosts)
        .def_property("host_discovery_infos", &DiscoveryServiceWrapper::get_host_discovery_infos, &DiscoveryServiceWrapper::set_host_discovery_infos)
        .def_property("hosts_count", &DiscoveryServiceWrapper::get_hosts_count, &DiscoveryServiceWrapper::set_hosts_count)
        .def_property("state_mutex", &DiscoveryServiceWrapper::get_state_mutex, &DiscoveryServiceWrapper::set_state_mutex)
        .def_property("thread", &DiscoveryServiceWrapper::get_thread, &DiscoveryServiceWrapper::set_thread)
        .def_property("stop_cond", &DiscoveryServiceWrapper::get_stop_cond, &DiscoveryServiceWrapper::set_stop_cond);
}