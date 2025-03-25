// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef CHIAKI_PY_DISCOVERYMANAGER_H
#define CHIAKI_PY_DISCOVERYMANAGER_H

#include "host.h"
#include "settings.h"
#include "core/struct_wrapper.h"

#include <chiaki/discoveryservice.h>

#include <vector>
#include <unordered_map>

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

    HostMAC GetHostMAC() const;
};

class DiscoveryHostWrapper : public StructWrapper<DiscoveryHost>
{
public:
    using StructWrapper::StructWrapper;

    bool getPs5() { return raw().ps5; }
    void setPs5(bool ps5) { raw().ps5 = ps5; }

    ChiakiDiscoveryHostState getState() { return raw().state; }
    void setState(ChiakiDiscoveryHostState state) { raw().state = state; }

    ChiakiTarget getTarget() { return raw().target; }
    void setTarget(ChiakiTarget target) { raw().target = target; }

    uint16_t getHostRequestPort() { return raw().host_request_port; }
    void setHostRequestPort(uint16_t host_request_port) { raw().host_request_port = host_request_port; }

    std::string getHostAddr() { return raw().host_addr; }
    void setHostAddr(std::string host_addr) { raw().host_addr = host_addr; }

    std::string getSystemVersion() { return raw().system_version; }
    void setSystemVersion(std::string system_version) { raw().system_version = system_version; }

    std::string getDeviceDiscoveryProtocolVersion() { return raw().device_discovery_protocol_version; }
    void setDeviceDiscoveryProtocolVersion(std::string device_discovery_protocol_version) { raw().device_discovery_protocol_version = device_discovery_protocol_version; }

    std::string getHostName() { return raw().host_name; }
    void setHostName(std::string host_name) { raw().host_name = host_name; }

    std::string getHostType() { return raw().host_type; }
    void setHostType(std::string host_type) { raw().host_type = host_type; }

    std::string getHostId() { return raw().host_id; }
    void setHostId(std::string host_id) { raw().host_id = host_id; }

    std::string getRunningAppTitleId() { return raw().running_app_titleid; }
    void setRunningAppTitleId(std::string running_app_titleid) { raw().running_app_titleid = running_app_titleid; }

    std::string getRunningAppName() { return raw().running_app_name; }
    void setRunningAppName(std::string running_app_name) { raw().running_app_name = running_app_name; }

    HostMAC GetHostMAC() const { return raw().GetHostMAC(); };
};

struct ManualService
{
	~ManualService() { chiaki_discovery_service_fini(&service); }

	class DiscoveryManager *manager;
	bool discovered = false;
	DiscoveryHostWrapper discovery_host;
	ChiakiDiscoveryService service;
};

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
		std::vector<DiscoveryHostWrapper> hosts;
		Settings *settings = {};
        std::unordered_map<std::string, ManualService *> manual_services;

    // slots

	public:
		explicit DiscoveryManager();
		~DiscoveryManager();

		void SetActive(bool active);
		void SetSettings(Settings *settings);

        void SendWakeup(const std::string &host, const std::string &regist_key, bool ps5);

        bool GetActive() const { return service_active; }
		const std::vector<DiscoveryHostWrapper> GetHosts() const;

		void DiscoveryServiceHosts(std::vector<DiscoveryHostWrapper> hosts);
		void UpdateManualServices();

	// signals
		void HostsUpdated();
};

#endif //CHIAKI_PY_DISCOVERYMANAGER_H
