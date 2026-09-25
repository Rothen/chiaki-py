#include <time.h>
#include "core/common.h"
#include "core/audio.h"
#include "core/base64.h"
#include "core/bitstream.h"
#include "core/controller.h"
// #include "core/discovery_service.h"
#include "core/ecdh.h"
#include "core/fec.h"
#include "core/feedback.h"
#include "core/log.h"
#include "event_source.h"
#include "settings.h"
#include "chiakipysession.h"
#include "discovery_manager.h"
#include "backend.h"
#include "cuda_driver.h"
#include "frame_handler.h"
#include "audio_handler.h"
#include "vulkan_renderer.h"
#include "pylog.h"
// #include "core/session.h"
// #include "core/takion.h"
// #include "core/remote/holepunch.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdexcept>
#include <string>
#include <cstring>
#include <optional> // Required for std::optional

#define PYBIND11_DETAILED_ERROR_MESSAGES
#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>
#include <pybind11/complex.h>
#include <pybind11/functional.h>

extern "C"
{
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/imgutils.h>
#include <libavutil/hwcontext.h>
#include <libswscale/swscale.h>
}

namespace py = pybind11;

// Wrap interleaved int16 PCM as a (frames, channels) numpy array; (0, 0) when there is none yet.
static py::array_t<int16_t> pcm_to_array(const std::vector<int16_t> &pcm, size_t channels)
{
    if (channels == 0 || pcm.empty())
        return py::array_t<int16_t>(std::vector<py::ssize_t>{0, 0});
    py::array_t<int16_t> arr({(py::ssize_t)(pcm.size() / channels), (py::ssize_t)channels});
    std::memcpy(arr.mutable_data(), pcm.data(), pcm.size() * sizeof(int16_t));
    return arr;
}

