#include "core/session.h"

#include <chiaki/session.h>

class ChiakiSocket
{
public:
    chiaki_socket_t socket;

    explicit ChiakiSocket(chiaki_socket_t sock = CHIAKI_INVALID_SOCKET) : socket(sock) {}

    bool is_valid() const
    {
        return socket != CHIAKI_INVALID_SOCKET;
    }

    chiaki_socket_t get_socket() const
    {
        return socket;
    }

    void close_socket()
    {
        if (is_valid())
        {
            CHIAKI_SOCKET_CLOSE(socket);
            socket = CHIAKI_INVALID_SOCKET;
        }
    }
};

class ChiakiConnectInfoWrapper
{
public:
    ChiakiConnectInfo info;

    ChiakiConnectInfoWrapper() : info{} {}

    bool get_ps5() const { return info.ps5; }
    void set_ps5(bool value) { info.ps5 = value; }

    std::string get_host() const { return info.host ? std::string(info.host) : ""; }
    void set_host(const std::string &host) { info.host = host.c_str(); }

    std::string get_regist_key() const { return std::string(info.regist_key, CHIAKI_SESSION_AUTH_SIZE); }
    void set_regist_key(const std::string &key)
    {
        memset(info.regist_key, 0, CHIAKI_SESSION_AUTH_SIZE);
        strncpy(info.regist_key, key.c_str(), CHIAKI_SESSION_AUTH_SIZE);
    }

    std::vector<uint8_t> get_morning() const
    {
        return std::vector<uint8_t>(info.morning, info.morning + 0x10);
    }
    void set_morning(const std::vector<uint8_t> &morning)
    {
        if (morning.size() != 0x10)
            throw std::runtime_error("Morning must be 16 bytes long");
        memcpy(info.morning, morning.data(), 0x10);
    }

    ChiakiConnectVideoProfile get_video_profile() const { return info.video_profile; }
    void set_video_profile(ChiakiConnectVideoProfile value) { info.video_profile = value; }

    bool get_video_profile_auto_downgrade() const { return info.video_profile_auto_downgrade; }
    void set_video_profile_auto_downgrade(bool value) { info.video_profile_auto_downgrade = value; }

    bool get_enable_keyboard() const{ return info.enable_keyboard; }
    void set_enable_keyboard(bool value) { info.enable_keyboard = value; }

    bool get_enable_dualsense() const { return info.enable_dualsense; }
    void set_enable_dualsense(bool value) { info.enable_dualsense = value; }

    ChiakiDisableAudioVideo get_audio_video_disabled() const { return info.audio_video_disabled; }
    void set_audio_video_disabled(ChiakiDisableAudioVideo value) { info.audio_video_disabled = value; }

    bool get_auto_regist() const { return info.auto_regist; }
    void set_auto_regist(bool value) { info.auto_regist = value; }

    ChiakiHolepunchSession get_holepunch_session() const { return info.holepunch_session; }
    void set_holepunch_session(ChiakiHolepunchSession session) { info.holepunch_session = session; }

    std::vector<uint8_t> get_psn_account_id() const { return std::vector<uint8_t>(info.psn_account_id, info.psn_account_id + CHIAKI_PSN_ACCOUNT_ID_SIZE); }
    void set_psn_account_id(const std::vector<uint8_t> &psn_id)
    {
        if (psn_id.size() != CHIAKI_PSN_ACCOUNT_ID_SIZE)
            throw std::runtime_error("PSN Account ID must be correct size");
        memcpy(info.psn_account_id, psn_id.data(), CHIAKI_PSN_ACCOUNT_ID_SIZE);
    }

    std::shared_ptr<ChiakiSocket> get_rudp_sock() const
    {
        if (!info.rudp_sock)
            return std::make_shared<ChiakiSocket>(CHIAKI_INVALID_SOCKET);
        return std::make_shared<ChiakiSocket>(*info.rudp_sock);
    }

