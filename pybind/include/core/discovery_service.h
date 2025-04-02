#ifndef CHIAKI_PY_DISCOVERY_SERVICE_H
#define CHIAKI_PY_DISCOVERY_SERVICE_H

#include "struct_wrapper.h"
#include "log.h"
#include "common.h"
#include "../event_source.h"
#include "../utils.h"

#include <chiaki/discoveryservice.h>

#include <pybind11/pybind11.h>

namespace py = pybind11;

void init_core_discovery_service(py::module &m);

class DiscoveryServiceEvent
{
public:
    ChiakiDiscoveryHost *hosts;
    size_t hosts_count;
    void *user;

    void map_cb(ChiakiDiscoveryHost *hosts, size_t hosts_count, void *user)
    {
        this->hosts = hosts;
        this->hosts_count = hosts_count;
        this->user = user;
    }
};

class DiscoveryServiceOptionsWrapper : public StructWrapper<ChiakiDiscoveryServiceOptions>
{
private:
    EventSource<DiscoveryServiceEvent> discoveryServiceEvent{};

public:
    using StructWrapper::StructWrapper;

    DiscoveryServiceOptionsWrapper() : StructWrapper<ChiakiDiscoveryServiceOptions>() {
        raw().cb = discoveryServiceEvent.cb_to_event<ChiakiDiscoveryServiceCb>();
        raw().cb_user = const_cast<EventSource<DiscoveryServiceEvent> *>(&discoveryServiceEvent);
    }

    size_t get_hosts_max() { return raw().hosts_max; }
    void set_hosts_max(size_t hosts_max) { raw().hosts_max = hosts_max; }

    uint64_t get_host_drop_pings() { return raw().host_drop_pings; }
    void set_host_drop_pings(uint64_t host_drop_pings) { raw().host_drop_pings = host_drop_pings; }

    uint64_t get_ping_ms() { return raw().ping_ms; }
    void set_ping_ms(uint64_t ping_ms) { raw().ping_ms = ping_ms; }

    uint64_t get_ping_initial_ms() { return raw().ping_initial_ms; }
    void set_ping_initial_ms(uint64_t ping_initial_ms) { raw().ping_initial_ms = ping_initial_ms; }

    sockaddr_storage *get_send_addr() { return raw().send_addr; }
    void set_send_addr(sockaddr_storage *send_addr) { raw().send_addr = send_addr; }

    size_t get_send_addr_size() { return raw().send_addr_size; }
    void set_send_addr_size(size_t send_addr_size) { raw().send_addr_size = send_addr_size; }

    sockaddr_storage *get_broadcast_addrs() { return raw().broadcast_addrs; }
    void set_broadcast_addrs(sockaddr_storage *broadcast_addrs) { raw().broadcast_addrs = broadcast_addrs; }

    size_t get_broadcast_num() { return raw().broadcast_num; }
    void set_broadcast_num(size_t broadcast_num) { raw().broadcast_num = broadcast_num; }

    char *get_send_host() { return raw().send_host; }
    void set_send_host(char *send_host) { raw().send_host = send_host; }

};

class DiscoveryServiceHostDiscoveryInfoWrapper : public StructWrapper<ChiakiDiscoveryServiceHostDiscoveryInfo>
{
public:
    using StructWrapper::StructWrapper;

    uint64_t get_last_ping_index() const { return ptr()->last_ping_index; }
    void set_last_ping_index(uint64_t last_ping_index) { ptr()->last_ping_index = last_ping_index; }
};

class DiscoveryServiceWrapper : public StructWrapper<ChiakiDiscoveryService>
{
public:
    using StructWrapper::StructWrapper;

    DiscoveryServiceWrapper(DiscoveryServiceOptionsWrapper &options, LogWrapper &log) : StructWrapper<ChiakiDiscoveryService>()
    {
        ChiakiErrorCode err = chiaki_discovery_service_init(ptr(), options.ptr(), log.ptr());
        if (err != CHIAKI_ERR_SUCCESS)
        {
            throw std::runtime_error("Failed to initialize DiscoveryService: " + std::string(chiaki_error_string(err)));
        }
    }

    ~DiscoveryServiceWrapper()
    {
        chiaki_discovery_service_fini(ptr());
    }

    ChiakiLog *get_log() { return raw().log; }
    void set_log(ChiakiLog *log) { raw().log = log; }

    ChiakiDiscoveryServiceOptions get_options() { return raw().options; }
    void set_options(ChiakiDiscoveryServiceOptions options) { raw().options = options; }

    ChiakiDiscovery get_discovery() { return raw().discovery; }
    void set_discovery(ChiakiDiscovery discovery) { raw().discovery = discovery; }

    uint64_t get_ping_index() { return raw().ping_index; }
    void set_ping_index(uint64_t ping_index) { raw().ping_index = ping_index; }

    ChiakiDiscoveryHost *get_hosts() { return raw().hosts; }
    void set_hosts(ChiakiDiscoveryHost *hosts) { raw().hosts = hosts; }

    ChiakiDiscoveryServiceHostDiscoveryInfo *get_host_discovery_infos() { return raw().host_discovery_infos; }
    void set_host_discovery_infos(ChiakiDiscoveryServiceHostDiscoveryInfo *host_discovery_infos) { raw().host_discovery_infos = host_discovery_infos; }

    size_t get_hosts_count() { return raw().hosts_count; }
    void set_hosts_count(size_t hosts_count) { raw().hosts_count = hosts_count; }

    ChiakiMutex get_state_mutex() { return raw().state_mutex; }
    void set_state_mutex(ChiakiMutex state_mutex) { raw().state_mutex = state_mutex; }

    ChiakiThread get_thread() { return raw().thread; }
    void set_thread(ChiakiThread thread) { raw().thread = thread; }

    ChiakiBoolPredCond get_stop_cond() { return raw().stop_cond; }
    void set_stop_cond(ChiakiBoolPredCond stop_cond) { raw().stop_cond = stop_cond; }
};

#endif //CHIAKI_PY_DISCOVERY_SERVICE_H