PYBIND11_MODULE(chiaki_py, m)
{
    m.doc() = "Low-level pybind11 bindings around Chiaki, the PS4/PS5 Remote Play client library: "
              "discover consoles (DiscoveryManager), register with one (Backend), connect and stream "
              "one (ChiakiPySession) and pull its decoded frames (FrameHandler and subclasses). Most "
              "users want the pythonic wrapper in the `chiaki_py` package instead of this module directly.";

#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    init_pylog();

    auto m_core = m.def_submodule("core", "The core submodule.");
    // auto m_core_takion = m_core.def_submodule("takion", "The takion submodule.");
    auto m_core_common = m_core.def_submodule("common", "The common submodule.");
    auto m_core_audio = m_core.def_submodule("audio", "The audio submodule.");
    auto m_core_base64 = m_core.def_submodule("base64", "The base64 submodule.");
    auto m_core_bitstream = m_core.def_submodule("bitstream", "The bitstream submodule.");
    auto m_core_controller = m_core.def_submodule("controller", "The controller submodule.");
    auto m_core_discovery_service = m_core.def_submodule("discovery_service", "The discovery service submodule.");
    auto m_core_ecdh = m_core.def_submodule("ecdh", "The ecdh submodule.");
    auto m_core_fec = m_core.def_submodule("fec", "The fec submodule.");
    auto m_core_feedback = m_core.def_submodule("feedback", "The feedback submodule.");
    auto m_core_log = m_core.def_submodule("log", "The log submodule.");
    // auto m_core_session = m_core.def_submodule("session", "The session submodule.");

    // auto m_remote = m.def_submodule("remote", "The remote submodule.");
    // auto m_remote_holepunch = m.def_submodule("holepunch", "The holepunch submodule.");

    init_event_source(m);
    init_core_common(m_core_common);
    init_core_audio(m_core_audio);
    init_core_base64(m_core_base64);
    init_core_log(m_core_log);
    init_core_bitstream(m_core_bitstream);
    init_core_controller(m_core_controller);
    // init_core_discovery_service(m_core_discovery_service);
    init_core_ecdh(m_core_ecdh);
    init_core_fec(m_core_fec);
    init_core_feedback(m_core_feedback);
    init_backend(m);
    // init_core_session(m_core_session);
    // init_core_remote_holepunch(m_remote_holepunch);

    py::enum_<RumbleHapticsIntensity>(m, "RumbleHapticsIntensity",
        "How strongly the controller rumbles for haptic feedback, from Settings.get/set_rumble_haptics_intensity().")
        .value("Off", RumbleHapticsIntensity::Off)
        .value("VeryWeak", RumbleHapticsIntensity::VeryWeak)
        .value("Weak", RumbleHapticsIntensity::Weak)
        .value("Normal", RumbleHapticsIntensity::Normal)
        .value("Strong", RumbleHapticsIntensity::Strong)
        .value("VeryStrong", RumbleHapticsIntensity::VeryStrong)
        .export_values();

    py::enum_<Decoder>(m, "Decoder",
        "Which decoder implementation Settings.get/set_decoder() selects: Ffmpeg (the normal path, used "
        "by all of FrameHandler's subclasses) or Pi (the Raspberry Pi hardware decoder).")
        .value("Ffmpeg", Decoder::Ffmpeg)
        .value("Pi", Decoder::Pi)
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
        "The stream's negotiated video settings, as returned by ChiakiPySession.get_video_profile().")
        .def_readwrite("width", &ChiakiConnectVideoProfile::width)
        .def_readwrite("height", &ChiakiConnectVideoProfile::height)
        .def_readwrite("max_fps", &ChiakiConnectVideoProfile::max_fps)
        .def_readwrite("bitrate", &ChiakiConnectVideoProfile::bitrate)
        .def_readwrite("codec", &ChiakiConnectVideoProfile::codec);

    py::class_<VulkanFrame>(m, "VulkanFrame",
                         "A decoded video frame still resident in GPU memory. Keeps the decoder's frame "
                         "pool slot and device alive until dropped, so release it promptly. The integers "
                         "are raw handles for interop; what they point to depends on hw_type "
                         "('vulkan': data[0] is an AVVkFrame*, 'cuda': one device pointer per plane, "
                         "'d3d11va': data[0] is an ID3D11Texture2D* and data[1] its array slice).")
        .def_property_readonly("hw_type", &VulkanFrame::hw_type, "Hardware decoder type, e.g. 'vulkan', 'cuda', 'd3d11va'.")
        .def_property_readonly("format", &VulkanFrame::format, "Pixel format of the frame itself, e.g. 'vulkan', 'cuda', 'd3d11'.")
        .def_property_readonly("sw_format", &VulkanFrame::sw_format, "Underlying pixel layout on the GPU, e.g. 'nv12' or 'p010le'.")
        .def_property_readonly("width", [](const VulkanFrame &f) { f.require_frame(); return f.frame->width; })
        .def_property_readonly("height", [](const VulkanFrame &f) { f.require_frame(); return f.frame->height; })
        .def_property_readonly("pts", [](const VulkanFrame &f) { return f.pts; }, "Presentation time in seconds.")
        .def_property_readonly("duration", [](const VulkanFrame &f) { return f.duration; }, "Frame duration in seconds.")
        .def_property_readonly("data", &VulkanFrame::data, "Raw per-plane pointers / handles (integers).")
        .def_property_readonly("linesize", &VulkanFrame::linesize, "Row stride in bytes for each entry of `data`.")
        .def_property_readonly("device_hwctx", &VulkanFrame::device_hwctx,
                               "Address of the hardware device context struct (AVVulkanDeviceContext*, "
                               "AVCUDADeviceContext*, ...) the frame lives on.");

    py::class_<FrameHandler>(m, "FrameHandler",
                             "Base class of the frame handlers, which pull decoded frames out of a ChiakiPySession "
                             "in different forms (CpuFrameHandler, CudaFrameHandler, VulkanFrameHandler). Not "
                             "instantiable itself.")
        .def("get_frame", &FrameHandler::get_frame,
             py::arg("out") = py::none(),
             "Pull the next decoded video frame, or None if none was available yet. What is returned, and "
             "what `out` may be, depends on the subclass. Raises RuntimeError on decoding failure.")
        .def_static("empty_frame", &FrameHandler::empty_frame,
                    py::arg("width") = 0, py::arg("height") = 0,
                    "Not implemented on the base class; call empty_frame() on a concrete subclass instead.");

    py::class_<AudioHandler>(m, "AudioHandler")
        .def("get_frame", &AudioHandler::GetFrame, py::arg("max_frames") = 0,
             "Remove and return up to max_frames queued audio frames (0 = all) as an int16 array of shape (frames, channels).")
        .def("get_audio_queued_frames", &AudioHandler::GetAudioQueuedFrames, "Get the number of audio frames waiting in the queue.")
        .def("clear_audio_queue", &AudioHandler::ClearAudioQueue, "Drop every queued audio frame.")
        .def("get_audio_queue_max_frames", &AudioHandler::GetAudioQueueMaxFrames,
             "Get the queue capacity in frames; the oldest frames are dropped beyond it.")
        .def("set_audio_queue_max_frames", &AudioHandler::SetAudioQueueMaxFrames, py::arg("max_frames"),
             "Set the queue capacity in frames (0 = 3x the audio buffer size from Settings).")
        .def("get_audio_channels", &AudioHandler::GetAudioChannels, "Get the number of audio channels.")
        .def("get_audio_rate", &AudioHandler::GetAudioRate, "Get the audio sample rate in Hz.");

    py::class_<CpuFrameHandler, FrameHandler>(m, "CpuFrameHandler",
        "Decodes and converts frames to RGB entirely on the CPU. Works with any decoder, needs no "
        "hardware decoder configured, and is the default FrameHandler `chiaki_py.Session` uses.")
        .def(py::init<ChiakiPySession *>(), py::arg("stream_session"), py::keep_alive<1, 2>())
        .def("get_frame", &CpuFrameHandler::get_frame,
             py::arg("out") = py::none(),
             "Pull the next decoded video frame as a (height, width, 3) uint8 RGB array, or None if "
             "none was available yet. If `out` is given it must already have the frame's exact shape "
             "(C-contiguous, uint8, writable); the frame is written into it and `out` is returned, "
             "otherwise a new array is allocated. Raises RuntimeError on decoding failure and "
             "TypeError/ValueError for an unusable `out`.")
        .def_static("empty_frame", &CpuFrameHandler::empty_frame, py::arg("width"), py::arg("height"),
             "A (height, width, 3) uint8 numpy array, ready to be reused as `out` for get_frame() of frames "
             "of this size.");

    py::class_<CudaFrameHandler, FrameHandler>(m, "CudaFrameHandler",
        "Converts decoded frames to RGB on the GPU with CUDA and returns them as a CuPy (or PyTorch) "
        "array, without a round trip through system memory. Requires an NVIDIA GPU and "
        "Settings.set_hardware_decoder('cuda') before connecting.")
        .def(py::init<ChiakiPySession *>(), py::arg("stream_session"), py::keep_alive<1, 2>())
        .def("get_frame", &CudaFrameHandler::get_frame,
             py::arg("out") = py::none(),
             "Pull the next decoded video frame on the GPU as a (height, width, 3) uint8 RGB CuPy array, "
             "converted on the GPU, or None if none was available yet. If `out` is given - a writable "
             "uint8 CUDA array such as a torch tensor or cupy array, exposing __cuda_array_interface__, "
             "shaped (height, width, 3) and C-contiguous ('channels last'), or (3, height, width) as a "
             "transpose/permute view over that same interleaved memory ('channels first', e.g. from "
             "empty_frame(channels_last=False)) - the frame is converted into it instead and `out` is "
             "returned; otherwise a new CuPy array is allocated. The conversion has finished when this "
             "returns. Requires hardware_decoder='cuda' (RuntimeError otherwise); a bad `out` raises "
             "TypeError/ValueError.")
        .def_static("empty_frame", &CudaFrameHandler::empty_frame,
             py::arg("width"), py::arg("height"), py::arg("backend") = "cupy", py::arg("channels_last") = true,
             "A uint8 CUDA array, ready to be reused as `out` for get_frame() of frames of this size. "
             "`backend` is 'cupy' (default) or 'torch', selecting what allocates it - torch requires a "
             "CUDA build of PyTorch and returns a tensor on 'cuda'. With `channels_last` true (default) "
             "it is shaped (height, width, 3); with it false, (3, height, width) instead - a "
             "transpose/permute view of that same interleaved RGB memory, for a model that wants CHW, "
             "with no extra copy.");

    py::class_<VulkanFrameHandler, FrameHandler>(m, "VulkanFrameHandler",
        "Hands out frames as VulkanFrame objects that stay resident on the Vulkan device the decoder "
        "decoded them on - no conversion, no copy. Requires Settings.set_hardware_decoder('vulkan') "
        "before connecting; pair with VulkanRenderer to draw them, e.g. via chiaki_py.gui.VulkanVideoWidget.")
        .def(py::init<ChiakiPySession *>(), py::arg("stream_session"), py::keep_alive<1, 2>())
        .def("get_frame", &VulkanFrameHandler::get_frame,
             py::arg("out") = py::none(),
             "Pull the next decoded video frame without leaving GPU memory. Returns a VulcanFrame, or None "
             "if none was available yet. If `out` is a VulcanFrame it takes over the new frame (releasing "
             "the one it held) and is returned instead of a new VulcanFrame being created. Raises "
             "RuntimeError if the session doesn't use a hardware decoder and TypeError if `out` isn't a VulcanFrame.")
        .def_static("empty_frame", &VulkanFrameHandler::empty_frame,
             py::arg("width") = 0, py::arg("height") = 0,
             "An empty VulkanFrame holding no decoded frame yet, ready to be reused as `out` for get_frame(). "
             "`width`/`height` are accepted for a uniform empty_frame(width, height) signature but otherwise "
             "unused, since a VulkanFrame takes on whatever size the next decoded frame actually has. Does no "
             "GPU work, unlike VulkanFrame.black(): use that directly if you need a real placeholder picture "
             "and can guarantee it won't run concurrently with the hardware decoder's own Vulkan submissions.");

    py::class_<Settings>(m, "Settings",
        "In-memory connection/decoding/UI settings, passed to ChiakiPySessionConnectInfo, Backend and "
        "DiscoveryManager. A fresh instance starts at chiaki-ng's defaults; there is no persistence "
        "here (use chiaki_py.Serializer to save/load whichever of these settings an application cares "
        "about). Most get_/set_ pairs are self-explanatory config knobs; set_hardware_decoder() and "
        "set_log_level()/set_log_verbose() are the ones most callers need to touch directly.")
        .def(py::init<>())
        .def("get_audio_video_disabled", &Settings::GetAudioVideoDisabled, "Get the audio/video disabled.")
        .def("get_log_verbose", &Settings::GetLogVerbose, "Get the log verbose.")
        .def("set_log_verbose", &Settings::SetLogVerbose, py::arg("log_verbose") , "Set the log verbose.")
        .def("get_log_level", &Settings::GetLogLevel, "Get the least severe log level that is still logged.")
        .def("set_log_level", &Settings::SetLogLevel, py::arg("log_level"),
             "Set the least severe log level that is still logged, e.g. LogLevel.WARNING to hide the "
             "INFO chatter (the default, LogLevel.DEBUG, logs everything). VERBOSE is additionally "
             "controlled by set_log_verbose.")
        .def("get_log_level_mask", &Settings::GetLogLevelMask, "Get the log level mask.")
        .def("get_rumble_haptics_intensity", &Settings::GetRumbleHapticsIntensity, "Get the rumble haptics intensity.")
        .def("set_rumble_haptics_intensity", &Settings::SetRumbleHapticsIntensity, py::arg("rumble_haptics_intensity"), "Set the rumble haptics intensity.")
        .def("get_buttons_by_position", &Settings::GetButtonsByPosition, "Get the buttons by position.")
        .def("set_buttons_by_position", &Settings::SetButtonsByPosition, py::arg("buttons_by_position"), "Set the buttons by position.")
        .def("get_start_mic_unmuted", &Settings::GetStartMicUnmuted, "Get the start mic unmuted.")
        .def("set_start_mic_unmuted", &Settings::SetStartMicUnmuted, py::arg("start_mic_unmuted"), "Set the start mic unmuted.")
        .def("get_haptic_override", &Settings::GetHapticOverride, "Get the haptic override.")
        .def("set_haptic_override", &Settings::SetHapticOverride, py::arg("haptic_override"), "Set the haptic override.")
        .def("get_resolution_local_ps4", &Settings::GetResolutionLocalPS4, "Get the local PS4 resolution.")
        .def("get_resolution_remote_ps4", &Settings::GetResolutionRemotePS4, "Get the remote PS4 resolution.")
        .def("get_resolution_local_ps5", &Settings::GetResolutionLocalPS5, "Get the local PS5 resolution.")
        .def("get_resolution_remote_ps5", &Settings::GetResolutionRemotePS5, "Get the remote PS5 resolution.")
        .def("set_resolution_local_ps4", &Settings::SetResolutionLocalPS4, py::arg("resolution_local_ps4"), "Set the local PS4 resolution.")
        .def("set_resolution_remote_ps4", &Settings::SetResolutionRemotePS4, py::arg("resolution_remote_ps4"), "Set the remote PS4 resolution.")
        .def("set_resolution_local_ps5", &Settings::SetResolutionLocalPS5, py::arg("resolution_local_ps5"), "Set the local PS5 resolution.")
        .def("set_resolution_remote_ps5", &Settings::SetResolutionRemotePS5, py::arg("resolution_remote_ps5"), "Set the remote PS5 resolution.")
        .def("get_fpslocal_ps4", &Settings::GetFPSLocalPS4, "Get the local PS4 FPS.")
        .def("get_fpsremote_ps4", &Settings::GetFPSRemotePS4, "Get the remote PS4 FPS.")
        .def("get_fpslocal_ps5", &Settings::GetFPSLocalPS5, "Get the local PS5 FPS.")
        .def("get_fpsremote_ps5", &Settings::GetFPSRemotePS5, "Get the remote PS5 FPS.")
        .def("set_fpslocal_ps4", &Settings::SetFPSLocalPS4, py::arg("fps_local_ps4"), "Set the local PS4 FPS.")
        .def("set_fpsremote_ps4", &Settings::SetFPSRemotePS4, py::arg("fps_remote_ps4"), "Set the remote PS4 FPS.")
        .def("set_fpslocal_ps5", &Settings::SetFPSLocalPS5, py::arg("fps_local_ps5"), "Set the local PS5 FPS.")
        .def("set_fpsremote_ps5", &Settings::SetFPSRemotePS5, py::arg("fps_remote_ps5"), "Set the remote PS5 FPS.")
        .def("get_bitrate_local_ps4", &Settings::GetBitrateLocalPS4, "Get the local PS4 bitrate.")
        .def("get_bitrate_remote_ps4", &Settings::GetBitrateRemotePS4, "Get the remote PS4 bitrate.")
        .def("get_bitrate_local_ps5", &Settings::GetBitrateLocalPS5, "Get the local PS5 bitrate.")
        .def("get_bitrate_remote_ps5", &Settings::GetBitrateRemotePS5, "Get the remote PS5 bitrate.")
        .def("set_bitrate_local_ps4", &Settings::SetBitrateLocalPS4, py::arg("bitrate_local_ps4"), "Set the local PS4 bitrate.")
        .def("set_bitrate_remote_ps4", &Settings::SetBitrateRemotePS4, py::arg("bitrate_remote_ps4"), "Set the remote PS4 bitrate.")
        .def("set_bitrate_local_ps5", &Settings::SetBitrateLocalPS5, py::arg("bitrate_local_ps5"), "Set the local PS5 bitrate.")
        .def("set_bitrate_remote_ps5", &Settings::SetBitrateRemotePS5, py::arg("bitrate_remote_ps5"), "Set the remote PS5 bitrate.")
        .def("get_codec_ps4", &Settings::GetCodecPS4, "Get the PS4 codec.")
        .def("get_codec_local_ps5", &Settings::GetCodecLocalPS5, "Get the local PS5 codec.")
        .def("get_codec_remote_ps5", &Settings::GetCodecRemotePS5, "Get the remote PS5 codec.")
        .def("set_codec_ps4", &Settings::SetCodecPS4, py::arg("codec_ps4"), "Set the PS4 codec.")
        .def("set_codec_local_ps5", &Settings::SetCodecLocalPS5, py::arg("codec_local_ps5"), "Set the local PS5 codec.")
        .def("set_codec_remote_ps5", &Settings::SetCodecRemotePS5, py::arg("codec_remote_ps5"), "Set the remote PS5 codec.")
        .def("get_display_target_contrast", &Settings::GetDisplayTargetContrast, "Get the display target contrast.")
        .def("set_display_target_contrast", &Settings::SetDisplayTargetContrast, py::arg("display_target_contrast"), "Set the display target contrast.")
        .def("get_display_target_peak", &Settings::GetDisplayTargetPeak, "Get the display target peak.")
        .def("set_display_target_peak", &Settings::SetDisplayTargetPeak, py::arg("display_target_peak"), "Set the display target peak.")
        .def("get_display_target_trc", &Settings::GetDisplayTargetTrc, "Get the display target TRC.")
        .def("set_display_target_trc", &Settings::SetDisplayTargetTrc, py::arg("display_target_trc"), "Set the display target TRC.")
        .def("get_display_target_prim", &Settings::GetDisplayTargetPrim, "Get the display target PRIM.")
        .def("set_display_target_prim", &Settings::SetDisplayTargetPrim, py::arg("display_target_prim"), "Set the display target PRIM.")
        .def("get_decoder", &Settings::GetDecoder, "Get the decoder.")
        .def("set_decoder", &Settings::SetDecoder, py::arg("decoder"), "Set the decoder.")
        .def("get_hardware_decoder", &Settings::GetHardwareDecoder, "Get the hardware decoder.")
        .def("set_hardware_decoder", &Settings::SetHardwareDecoder, py::arg("hardware_decoder"), "Set the hardware decoder.")
        .def("get_packet_loss_max", &Settings::GetPacketLossMax, "Get the packet loss max.")
        .def("set_packet_loss_max", &Settings::SetPacketLossMax, py::arg("packet_loss_max"), "Set the packet loss max.")
        .def("get_audio_volume", &Settings::GetAudioVolume, "Get the audio volume.")
        .def("set_audio_volume", &Settings::SetAudioVolume, py::arg("audio_volume"), "Set the audio volume.")
        .def("get_audio_buffer_size_default", &Settings::GetAudioBufferSizeDefault, "Get the audio buffer size default.")
        .def("get_audio_buffer_size_raw", &Settings::GetAudioBufferSizeRaw, "Get the audio buffer size raw.")
        .def("get_audio_buffer_size", &Settings::GetAudioBufferSize, "Get the audio buffer size.")
        .def("set_audio_buffer_size", &Settings::SetAudioBufferSize, py::arg("audio_buffer_size"), "Set the audio buffer size.")
        .def("get_audio_out_device", &Settings::GetAudioOutDevice, "Get the audio out device.")
        .def("set_audio_out_device", &Settings::SetAudioOutDevice, py::arg("audio_out_device"), "Set the audio out device.")
        .def("get_audio_in_device", &Settings::GetAudioInDevice, "Get the audio in device.")
        .def("set_audio_in_device", &Settings::SetAudioInDevice, py::arg("audio_in_device"), "Set the audio in device.")
        .def("get_psn_auth_token", &Settings::GetPsnAuthToken, "Get the PSN auth token.")
        .def("set_psn_auth_token", &Settings::SetPsnAuthToken, py::arg("psn_auth_token"), "Set the PSN auth token.")
        .def("get_dpad_touch_enabled", &Settings::GetDpadTouchEnabled, "Get the D-pad touch enabled.")
        .def("set_dpad_touch_enabled", &Settings::SetDpadTouchEnabled, py::arg("dpad_touch_enabled"), "Set the D-pad touch enabled.")
        .def("get_dpad_touch_increment", &Settings::GetDpadTouchIncrement, "Get the D-pad touch increment.")
        .def("set_dpad_touch_increment", &Settings::SetDpadTouchIncrement, py::arg("dpad_touch_increment"), "Set the D-pad touch increment.")
        .def("get_dpad_touch_shortcut1", &Settings::GetDpadTouchShortcut1, "Get the D-pad touch shortcut 1.")
        .def("set_dpad_touch_shortcut1", &Settings::SetDpadTouchShortcut1, py::arg("dpad_touch_shortcut1"), "Set the D-pad touch shortcut 1.")
        .def("get_dpad_touch_shortcut2", &Settings::GetDpadTouchShortcut2, "Get the D-pad touch shortcut 2.")
        .def("set_dpad_touch_shortcut2", &Settings::SetDpadTouchShortcut2, py::arg("dpad_touch_shortcut2"), "Set the D-pad touch shortcut 2.")
        .def("get_dpad_touch_shortcut3", &Settings::GetDpadTouchShortcut3, "Get the D-pad touch shortcut 3.")
        .def("set_dpad_touch_shortcut3", &Settings::SetDpadTouchShortcut3, py::arg("dpad_touch_shortcut3"), "Set the D-pad touch shortcut 3.")
        .def("get_dpad_touch_shortcut4", &Settings::GetDpadTouchShortcut4, "Get the D-pad touch shortcut 4.")
        .def("set_dpad_touch_shortcut4", &Settings::SetDpadTouchShortcut4, py::arg("dpad_touch_shortcut4"), "Set the D-pad touch shortcut 4.")
        .def("get_psn_account_id", &Settings::GetPsnAccountId, "Get the PSN account ID.")
        .def("set_psn_account_id", &Settings::SetPsnAccountId, py::arg("psn_account_id"), "Set the PSN account ID.")
        .def("get_video_profile_local_ps4", &Settings::GetVideoProfileLocalPS4, "Get the local PS4 video profile.")
        .def("get_video_profile_remote_ps4", &Settings::GetVideoProfileRemotePS4, "Get the remote PS4 video profile.")
        .def("get_video_profile_local_ps5", &Settings::GetVideoProfileLocalPS5, "Get the local PS5 video profile.")
        .def("get_video_profile_remote_ps5", &Settings::GetVideoProfileRemotePS5, "Get the remote PS5 video profile.")
        .def_static("get_chiaki_controller_button_name", &Settings::GetChiakiControllerButtonName, py::arg("chiaki_button"), "Get the Chiaki controller button name.")
        .def("set_controller_button_mapping", &Settings::SetControllerButtonMapping, py::arg("chiaki_button"), py::arg("key"), "Set the controller button mapping.")
        .def("get_controller_mapping", &Settings::GetControllerMapping, "Get the controller mapping.")
        .def("get_controller_mapping_for_decoding", &Settings::GetControllerMappingForDecoding, "Get the controller mapping for decoding.")
        .def("__repr__", [](const Settings &s)
             {
                std::ostringstream repr;
                repr << "<Settings("
                    << "logVerbose=" << (s.GetLogVerbose() ? "True" : "False") << ", "
                    << "logLevelMask=" << s.GetLogLevelMask() << ", "
                    << "rumbleHapticsIntensity=" << static_cast<int>(s.GetRumbleHapticsIntensity()) << ", "
                    << "buttonsByPosition=" << (s.GetButtonsByPosition() ? "True" : "False") << ", "
                    << "startMicUnmuted=" << (s.GetStartMicUnmuted() ? "True" : "False") << ", "
                    << "hapticOverride=" << s.GetHapticOverride() << ", "
                    << "resolutionLocalPS4=" << static_cast<int>(s.GetResolutionLocalPS4()) << ", "
                    << "resolutionRemotePS4=" << static_cast<int>(s.GetResolutionRemotePS4()) << ", "
                    << "fpsLocalPS4=" << static_cast<int>(s.GetFPSLocalPS4()) << ", "
                    << "fpsRemotePS4=" << static_cast<int>(s.GetFPSRemotePS4()) << ", "
                    << "bitrateLocalPS4=" << s.GetBitrateLocalPS4() << ", "
                    << "bitrateRemotePS4=" << s.GetBitrateRemotePS4() << ", "
                    << "codecPS4=" << static_cast<int>(s.GetCodecPS4()) << ", "
                    << "decoder=" << static_cast<int>(s.GetDecoder()) << ", "
                    << "hardwareDecoder='" << s.GetHardwareDecoder() << "', "
                    << "packetLossMax=" << s.GetPacketLossMax() << ", "
                    << "audioVolume=" << s.GetAudioVolume() << ", "
                    << "audioBufferSize=" << s.GetAudioBufferSize() << ", "
                    << "audioOutDevice='" << s.GetAudioOutDevice() << "', "
                    << "audioInDevice='" << s.GetAudioInDevice() << "', "
                    << "psnAccountId='" << s.GetPsnAccountId() << "'"
                    << ")>";
                return repr.str(); });

    py::class_<ChiakiPySessionConnectInfo>(m, "ChiakiPySessionConnectInfo",
                                           "Everything ChiakiPySession needs to open a connection to an already-registered console. "
                                           "Built from the registration a prior Backend.register_host() produced (`host`, `nickname`, "
                                           "`regist_key`, `morning` i.e. the RP key, `target`) plus `settings`; the pythonic "
                                           "`chiaki_py.Session.connect()` builds one of these from a `HostRegistration` for you.")
        .def(py::init<>())
        .def(py::init<Settings *,
                      ChiakiTarget,
                      std::string,
                      std::string,
                      std::string &,
                      py::bytes,
                      std::string,
                      std::string,
                      bool,
                      bool,
                      bool,
                      bool>(),
             py::arg("settings"), py::arg("target"), py::arg("host"), py::arg("nickname"),
             py::arg("regist_key"), py::arg("morning"), py::arg("initial_login_pin"),
             py::arg("duid"), py::arg("auto_regist"), py::arg("fullscreen"),
             py::arg("zoom"), py::arg("stretch"));

    py::class_<ChiakiPySession>(m, "ChiakiPySession",
                                "A live or about-to-be-started connection to a console: start()/stop() it, feed it "
                                "controller/motion input with the press_*/release_*/set_* methods, and pull decoded video "
                                "through a FrameHandler built around it (see FrameHandler.get_frame() and the CpuFrameHandler/ "
                                "CudaFrameHandler/VulkanFrameHandler subclasses). The on_*() methods each return an event "
                                "source frames/state changes can be subscribed to. Usually built and driven indirectly, via "
                                "chiaki_py.Session, rather than used directly.")
        .def(py::init<const ChiakiPySessionConnectInfo &>(), py::arg("connect_info"))
        .def("start", &ChiakiPySession::Start, "Start the stream session.")
        .def("stop", &ChiakiPySession::Stop, "Stop the stream session.")
        .def("go_to_bed", &ChiakiPySession::GoToBed, "Go to bed.")
        .def("set_login_pin", &ChiakiPySession::SetLoginPIN, py::arg("pin"), "Set the login PIN.")
        .def("go_home", &ChiakiPySession::GoHome, "Go home.")
        .def("get_host", &ChiakiPySession::GetHost, "Get the host.")
        .def("is_connected", &ChiakiPySession::IsConnected, "Check if connected.")
        .def("is_connecting", &ChiakiPySession::IsConnecting, "Check if connecting.")
        .def("get_measured_bitrate", &ChiakiPySession::GetMeasuredBitrate, "Get the measured bitrate.")
        .def("get_average_packet_loss", &ChiakiPySession::GetAveragePacketLoss, "Get the average packet loss.")
        .def("get_muted", &ChiakiPySession::GetMuted, "Get the muted status.")
        .def("set_audio_volume", &ChiakiPySession::SetAudioVolume, py::arg("volume"), "Set the audio volume.")
        .def("get_cant_display", &ChiakiPySession::GetCantDisplay, "Get the cant display status.")
        .def("get_ffmpeg_decoder", &ChiakiPySession::GetFfmpegDecoder, "Get the FFmpeg decoder.", py::return_value_policy::reference)
        .def("has_hardware_decoder", [](ChiakiPySession &s)
             {
            ChiakiFfmpegDecoder *decoder = s.GetFfmpegDecoder();
            return decoder && decoder->hw_device_ctx; }, "Whether the video decoder is hardware-accelerated (i.e. get_frame_gpu() can return frames).")
        .def("get_video_profile", &ChiakiPySession::GetVideoProfile, "The stream's video profile (width, height, max_fps, bitrate, codec): the requested one until the "
                                                                     "console answers, then the negotiated one. Known before the first frame arrives.")
        .def("get_audio_handler", &ChiakiPySession::GetAudioHandler, py::return_value_policy::reference)
        .def("hardware_decoder_type", [](ChiakiPySession &s) -> std::string
             {
            ChiakiFfmpegDecoder *decoder = s.GetFfmpegDecoder();
            if (!decoder || !decoder->hw_device_ctx)
                return "";
            return av_hwdevice_get_type_name(reinterpret_cast<AVHWDeviceContext *>(decoder->hw_device_ctx->data)->type); }, "Type of hardware decoder in use ('vulkan', 'cuda', 'd3d11va', ...), or '' if decoding on the CPU.")
        .def("on_audio_frame_available", &ChiakiPySession::OnAudioFrameAvailable, "Retrieve the Audio frame available event.", py::return_value_policy::reference)
        .def("on_frame_available", &ChiakiPySession::OnFfmpegFrameAvailable, "Retrieve the FFmpeg frame available event.", py::return_value_policy::reference)
        .def("on_session_quit", &ChiakiPySession::OnSessionQuit, "Retrieve the session quit event.", py::return_value_policy::reference)
        .def("on_login_pin_requested", &ChiakiPySession::OnLoginPINRequested, "Retrieve the login PIN requested event.", py::return_value_policy::reference)
        .def("on_data_holepunch_progress", &ChiakiPySession::OnDataHolepunchProgress, "Retrieve the data holepunch progress event.", py::return_value_policy::reference)
        .def("on_auto_regist_succeeded", &ChiakiPySession::OnAutoRegistSucceeded, "Retrieve the auto-registration succeeded event.", py::return_value_policy::reference)
        .def("on_nickname_received", &ChiakiPySession::OnNicknameReceived, "Retrieve the nickname received event.", py::return_value_policy::reference)
        .def("on_connected_changed", &ChiakiPySession::OnConnectedChanged, "Retrieve the connected changed event.", py::return_value_policy::reference)
        .def("on_measured_bitrate_changed", &ChiakiPySession::OnMeasuredBitrateChanged, "Retrieve the measured bitrate changed event.", py::return_value_policy::reference)
        .def("on_average_packet_loss_changed", &ChiakiPySession::OnAveragePacketLossChanged, "Retrieve the average packet loss changed event.", py::return_value_policy::reference)
        .def("on_cant_display_changed", &ChiakiPySession::OnCantDisplayChanged, "Retrieve the cant display changed event.", py::return_value_policy::reference)
        .def("press_cross", &ChiakiPySession::pressCross, "Press the cross button.")
        .def("release_cross", &ChiakiPySession::releaseCross, "Release the cross button.")
        .def("press_circle", &ChiakiPySession::pressCircle, "Press the circle button.")
        .def("release_circle", &ChiakiPySession::releaseCircle, "Release the circle button.")
        .def("press_square", &ChiakiPySession::pressSquare, "Press the square button.")
        .def("release_square", &ChiakiPySession::releaseSquare, "Release the square button.")
        .def("press_triangle", &ChiakiPySession::pressTriangle, "Press the triangle button.")
        .def("release_triangle", &ChiakiPySession::releaseTriangle, "Release the triangle button.")
        .def("press_left", &ChiakiPySession::pressLeft, "Press the left button.")
        .def("release_left", &ChiakiPySession::releaseLeft, "Release the left button.")
        .def("press_right", &ChiakiPySession::pressRight, "Press the right button.")
        .def("release_right", &ChiakiPySession::releaseRight, "Release the right button.")
        .def("press_up", &ChiakiPySession::pressUp, "Press the up button.")
        .def("release_up", &ChiakiPySession::releaseUp, "Release the up button.")
        .def("press_down", &ChiakiPySession::pressDown, "Press the down button.")
        .def("release_down", &ChiakiPySession::releaseDown, "Release the down button.")
        .def("press_l1", &ChiakiPySession::pressL1, "Press the L1 button.")
        .def("release_l1", &ChiakiPySession::releaseL1, "Release the L1 button.")
        .def("press_r1", &ChiakiPySession::pressR1, "Press the R1 button.")
        .def("release_r1", &ChiakiPySession::releaseR1, "Release the R1 button.")
        .def("press_l3", &ChiakiPySession::pressL3, "Press the L3 button.")
        .def("release_l3", &ChiakiPySession::releaseL3, "Release the L3 button.")
        .def("press_r3", &ChiakiPySession::pressR3, "Press the R3 button.")
        .def("release_r3", &ChiakiPySession::releaseR3, "Release the R3 button.")
        .def("press_options", &ChiakiPySession::pressOptions, "Press the options button.")
        .def("release_options", &ChiakiPySession::releaseOptions, "Release the options button.")
        .def("press_create", &ChiakiPySession::pressCreate, "Press the create button.")
        .def("release_create", &ChiakiPySession::releaseCreate, "Release the create button.")
        .def("press_touchpad", &ChiakiPySession::pressTouchpad, "Press the touchpad button.")
        .def("release_touchpad", &ChiakiPySession::releaseTouchpad, "Release the touchpad button.")
        .def("press_ps", &ChiakiPySession::pressPS, "Press the PS button.")
        .def("release_ps", &ChiakiPySession::releasePS, "Release the PS button.")
        .def("set_l2", &ChiakiPySession::setL2, py::arg("state"), "Set the L2 trigger state [0, 255].")
        .def("set_r2", &ChiakiPySession::setR2, py::arg("state"), "Set the R2 trigger state [0, 255].")
        .def("set_left_x", &ChiakiPySession::setLeftX, py::arg("x"), "Set the left analog's stick x value [0, 1023].")
        .def("set_left_y", &ChiakiPySession::setLeftY, py::arg("y"), "Set the left analog's stick y value [0, 1023].")
        .def("set_left", &ChiakiPySession::setLeft, py::arg("x"), py::arg("y"), "Set the left analog's stick x and y value [0, 1023].")
        .def("set_right_x", &ChiakiPySession::setRightX, py::arg("x"), "Set the right analog's stick x value [0, 1023].")
        .def("set_right_y", &ChiakiPySession::setRightY, py::arg("y"), "Set the right analog's stick y value [0, 1023].")
        .def("set_right", &ChiakiPySession::setRight, py::arg("x"), py::arg("y"), "Set the right analog's stick x and y value [0, 1023].")

        .def("set_accelerometer_x", &ChiakiPySession::setAccelerometerX, py::arg("x"), "Set the accelerometer x value [0, 1023].")
        .def("set_accelerometer_y", &ChiakiPySession::setAccelerometerY, py::arg("y"), "Set the accelerometer y value [0, 1023].")
        .def("set_accelerometer_z", &ChiakiPySession::setAccelerometerZ, py::arg("z"), "Set the accelerometer z value [0, 1023].")
        .def("set_accelerometer", &ChiakiPySession::setAccelerometer, py::arg("x"), py::arg("y"), py::arg("z"), "Set the accelerometer x, y and z value [0, 1023].")

        .def("set_gyroscope_x", &ChiakiPySession::setGyroscopeX, py::arg("x"), "Set the gyroscope x value [0, 1023].")
        .def("set_gyroscope_y", &ChiakiPySession::setGyroscopeY, py::arg("y"), "Set the gyroscope y value [0, 1023].")
        .def("set_gyroscope_z", &ChiakiPySession::setGyroscopeZ, py::arg("z"), "Set the gyroscope z value [0, 1023].")
        .def("set_gyroscope", &ChiakiPySession::setGyroscope, py::arg("x"), py::arg("y"), py::arg("z"), "Set the gyroscope x, y and z value [0, 1023].")

        .def("set_orientation_x", &ChiakiPySession::setOrientationX, py::arg("x"), "Set the orientation x value [0, 1023].")
        .def("set_orientation_y", &ChiakiPySession::setOrientationY, py::arg("y"), "Set the orientation y value [0, 1023].")
        .def("set_orientation_z", &ChiakiPySession::setOrientationZ, py::arg("z"), "Set the orientation z value [0, 1023].")
        .def("set_orientation_w", &ChiakiPySession::setOrientationW, py::arg("w"), "Set the orientation w value [0, 1023].")
        .def("set_orientation", &ChiakiPySession::setOrientation, py::arg("x"), py::arg("y"), py::arg("z"), py::arg("w"), "Set the orientation x, y, z and w value [0, 1023].")

        .def("send_feedback_state", &ChiakiPySession::SendFeedbackState, "Send the feedback state.");

    py::reinterpret_borrow<py::class_<VulkanFrame>>(m.attr("VulkanFrame"))
        .def_static("upload_nv12", &VulkanFrame::upload_nv12, py::arg("stream_session"), py::arg("nv12"),
                    py::arg("visible_width") = py::none(), py::arg("visible_height") = py::none(),
                    "Upload an NV12 picture from system memory - a C-contiguous uint8 array of shape "
                    "(height * 3 / 2, width): the luma rows, then the interleaved chroma rows - into a new "
                    "VulkanFrame on the session's Vulkan hardware device (RuntimeError if it does not use one), "
                    "as the decoder would have produced it. The frame shows only the top left "
                    "visible_width x visible_height pixels if given, like a decoded picture that is smaller "
                    "than the image it is stored in. For trying out consumers of VulkanFrames, such as "
                    "VulkanRenderer, without a console.")
        .def_static("black", &VulkanFrame::black, py::arg("stream_session"), py::arg("width"), py::arg("height"),
                    "A new all-black width x height VulkanFrame on the session's Vulkan hardware device "
                    "(RuntimeError if it does not use one). What VulkanFrameHandler.empty_frame() returns.");

    py::class_<VulkanRenderer>(m, "VulkanRenderer",
                               "Draws VulkanFrames of the Vulkan hardware decoder into a native window without them "
                               "leaving the GPU: libplacebo converts the frames' NV12/P010 planes to RGB (SDR or "
                               "HDR), scales them and presents them on the same Vulkan device the decoder decodes "
                               "on. Windows only so far. Call from one thread (the GUI thread), and close() before "
                               "the window is destroyed.")
        .def(py::init<ChiakiPySession &, uintptr_t>(), py::arg("stream_session"), py::arg("window"), py::keep_alive<1, 2>(),
             "Draw into the native window `window` (an HWND). The session must use the Vulkan hardware decoder "
             "(Settings.set_hardware_decoder(\"vulkan\")); raises RuntimeError otherwise, or if the window can't be drawn into.")
        .def("render", &VulkanRenderer::render, py::arg("frame"), py::call_guard<py::gil_scoped_release>(),
             "Draw `frame`, a VulkanFrame from the Vulkan decoder, scaled to fit the window with its aspect "
             "ratio kept, and present it. Does nothing while the window has no area (is minimised). Returns "
             "once the drawing is submitted, not finished; the GPU is waited for when the next frame needs it.")
        .def("set_overlay", [](VulkanRenderer &renderer, const py::array_t<uint8_t, py::array::c_style | py::array::forcecast> &rgba, int margin) {
                 if (rgba.ndim() != 3 || rgba.shape(2) != 4)
                     throw py::value_error("The overlay must be an array of shape (height, width, 4)");
                 renderer.set_overlay(rgba.data(), static_cast<int>(rgba.shape(1)), static_cast<int>(rgba.shape(0)), margin);
             },
             py::arg("rgba"), py::arg("margin") = 12,
             "Show `rgba`, a uint8 array of shape (height, width, 4) with premultiplied alpha, on top of the video "
             "in the top-right corner of the window, `margin` pixels from its edges, until it is replaced or cleared. "
             "The pixels are copied. libplacebo composites it in the same pass that draws the video, so it is not "
             "part of the frames, and it stays where it is while the window is resized.")
        .def("clear_overlay", &VulkanRenderer::clear_overlay, "Remove the overlay, if there is one.")
        .def("close", &VulkanRenderer::close, py::call_guard<py::gil_scoped_release>(),
             "Wait for the GPU to be done and free everything. Call before the window is destroyed.")
        .def_static("is_supported", &VulkanRenderer::is_supported, "Whether windows of this platform can be drawn into (only Windows so far).");

    py::enum_<ChiakiDiscoveryHostState>(m, "DiscoveryHostState",
        "A discovered console's power state (DiscoveryHost.state): Ready to stream, Standby (needs a "
        "wakeup packet first, see DiscoveryManager.send_wakeup()), or Unknown before a reply is parsed.")
        .value("Unknown", ChiakiDiscoveryHostState::CHIAKI_DISCOVERY_HOST_STATE_UNKNOWN)
        .value("Ready", ChiakiDiscoveryHostState::CHIAKI_DISCOVERY_HOST_STATE_READY)
        .value("Standby", ChiakiDiscoveryHostState::CHIAKI_DISCOVERY_HOST_STATE_STANDBY)
        .export_values();

    py::class_<HostMAC>(m, "HostMAC",
        "A console's 6-byte Ethernet MAC address, as used to identify a registered/discovered host.")
        .def("to_string", &HostMAC::ToString, "Get the MAC address as a hex string.")
        .def("__str__", &HostMAC::ToString);

    py::class_<DiscoveryHostWrapper>(m, "DiscoveryHost",
        "One console found by DiscoveryManager's broadcast discovery, or filled in by hand to "
        "register a manual one. `ps5`, `host_addr` and `state` (whether it's awake or in standby) "
        "are the fields most callers need; the rest mirrors what the console's discovery reply reports.")
        .def(py::init<>())
        .def("get_host_mac", &DiscoveryHostWrapper::GetHostMAC, "Get the host MAC address.")
        .def_property("ps5", &DiscoveryHostWrapper::getPs5, &DiscoveryHostWrapper::setPs5, "Get or set the PS5.")
        .def_property("state", &DiscoveryHostWrapper::getState, &DiscoveryHostWrapper::setState, "Get or set the state.")
        .def_property("target", &DiscoveryHostWrapper::getTarget, &DiscoveryHostWrapper::setTarget, "Get or set the target.")
        .def_property("host_request_port", &DiscoveryHostWrapper::getHostRequestPort, &DiscoveryHostWrapper::setHostRequestPort, "Get or set the host request port.")
        .def_property("host_addr", &DiscoveryHostWrapper::getHostAddr, &DiscoveryHostWrapper::setHostAddr, "Get or set the host address.")
        .def_property("system_version", &DiscoveryHostWrapper::getSystemVersion, &DiscoveryHostWrapper::setSystemVersion, "Get or set the system version.")
        .def_property("device_discovery_protocol_version", &DiscoveryHostWrapper::getDeviceDiscoveryProtocolVersion, &DiscoveryHostWrapper::setDeviceDiscoveryProtocolVersion, "Get or set the device discovery protocol version.")
        .def_property("host_name", &DiscoveryHostWrapper::getHostName, &DiscoveryHostWrapper::setHostName, "Get or set the host name.")
        .def_property("host_type", &DiscoveryHostWrapper::getHostType, &DiscoveryHostWrapper::setHostType, "Get or set the host type.")
        .def_property("host_id", &DiscoveryHostWrapper::getHostId, &DiscoveryHostWrapper::setHostId, "Get or set the host ID.")
        .def_property("running_app_titleid", &DiscoveryHostWrapper::getRunningAppTitleId, &DiscoveryHostWrapper::setRunningAppTitleId, "Get or set the running app title ID.")
        .def_property("running_app_name", &DiscoveryHostWrapper::getRunningAppName, &DiscoveryHostWrapper::setRunningAppName, "Get or set the running app name.");

    py::class_<DiscoveryManager>(m, "DiscoveryManager",
                                 "Broadcasts for PS4/PS5 hosts on the local network (IPv4 and IPv6) in the background and keeps "
                                 "track of what answered, plus individually pings any manually-added registered hosts from "
                                 "`settings` so they show up even when broadcast can't reach them. `chiaki_py.discover_hosts()` "
                                 "wraps the start/wait/collect/stop sequence this class otherwise requires driving by hand.")
        .def(py::init<>())
        .def("set_active", &DiscoveryManager::SetActive, py::arg("active"),
             "Start or stop broadcasting. Starting re-inits the discovery sockets if they were not "
             "already active; stopping tears them down and clears the discovered host list.")
        .def("set_settings", &DiscoveryManager::SetSettings, py::arg("settings"),
             "Set the Settings this manager reads its log level and manually-registered hosts from, "
             "and refresh the manual per-host discovery services from it immediately.")
        .def("send_wakeup", &DiscoveryManager::SendWakeup, py::arg("host"), py::arg("regist_key"), py::arg("ps5"),
             "Send a wakeup packet to `host` (a registration's `regist_key`, hex-encoded) so a console "
             "in standby powers on. Raises RuntimeError if `regist_key` is malformed or sending fails.")
        .def("get_active", &DiscoveryManager::GetActive, "Whether broadcast discovery is currently running.")
        .def("get_hosts", &DiscoveryManager::GetHosts,
             "The hosts discovered so far: everything the last broadcast round found, plus any manually "
             "probed host currently confirmed reachable. Empty until set_active(True) has had time to hear back.")
        .def("discovery_service_hosts", &DiscoveryManager::DiscoveryServiceHosts, py::arg("hosts"),
             "Replace the broadcast-discovered host list wholesale. Called internally as broadcast replies "
             "come in; not normally needed from Python.")
        .def("update_manual_services", &DiscoveryManager::UpdateManualServices,
             "Re-sync the per-host discovery pings from `settings`' currently registered manual hosts, "
             "starting one for each newly added host and dropping ones no longer configured.");
}