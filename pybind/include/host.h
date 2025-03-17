// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef CHIAKI_PY_HOST_H
#define CHIAKI_PY_HOST_H

#include <string>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <vector>
#include <fstream>
#include <chiaki/regist.h>

class HostMAC
{
private:
    uint8_t mac[6];

public:
    HostMAC() { memset(mac, 0, sizeof(mac)); }
    HostMAC(const HostMAC &o) { memcpy(mac, o.GetMAC(), sizeof(mac)); }
    explicit HostMAC(const uint8_t mac[6]) { memcpy(this->mac, mac, sizeof(this->mac)); }
    const uint8_t *GetMAC() const { return mac; }
    std::string ToString() const
    {
        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        for (size_t i = 0; i < sizeof(mac); ++i)
        {
            ss << std::setw(2) << static_cast<int>(mac[i]);
        }
        return ss.str();
    }
    uint64_t GetValue() const
    {
        return ((uint64_t)mac[0] << 0x28) | ((uint64_t)mac[1] << 0x20) | ((uint64_t)mac[2] << 0x18) | ((uint64_t)mac[3] << 0x10) | ((uint64_t)mac[4] << 0x8) | mac[5];
    }
};

static bool operator==(const HostMAC &a, const HostMAC &b) { return memcmp(a.GetMAC(), b.GetMAC(), 6) == 0; }
static bool operator!=(const HostMAC &a, const HostMAC &b) { return !(a == b); }
static bool operator<(const HostMAC &a, const HostMAC &b) { return a.GetValue() < b.GetValue(); }

class HiddenHost
{
public:
    HostMAC server_mac;
    std::string server_nickname;
    HiddenHost() { ; }
    HiddenHost(HostMAC server_mac, std::string server_nickname)
    {
        this->server_mac = server_mac;
        this->server_nickname = server_nickname;
    }
    HostMAC GetMAC() const { return server_mac; }
    std::string GetNickname() const { return server_nickname; }
    void SetNickname(const std::string &nickname) { this->server_nickname = nickname; }
};

static bool operator==(const HiddenHost &a, const HiddenHost &b) { return (a.GetMAC() == b.GetMAC() && a.GetNickname() == b.GetNickname()); }
class RegisteredHost
{
public:
    ChiakiTarget target;
    std::string ap_ssid;
    std::string ap_bssid;
    std::string ap_key;
    std::string ap_name;
    HostMAC server_mac;
    std::string server_nickname;
    char rp_regist_key[CHIAKI_SESSION_AUTH_SIZE];
    uint32_t rp_key_type;
    uint8_t rp_key[0x10];
    std::string console_pin;
    RegisteredHost();
    RegisteredHost(const RegisteredHost &o);

    RegisteredHost(const ChiakiRegisteredHost &chiaki_host);
    void SetConsolePin(RegisteredHost &host, const std::string &console_pin);
    ChiakiTarget GetTarget() const { return target; }
    const HostMAC &GetServerMAC() const { return server_mac; }
    const std::string &GetServerNickname() const { return server_nickname; }
    const std::vector<uint8_t> GetRPRegistKey() const { return std::vector<uint8_t>(rp_regist_key, rp_regist_key + sizeof(rp_regist_key)); }
    const std::vector<uint8_t> GetRPKey() const { return std::vector<uint8_t>(rp_key, rp_key + sizeof(rp_key)); }
    const std::string GetConsolePin() const { return console_pin; }
    const std::string &GetAPSSID() { return ap_ssid; }
    const std::string &GetAPBSSID() { return ap_bssid; }
    const std::string &GetAPKey() { return ap_key; }
    const std::string &GetAPName() { return ap_name; }
    const uint32_t &GetRPKeyType() { return rp_key_type; }
};

class ManualHost
{
public:
    int id;
    std::string host;
    bool registered;
    HostMAC registered_mac;
    ManualHost();
    ManualHost(int id, const std::string &host, bool registered, const HostMAC &registered_mac);
    ManualHost(int id, const ManualHost &o);
    void SetHost(const std::string &hostadd);

    int GetID() const { return id; }
    std::string GetHost() const { return host; }
    bool GetRegistered() const { return registered; }
    HostMAC GetMAC() const { return registered_mac; }

    void Register(const RegisteredHost &registered_host)
    {
        this->registered = true;
        this->registered_mac = registered_host.GetServerMAC();
    }
};
static bool operator==(const ManualHost &a, const ManualHost &b) { return (a.GetID() == b.GetID() && a.GetHost() == b.GetHost() && a.GetRegistered() == b.GetRegistered() && a.GetMAC() == b.GetMAC()); }
class PsnHost
{
private:
    std::string duid;
    std::string name;
    bool ps5;

public:
    PsnHost();
    PsnHost(const std::string &duid, const std::string &name, bool ps5);

    std::string GetDuid() const { return duid; }
    std::string GetName() const { return name; }
    bool IsPS5() const { return ps5; }
    ChiakiTarget GetTarget() const;
};

#endif // CHIAKI_PY_HOST_H
