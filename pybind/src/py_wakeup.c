// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <chiaki-pybind.h>

#include <string.h>
#include <stdio.h>

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

#include <chiaki/discovery.h>

int initialize_winsock2()
{
    int result = 0;
#ifdef _WIN32
    WSADATA wsaData;
    result = WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
    return result;
}

void cleanup_winsock2()
{
#ifdef _WIN32
    WSACleanup();
#endif
}

CHIAKI_EXPORT ChiakiErrorCode chiaki_pybind_wakeup(ChiakiLog *log, const char *host, const char *registkey, bool ps5)
{
	if(!host)
	{
		fprintf(stderr, "No host specified, see --help.\n");
        return CHIAKI_ERR_PARSE_ADDR;
    }
	if(!registkey)
	{
		fprintf(stderr, "No registration key specified, see --help.\n");
        return CHIAKI_ERR_INVALID_DATA;
    }
	if(strlen(registkey) > 8)
	{
		fprintf(stderr, "Given registkey is too long.\n");
        return CHIAKI_ERR_INVALID_DATA;
    }

	uint64_t credential = (uint64_t)strtoull(registkey, NULL, 16);
    fprintf(stdout, "Waking up %llu\n", credential);
    initialize_winsock2();
    ChiakiErrorCode res = chiaki_discovery_wakeup(log, NULL, host, credential, ps5);
    cleanup_winsock2();
    return res;
}
