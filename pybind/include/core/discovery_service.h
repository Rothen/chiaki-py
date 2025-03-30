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
    const EventSource<DiscoveryServiceEvent> &discoveryServiceEvent{};

public:
    using StructWrapper::StructWrapper;

    DiscoveryServiceOptionsWrapper() : StructWrapper<ChiakiDiscoveryServiceOptions>() {
        raw().cb = discoveryServiceEvent.cb_to_event();
        raw().cb_user = const_cast<EventSource<DiscoveryServiceEvent> *>(&discoveryServiceEvent);
    }
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
        chiaki_discovery_service_init(ptr(), options.ptr(), log.ptr());
    }

    ~DiscoveryServiceWrapper()
    {
        chiaki_discovery_service_fini(ptr());
    }
};

#endif //CHIAKI_PY_DISCOVERY_SERVICE_H