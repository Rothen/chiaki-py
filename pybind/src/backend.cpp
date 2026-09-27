#include "backend.h"

#include <cstring>
#include <iomanip>
#include <sstream>

namespace
{
// A fixed-size char field of chiaki's, which is NUL-terminated only if it is shorter than the field.
template <size_t N>
std::string field_string(const char (&field)[N])
{
    return std::string(field, strnlen(field, N));
}
}

RegistResult::RegistResult(const ChiakiRegistEvent &event)
{
    const ChiakiRegisteredHost &host = *event.registered_host;
    type = event.type;
    target = host.target;
    ap_ssid = field_string(host.ap_ssid);
    ap_bssid = field_string(host.ap_bssid);
    ap_key = field_string(host.ap_key);
    ap_name = field_string(host.ap_name);

    std::ostringstream mac;
    mac << std::hex << std::setfill('0');
    for (size_t i = 0; i < sizeof(host.server_mac); ++i)
    {
        if (i > 0)
            mac << ":";
        mac << std::setw(2) << static_cast<int>(host.server_mac[i]);
    }
    server_mac = mac.str();

    server_nickname = field_string(host.server_nickname);
    rp_regist_key = field_string(host.rp_regist_key);
    rp_key_type = host.rp_key_type;
    std::memcpy(rp_key, host.rp_key, sizeof(rp_key));
    console_pin = host.console_pin;
}