    void set_rudp_sock(std::shared_ptr<ChiakiSocket> sock)
    {
        if (!sock)
        {
            info.rudp_sock = nullptr;
        }
        else
        {
            info.rudp_sock = new chiaki_socket_t(sock->get_socket());
        }
    }

    double get_packet_loss_max() const { return info.packet_loss_max; }
    void set_packet_loss_max(double value) { info.packet_loss_max = value; }

    std::string to_string() const
    {
        return "<ChiakiConnectInfoWrapper ps5=" + std::to_string(info.ps5) +
               " host=" + get_host() +
               " regist_key=" + get_regist_key() +
               " enable_keyboard=" + std::to_string(info.enable_keyboard) +
               " rudp_sock=" + (info.rudp_sock && *info.rudp_sock != CHIAKI_INVALID_SOCKET ? "Valid" : "Invalid") +
               ">";
    }
};

namespace py = pybind11;

void init_core_session(py::module &m)
{
    // Define constants
    m.attr("CHIAKI_INVALID_SOCKET") = CHIAKI_INVALID_SOCKET;

    m.attr("CHIAKI_RP_APPLICATION_REASON_REGIST_FAILED") = CHIAKI_RP_APPLICATION_REASON_REGIST_FAILED;
    m.attr("CHIAKI_RP_APPLICATION_REASON_INVALID_PSN_ID") = CHIAKI_RP_APPLICATION_REASON_INVALID_PSN_ID;
    m.attr("CHIAKI_RP_APPLICATION_REASON_IN_USE") = CHIAKI_RP_APPLICATION_REASON_IN_USE;
    m.attr("CHIAKI_RP_APPLICATION_REASON_CRASH") = CHIAKI_RP_APPLICATION_REASON_CRASH;
    m.attr("CHIAKI_RP_APPLICATION_REASON_RP_VERSION") = CHIAKI_RP_APPLICATION_REASON_RP_VERSION;
    m.attr("CHIAKI_RP_APPLICATION_REASON_UNKNOWN") = CHIAKI_RP_APPLICATION_REASON_UNKNOWN;

    m.def("chiaki_rp_application_reason_string", &chiaki_rp_application_reason_string, py::arg("reason"), "Get the RP application reason string.");
    m.def("chiaki_rp_version_string", chiaki_rp_version_string, py::arg("target"), "Get the RP version string.");
    m.def("chiaki_rp_version_parse", chiaki_rp_version_parse, py::arg("rp_version_str"), py::arg("is_ps5"), "Parse the RP version string.");

    m.attr("CHIAKI_RP_DID_SIZE") = CHIAKI_RP_DID_SIZE;
    m.attr("CHIAKI_SESSION_ID_SIZE_MAX") = CHIAKI_SESSION_ID_SIZE_MAX;
    m.attr("CHIAKI_HANDSHAKE_KEY_SIZE") = CHIAKI_HANDSHAKE_KEY_SIZE;

    py::class_<ChiakiConnectVideoProfile>(m, "ChiakiConnectVideoProfile")
        .def(py::init<>())
        .def_readwrite("width", &ChiakiConnectVideoProfile::width)
        .def_readwrite("height", &ChiakiConnectVideoProfile::height)
        .def_readwrite("max_fps", &ChiakiConnectVideoProfile::max_fps)
        .def_readwrite("bitrate", &ChiakiConnectVideoProfile::bitrate)
        .def_readwrite("codec", &ChiakiConnectVideoProfile::codec)
        .def("__repr__", [](const ChiakiConnectVideoProfile &p)
             { return "<ChiakiConnectVideoProfile width=" + std::to_string(p.width) +
                      " height=" + std::to_string(p.height) +
                      " max_fps=" + std::to_string(p.max_fps) +
                      " bitrate=" + std::to_string(p.bitrate) +
                      " codec=" + std::to_string(static_cast<int>(p.codec)) + ">"; });

    py::enum_<ChiakiVideoResolutionPreset>(m, "VideoResolutionPreset")
        .value("PRESET_360p", CHIAKI_VIDEO_RESOLUTION_PRESET_360p)
        .value("PRESET_540p", CHIAKI_VIDEO_RESOLUTION_PRESET_540p)
        .value("PRESET_720p", CHIAKI_VIDEO_RESOLUTION_PRESET_720p)
        .value("PRESET_1080p", CHIAKI_VIDEO_RESOLUTION_PRESET_1080p)
        .export_values();

    py::enum_<ChiakiVideoFPSPreset>(m, "VideoFPSPreset")
        .value("PRESET_30", CHIAKI_VIDEO_FPS_PRESET_30)
        .value("PRESET_60", CHIAKI_VIDEO_FPS_PRESET_60)
        .export_values();

    m.def("chiaki_connect_video_profile_preset", &chiaki_connect_video_profile_preset, py::arg("profile"), py::arg("resolution"), py::arg("fps"), "Set the Chiaki connect video profile preset.");

    m.attr("CHIAKI_SESSION_AUTH_SIZE") = CHIAKI_SESSION_AUTH_SIZE;

    py::class_<ChiakiSocket, std::shared_ptr<ChiakiSocket>>(m, "ChiakiSocket")
        .def(py::init<>())
        .def("is_valid", &ChiakiSocket::is_valid)
        .def("close", &ChiakiSocket::close_socket)
        .def("__repr__", [](const ChiakiSocket &s)
             { return s.is_valid() ? "<ChiakiSocket: Valid>" : "<ChiakiSocket: Invalid>"; });

    py::class_<ChiakiConnectInfoWrapper>(m, "ConnectInfo")
        .def(py::init<>())
        .def_property("ps5", &ChiakiConnectInfoWrapper::get_ps5, &ChiakiConnectInfoWrapper::set_ps5)
        .def_property("host", &ChiakiConnectInfoWrapper::get_host, &ChiakiConnectInfoWrapper::set_host)
        .def_property("regist_key", &ChiakiConnectInfoWrapper::get_regist_key, &ChiakiConnectInfoWrapper::set_regist_key)
        .def_property("morning", &ChiakiConnectInfoWrapper::get_morning, &ChiakiConnectInfoWrapper::set_morning)
        .def_property("psn_account_id", &ChiakiConnectInfoWrapper::get_psn_account_id, &ChiakiConnectInfoWrapper::set_psn_account_id)
        .def_property("enable_keyboard", &ChiakiConnectInfoWrapper::get_enable_keyboard, &ChiakiConnectInfoWrapper::set_enable_keyboard)
        .def_property("enable_dualsense", &ChiakiConnectInfoWrapper::get_enable_dualsense, &ChiakiConnectInfoWrapper::set_enable_dualsense)
        .def_property("video_profile", &ChiakiConnectInfoWrapper::get_video_profile, &ChiakiConnectInfoWrapper::set_video_profile)
        .def_property("video_profile_auto_downgrade", &ChiakiConnectInfoWrapper::get_video_profile_auto_downgrade, &ChiakiConnectInfoWrapper::set_video_profile_auto_downgrade)
        .def_property("audio_video_disabled", &ChiakiConnectInfoWrapper::get_audio_video_disabled, &ChiakiConnectInfoWrapper::set_audio_video_disabled)
        .def_property("auto_regist", &ChiakiConnectInfoWrapper::get_auto_regist, &ChiakiConnectInfoWrapper::set_auto_regist)
        .def_property("holepunch_session", &ChiakiConnectInfoWrapper::get_holepunch_session, &ChiakiConnectInfoWrapper::set_holepunch_session)
        .def_property("rudp_sock", &ChiakiConnectInfoWrapper::get_rudp_sock, &ChiakiConnectInfoWrapper::set_rudp_sock)
        .def_property("packet_loss_max", &ChiakiConnectInfoWrapper::get_packet_loss_max, &ChiakiConnectInfoWrapper::set_packet_loss_max)
        .def("__repr__", &ChiakiConnectInfoWrapper::to_string);
}