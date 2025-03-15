// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include "py_host.h"

RegisteredHost::RegisteredHost()
{
    memset(rp_regist_key, 0, sizeof(rp_regist_key));
    memset(rp_key, 0, sizeof(rp_key));
}

RegisteredHost::RegisteredHost(const RegisteredHost &o)
    : target(o.target),
      ap_ssid(o.ap_ssid),
      ap_bssid(o.ap_bssid),
      ap_key(o.ap_key),
      ap_name(o.ap_name),
      server_mac(o.server_mac),
      server_nickname(o.server_nickname),
      rp_key_type(o.rp_key_type),
      console_pin(o.console_pin)
{
    memcpy(rp_regist_key, o.rp_regist_key, sizeof(rp_regist_key));
    memcpy(rp_key, o.rp_key, sizeof(rp_key));
}

RegisteredHost::RegisteredHost(const ChiakiRegisteredHost &chiaki_host)
    : server_mac(chiaki_host.server_mac)
{
    target = chiaki_host.target;
    ap_ssid = chiaki_host.ap_ssid;
    ap_bssid = chiaki_host.ap_bssid;
    ap_key = chiaki_host.ap_key;
    ap_name = chiaki_host.ap_name;
    server_nickname = chiaki_host.server_nickname;
    memcpy(rp_regist_key, chiaki_host.rp_regist_key, sizeof(rp_regist_key));
    rp_key_type = chiaki_host.rp_key_type;
    memcpy(rp_key, chiaki_host.rp_key, sizeof(rp_key));
    console_pin = std::to_string(chiaki_host.console_pin);
}

void RegisteredHost::SetConsolePin(RegisteredHost &host, const std::string &console_pin)
{
    host.console_pin = console_pin;
}

ManualHost::ManualHost()
{
    id = -1;
    registered = false;
}

ManualHost::ManualHost(int id, const std::string &host, bool registered, const HostMAC &registered_mac)
    : id(id),
      host(host),
      registered(registered),
      registered_mac(registered_mac)
{
}

ManualHost::ManualHost(int id, const ManualHost &o)
    : id(id),
      host(o.host),
      registered(o.registered),
      registered_mac(o.registered_mac)
{
}

void ManualHost::SetHost(const std::string &hostadd)
{
    host = hostadd;
}

PsnHost::PsnHost()
{
    duid = std::string();
    name = std::string();
    ps5 = false;
}

PsnHost::PsnHost(const std::string &duid, const std::string &name, bool ps5)
    : duid(duid),
      name(name),
      ps5(ps5)
{
}

ChiakiTarget PsnHost::GetTarget() const
{
    if (ps5)
    {
        ChiakiTarget target = CHIAKI_TARGET_PS5_1;
        return target;
    }
    else
    {
        ChiakiTarget target = CHIAKI_TARGET_PS4_10;
        return target;
    }
}
