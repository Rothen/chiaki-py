// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib") // Link Winsock automatically
#else
    #include <netdb.h>
    #include <netinet/in.h>
#endif
#include <stdio.h>
#include <string.h>
#include <chiaki-pybind.h>
#include <chiaki/discovery.h>

int initialize_winsock()
{
    int result = 0;
#ifdef _WIN32
    WSADATA wsaData;
    result = WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
    return result;
}

void cleanup_winsock()
{
#ifdef _WIN32
    WSACleanup();
#endif
}

static void discovery_cb(ChiakiDiscoveryHost *host, void *user)
{
    initialize_winsock ();
    ChiakiLog *log = user;

    CHIAKI_LOGI(log, "--");
	CHIAKI_LOGI(log, "Discovered Host:");
	CHIAKI_LOGI(log, "State:                             %s", chiaki_discovery_host_state_string(host->state));

	if(host->system_version)
		CHIAKI_LOGI(log, "System Version:                    %s", host->system_version);

	if(host->device_discovery_protocol_version)
		CHIAKI_LOGI(log, "Device Discovery Protocol Version: %s", host->device_discovery_protocol_version);

	if(host->host_request_port)
		CHIAKI_LOGI(log, "Request Port:                      %hu", (unsigned short)host->host_request_port);

	if(host->host_name)
		CHIAKI_LOGI(log, "Host Name:                         %s", host->host_name);

	if(host->host_type)
		CHIAKI_LOGI(log, "Host Type:                         %s", host->host_type);

	if(host->host_id)
		CHIAKI_LOGI(log, "Host ID:                           %s", host->host_id);

	if(host->running_app_titleid)
		CHIAKI_LOGI(log, "Running App Title ID:              %s", host->running_app_titleid);

	if(host->running_app_name)
		CHIAKI_LOGI(log, "Running App Name:                  %s%s", host->running_app_name, (strcmp(host->running_app_name, "Persona 5") == 0 ? " (best game ever)" : ""));

	CHIAKI_LOGI(log, "--");
}

CHIAKI_EXPORT ChiakiErrorCode chiaki_pybind_discover(ChiakiLog *log, const char *host, const float timeout)
{
	if(!host)
	{
		fprintf(stderr, "No host specified, see --help.\n");
		return 1;
	}

#ifdef _WIN32
    initialize_winsock(); // Ensure Winsock is started
#endif

    struct addrinfo *host_addrinfos;
	// make hostname use ipv4 for now
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP; // ✅ Force UDP
    hints.ai_family = AF_UNSPEC;     // ✅ Allow both IPv4 and IPv6
    char *ipv6 = strchr(host, ':');
	if(ipv6)
		hints.ai_family = AF_INET6;
	else
		hints.ai_family = AF_INET;
	int r = getaddrinfo(host, NULL, &hints, &host_addrinfos);
	if(r != 0)
	{
		CHIAKI_LOGE(log, "getaddrinfo failed");
        return CHIAKI_ERR_HOST_UNREACH;
    }

	struct sockaddr *host_addr = NULL;
	socklen_t host_addr_len = 0;
	for(struct addrinfo *ai=host_addrinfos; ai; ai=ai->ai_next)
	{
		if(ai->ai_protocol != IPPROTO_UDP)
			continue;
		if(ai->ai_family != AF_INET && ai->ai_family != AF_INET6)
			continue;

		host_addr_len = ai->ai_addrlen;
		host_addr = (struct sockaddr *)malloc(host_addr_len);
		if(!host_addr)
			break;
		memcpy(host_addr, ai->ai_addr, host_addr_len);
		break;
	}
	freeaddrinfo(host_addrinfos);

	if(!host_addr)
	{
		CHIAKI_LOGE(log, "Failed to get addr for hostname");
        return CHIAKI_ERR_HOST_UNREACH;
    }

	ChiakiDiscoveryPacket packet;
	memset(&packet, 0, sizeof(packet));
	packet.cmd = CHIAKI_DISCOVERY_CMD_SRCH;
	packet.protocol_version = CHIAKI_DISCOVERY_PROTOCOL_VERSION_PS4;
	if(host_addr->sa_family == AF_INET)
		((struct sockaddr_in *)host_addr)->sin_port = htons(CHIAKI_DISCOVERY_PORT_PS4);
	else
		((struct sockaddr_in6 *)host_addr)->sin6_port = htons(CHIAKI_DISCOVERY_PORT_PS4);

	ChiakiDiscovery discovery;
	ChiakiErrorCode err = chiaki_discovery_init(&discovery, log, host_addr->sa_family);
	if(err != CHIAKI_ERR_SUCCESS)
	{
		CHIAKI_LOGE(log, "Discovery init failed");
		goto cleanup_host_addr;
	}

	ChiakiDiscoveryThread thread;
	err = chiaki_discovery_thread_start_oneshot(&thread, &discovery, discovery_cb, NULL);
	if(err != CHIAKI_ERR_SUCCESS)
	{
		CHIAKI_LOGE(log, "Discovery thread init failed");
		goto cleanup;
	}
	err = chiaki_discovery_send(&discovery, &packet, host_addr, host_addr_len);
	if(err != CHIAKI_ERR_SUCCESS)
		CHIAKI_LOGE(log, "Failed to send discovery packet for PS4: %s", chiaki_error_string(err));
	packet.protocol_version = CHIAKI_DISCOVERY_PROTOCOL_VERSION_PS5;
	if(host_addr->sa_family == AF_INET)
		((struct sockaddr_in *)host_addr)->sin_port = htons(CHIAKI_DISCOVERY_PORT_PS5);
	else
		((struct sockaddr_in6 *)host_addr)->sin6_port = htons(CHIAKI_DISCOVERY_PORT_PS5);
	err = chiaki_discovery_send(&discovery, &packet, host_addr, host_addr_len);
	if(err != CHIAKI_ERR_SUCCESS)
		CHIAKI_LOGE(log, "Failed to send discovery packet for PS5: %s", chiaki_error_string(err));
	uint64_t timeout_ms=(timeout);
	err = chiaki_thread_timedjoin(&thread.thread, NULL, timeout_ms);
	if(err != CHIAKI_ERR_SUCCESS)
	{
		if(err == CHIAKI_ERR_TIMEOUT)
		{
			CHIAKI_LOGE(log, "Discovery request timed out after timeout: %.*f ms", 1, timeout);
			chiaki_discovery_thread_stop(&thread);
		}
		goto cleanup;
	}
	chiaki_discovery_fini(&discovery);
	free(host_addr);
    return CHIAKI_ERR_SUCCESS;

cleanup:
	chiaki_discovery_fini(&discovery);
cleanup_host_addr:
	free(host_addr);
    return CHIAKI_ERR_UNKNOWN;
}
