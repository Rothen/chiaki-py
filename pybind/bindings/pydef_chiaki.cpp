// Bindings of the types of chiaki's C library that the rest of the module uses. Written by hand, unlike
// the pydef_*.cpp files generate_bindings.py writes: litgen only handles the headers of this project.
// Their stubs are in chiaki_py/lib/chiaki_py/__init__.pyi and core/*.pyi, outside the generated sections.

#include "pydef.h"

#include <chiaki/common.h>
#include <chiaki/discovery.h>
#include <chiaki/ffmpegdecoder.h>
#include <chiaki/log.h>
#include <chiaki/regist.h>
#include <chiaki/session.h>

#include <iomanip>
#include <sstream>
#include <string>

void py_init_chiaki(py::module_ &m)
{
    auto m_core = m.def_submodule("core", "The core submodule.");
    auto m_core_common = m_core.def_submodule("common", "The common submodule.");
    auto m_core_log = m_core.def_submodule("log", "The log submodule.");

    py::enum_<ChiakiTarget>(m_core_common, "Target")
        .value("PS4_UNKNOWN", CHIAKI_TARGET_PS4_UNKNOWN)
        .value("PS4_8", CHIAKI_TARGET_PS4_8)
        .value("PS4_9", CHIAKI_TARGET_PS4_9)
        .value("PS4_10", CHIAKI_TARGET_PS4_10)
        .value("PS5_UNKNOWN", CHIAKI_TARGET_PS5_UNKNOWN)
        .value("PS5_1", CHIAKI_TARGET_PS5_1)
        .export_values();

    py::enum_<ChiakiCodec>(m_core_common, "Codec")
        .value("H264", CHIAKI_CODEC_H264)
        .value("H265", CHIAKI_CODEC_H265)
        .value("H265_HDR", CHIAKI_CODEC_H265_HDR)
        .export_values();

    py::enum_<ChiakiLogLevel>(m_core_log, "LogLevel")
        .value("DEBUG", CHIAKI_LOG_DEBUG)
        .value("VERBOSE", CHIAKI_LOG_VERBOSE)
        .value("INFO", CHIAKI_LOG_INFO)
        .value("WARNING", CHIAKI_LOG_WARNING)
        .value("ERROR", CHIAKI_LOG_ERROR)
        .export_values();

    py::enum_<ChiakiDisableAudioVideo>(m, "DisableAudioVideo",
        "Which of the audio/video streams to skip receiving, as returned by Settings.get_audio_video_disabled().")
        .value("None_", ChiakiDisableAudioVideo::CHIAKI_NONE_DISABLED)
        .value("Audio", ChiakiDisableAudioVideo::CHIAKI_AUDIO_DISABLED)
        .value("Video", ChiakiDisableAudioVideo::CHIAKI_VIDEO_DISABLED)
        .value("AudioVideo", ChiakiDisableAudioVideo::CHIAKI_AUDIO_VIDEO_DISABLED)
        .export_values();

    py::enum_<ChiakiVideoResolutionPreset>(m, "VideoResolutionPreset",
        "The resolution presets Settings' get/set_resolution_local_ps4/ps5 and .../remote_ps4/ps5 "
        "methods store; the actual pixel dimensions negotiated end up in ChiakiPySession.get_video_profile().")
        .value("Resolution360p", ChiakiVideoResolutionPreset::CHIAKI_VIDEO_RESOLUTION_PRESET_360p)
        .value("Resolution540p", ChiakiVideoResolutionPreset::CHIAKI_VIDEO_RESOLUTION_PRESET_540p)
        .value("Resolution720p", ChiakiVideoResolutionPreset::CHIAKI_VIDEO_RESOLUTION_PRESET_720p)
        .value("Resolution1080p", ChiakiVideoResolutionPreset::CHIAKI_VIDEO_RESOLUTION_PRESET_1080p)
        .export_values();

    py::enum_<ChiakiVideoFPSPreset>(m, "VideoFPSPreset",
        "The frame rate presets Settings' get/set_fpslocal/remote_ps4/ps5 methods store.")
        .value("FPS30", ChiakiVideoFPSPreset::CHIAKI_VIDEO_FPS_PRESET_30)
        .value("FPS60", ChiakiVideoFPSPreset::CHIAKI_VIDEO_FPS_PRESET_60)
        .export_values();

    py::enum_<ChiakiQuitReason>(m, "QuitReason",
        "Why a ChiakiPySession ended, as passed to ChiakiPySession.on_session_quit() subscribers. "
        "See quit_reason_is_error() and quit_reason_string().")
        .value("None_", CHIAKI_QUIT_REASON_NONE)
        .value("Stopped", CHIAKI_QUIT_REASON_STOPPED)
        .value("SessionRequestUnknown", CHIAKI_QUIT_REASON_SESSION_REQUEST_UNKNOWN)
        .value("SessionRequestConnectionRefused", CHIAKI_QUIT_REASON_SESSION_REQUEST_CONNECTION_REFUSED)
        .value("SessionRequestRpInUse", CHIAKI_QUIT_REASON_SESSION_REQUEST_RP_IN_USE)
        .value("SessionRequestRpCrash", CHIAKI_QUIT_REASON_SESSION_REQUEST_RP_CRASH)
        .value("SessionRequestRpVersionMismatch", CHIAKI_QUIT_REASON_SESSION_REQUEST_RP_VERSION_MISMATCH)
        .value("CtrlUnknown", CHIAKI_QUIT_REASON_CTRL_UNKNOWN)
        .value("CtrlConnectFailed", CHIAKI_QUIT_REASON_CTRL_CONNECT_FAILED)
        .value("CtrlConnectionRefused", CHIAKI_QUIT_REASON_CTRL_CONNECTION_REFUSED)
        .value("StreamConnectionUnknown", CHIAKI_QUIT_REASON_STREAM_CONNECTION_UNKNOWN)
        .value("StreamConnectionRemoteDisconnected", CHIAKI_QUIT_REASON_STREAM_CONNECTION_REMOTE_DISCONNECTED)
        .value("StreamConnectionRemoteShutdown", CHIAKI_QUIT_REASON_STREAM_CONNECTION_REMOTE_SHUTDOWN)
        .value("PsnRegistFailed", CHIAKI_QUIT_REASON_PSN_REGIST_FAILED);

    m.def("quit_reason_is_error", [](ChiakiQuitReason reason) { return chiaki_quit_reason_is_error(reason); },
        py::arg("reason"),
        "True if `reason` means the session failed, False for a normal stop or console shutdown.");

    m.def("quit_reason_string", [](ChiakiQuitReason reason) { return std::string(chiaki_quit_reason_string(reason)); },
        py::arg("reason"),
        "A human-readable description of `reason`.");

    py::class_<ChiakiFfmpegDecoder>(m, "FfmpegDecoder",
        "An opaque handle to a ChiakiPySession's FFmpeg decoder, as returned by "
        "ChiakiPySession.get_ffmpeg_decoder(). Not usable from Python beyond passing it back around; "
        "ChiakiPySession.has_hardware_decoder() and .hardware_decoder_type() are how to inspect it.");

    py::class_<ChiakiConnectVideoProfile>(m, "ChiakiConnectVideoProfile",
        "The stream's video settings, as returned by ChiakiPySession.get_video_profile(): the requested "
        "ones until the console answers, then the negotiated ones.")
        .def_readwrite("width", &ChiakiConnectVideoProfile::width)
        .def_readwrite("height", &ChiakiConnectVideoProfile::height)
        .def_readwrite("max_fps", &ChiakiConnectVideoProfile::max_fps)
        .def_readwrite("bitrate", &ChiakiConnectVideoProfile::bitrate)
        .def_readwrite("codec", &ChiakiConnectVideoProfile::codec);

    py::enum_<ChiakiDiscoveryHostState>(m, "DiscoveryHostState",
        "A discovered console's power state (DiscoveryHost.state): Ready to stream, Standby (needs a "
        "wakeup packet first, see DiscoveryManager.send_wakeup()), or Unknown before a reply is parsed.")
        .value("Unknown", ChiakiDiscoveryHostState::CHIAKI_DISCOVERY_HOST_STATE_UNKNOWN)
        .value("Ready", ChiakiDiscoveryHostState::CHIAKI_DISCOVERY_HOST_STATE_READY)
        .value("Standby", ChiakiDiscoveryHostState::CHIAKI_DISCOVERY_HOST_STATE_STANDBY)
        .export_values();

    py::class_<ChiakiRegisteredHost>(m, "RegisteredHost",
        "The result of a successful registration, as delivered through RegistEvent.registered_host by "
        "Backend.register_host_async(). Backend.register_host() (the blocking variant) returns the "
        "equivalent information as a RegistResult instead.")
        .def_readonly("target", &ChiakiRegisteredHost::target)
        .def_readonly("ap_ssid", &ChiakiRegisteredHost::ap_ssid)
        .def_readonly("ap_bssid", &ChiakiRegisteredHost::ap_bssid)
        .def_readonly("ap_key", &ChiakiRegisteredHost::ap_key)
        .def_readonly("ap_name", &ChiakiRegisteredHost::ap_name)
        .def_readonly("server_mac", &ChiakiRegisteredHost::server_mac)
        .def_readonly("server_nickname", &ChiakiRegisteredHost::server_nickname)
        .def_readonly("rp_regist_key", &ChiakiRegisteredHost::rp_regist_key)
        .def_readonly("rp_key_type", &ChiakiRegisteredHost::rp_key_type)
        .def_readonly("rp_key", &ChiakiRegisteredHost::rp_key)
        .def_readonly("console_pin", &ChiakiRegisteredHost::console_pin)
        .def("__repr__",
             [](const ChiakiRegisteredHost &host)
             {
                 std::ostringstream mac_ss;
                 mac_ss << std::hex << std::setfill('0');
                 for (int i = 0; i < 6; ++i)
                 {
                     mac_ss << std::setw(2) << static_cast<int>(host.server_mac[i]);
                     if (i < 5)
                         mac_ss << ":";
                 }

                 return "<RegisteredHost server_nickname='" + std::string(host.server_nickname) +
                        "' ap_name='" + std::string(host.ap_name) +
                        "' ap_ssid='" + std::string(host.ap_ssid) +
                        "' server_mac='" + mac_ss.str() +
                        "' console_pin=" + std::to_string(host.console_pin) +
                        ">";
             });

    py::enum_<ChiakiRegistEventType>(m, "RegistEventType",
        "How a Backend.register_host_async() attempt ended: FINISHED_SUCCESS (registered_host is set), "
        "FINISHED_FAILED (wrong PIN, console unreachable, ...) or FINISHED_CANCELED.")
        .value("FINISHED_CANCELED", ChiakiRegistEventType::CHIAKI_REGIST_EVENT_TYPE_FINISHED_CANCELED)
        .value("FINISHED_FAILED", ChiakiRegistEventType::CHIAKI_REGIST_EVENT_TYPE_FINISHED_FAILED)
        .value("FINISHED_SUCCESS", ChiakiRegistEventType::CHIAKI_REGIST_EVENT_TYPE_FINISHED_SUCCESS)
        .export_values();

    py::class_<ChiakiRegistEvent>(m, "RegistEvent",
        "Delivered to Backend.register_host_async()'s `on_next` callback once registration finishes; "
        "`type` says how (see RegistEventType), and `registered_host` carries the result if it succeeded.")
        .def_readonly("type", &ChiakiRegistEvent::type)
        .def_readonly("registered_host", &ChiakiRegistEvent::registered_host)
        .def("__repr__",
             [](const ChiakiRegistEvent &e)
             {
                 std::ostringstream ss;
                 ss << "<RegistEvent type=" << e.type
                    << " registered_host=" << e.registered_host << ">";
                 return ss.str();
             });
}
