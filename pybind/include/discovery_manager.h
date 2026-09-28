#ifndef CHIAKI_PY_DISCOVERYMANAGER_H
#define CHIAKI_PY_DISCOVERYMANAGER_H

#include "host.h"

#include <chiaki/discoveryservice.h>

#include <vector>
#include <unordered_map>
#include <mutex>

// One console found by DiscoveryManager's broadcast discovery, or filled in by hand to
// register a manual one. `ps5`, `host_addr` and `state` (whether it's awake or in standby)
// are the fields most callers need; the rest mirrors what the console's discovery reply reports.
struct DiscoveryHost
{
    bool ps5;
    ChiakiDiscoveryHostState state;
    ChiakiTarget target;
    uint16_t host_request_port;

    std::string host_addr;
    std::string system_version;
    std::string device_discovery_protocol_version;
    std::string host_name;
    std::string host_type;
    std::string host_id;
    std::string running_app_titleid;
    std::string running_app_name;

    // The host's MAC address, parsed from host_id.
    HostMAC GetHostMAC() const;
};

// Broadcasts for PS4/PS5 hosts on the local network (IPv4 and IPv6) in the background and keeps
// track of what answered. `chiaki_py.discover_hosts()` wraps the start/wait/collect/stop sequence
// this class otherwise requires driving by hand.
class DiscoveryManager
{
	friend class DiscoveryManagerPrivate;

	private:
		ChiakiLog log;
		std::vector<ChiakiDiscoveryService> services;
		ChiakiDiscoveryService service;
		ChiakiDiscoveryService service_ipv6;
		bool service_active;
		bool service_active_ipv6;
		mutable std::mutex hosts_mutex;
		std::vector<DiscoveryHost> hosts;

    // slots

	public:
		explicit DiscoveryManager();
		~DiscoveryManager();

		// Start or stop broadcasting. Starting re-inits the discovery sockets if they were not
		// already active; stopping tears them down and clears the discovered host list.
		void SetActive(bool active);

		// Send a wakeup packet to `host` (a registration's `regist_key`, hex-encoded) so a console
		// in standby powers on. Raises ValueError if `regist_key` isn't hex, and RuntimeError if it is
		// too long or sending fails.
		void SendWakeup(std::string host, std::string regist_key, bool ps5);

		// Whether broadcast discovery is currently running.
		bool GetActive() const { return service_active; }

		// The hosts the last broadcast round found. Empty until set_active(True) has had time to hear back.
		const std::vector<DiscoveryHost> GetHosts() const;

		// Replace the broadcast-discovered host list wholesale. Called internally as broadcast replies
		// come in; not normally needed from Python.
		void DiscoveryServiceHosts(std::vector<DiscoveryHost> hosts);

		void HostsUpdated();
};

#endif //CHIAKI_PY_DISCOVERYMANAGER_H
