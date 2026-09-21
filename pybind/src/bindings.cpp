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
#include "streamsession.h"
#include "discovery_manager.h"
#include "backend.h"
#include "cuda_driver.h"
#include "frame_handler.h"
// #include "core/session.h"
// #include "core/takion.h"
// #include "core/remote/holepunch.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdexcept>
#include <string>
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



PYBIND11_MODULE(chiaki_py, m)
{
    m.doc() = "Python bindings for Chiaki CLI commands";

#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

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

    py::enum_<RumbleHapticsIntensity>(m, "RumbleHapticsIntensity")
        .value("Off", RumbleHapticsIntensity::Off)
        .value("VeryWeak", RumbleHapticsIntensity::VeryWeak)
        .value("Weak", RumbleHapticsIntensity::Weak)
        .value("Normal", RumbleHapticsIntensity::Normal)
        .value("Strong", RumbleHapticsIntensity::Strong)
        .value("VeryStrong", RumbleHapticsIntensity::VeryStrong)
        .export_values();

    py::enum_<Decoder>(m, "Decoder")
        .value("Ffmpeg", Decoder::Ffmpeg)
        .value("Pi", Decoder::Pi)
        .export_values();

    py::enum_<ChiakiDisableAudioVideo>(m, "DisableAudioVideo")
        .value("None_", ChiakiDisableAudioVideo::CHIAKI_NONE_DISABLED)
        .value("Audio", ChiakiDisableAudioVideo::CHIAKI_AUDIO_DISABLED)
        .value("Video", ChiakiDisableAudioVideo::CHIAKI_VIDEO_DISABLED)
        .value("AudioVideo", ChiakiDisableAudioVideo::CHIAKI_AUDIO_VIDEO_DISABLED)
        .export_values();

    py::enum_<ChiakiVideoResolutionPreset>(m, "VideoResolutionPreset")
        .value("Resolution360p", ChiakiVideoResolutionPreset::CHIAKI_VIDEO_RESOLUTION_PRESET_360p)
        .value("Resolution540p", ChiakiVideoResolutionPreset::CHIAKI_VIDEO_RESOLUTION_PRESET_540p)
        .value("Resolution720p", ChiakiVideoResolutionPreset::CHIAKI_VIDEO_RESOLUTION_PRESET_720p)
        .value("Resolution1080p", ChiakiVideoResolutionPreset::CHIAKI_VIDEO_RESOLUTION_PRESET_1080p)
        .export_values();

    py::enum_<ChiakiVideoFPSPreset>(m, "VideoFPSPreset")
        .value("FPS30", ChiakiVideoFPSPreset::CHIAKI_VIDEO_FPS_PRESET_30)
        .value("FPS60", ChiakiVideoFPSPreset::CHIAKI_VIDEO_FPS_PRESET_60)
        .export_values();

    py::class_<ChiakiFfmpegDecoder>(m, "FfmpegDecoder");

    py::class_<ChiakiConnectVideoProfile>(m, "ChiakiConnectVideoProfile")
        .def_readwrite("width", &ChiakiConnectVideoProfile::width)
        .def_readwrite("height", &ChiakiConnectVideoProfile::height)
        .def_readwrite("max_fps", &ChiakiConnectVideoProfile::max_fps)
        .def_readwrite("bitrate", &ChiakiConnectVideoProfile::bitrate)
        .def_readwrite("codec", &ChiakiConnectVideoProfile::codec);

    py::class_<GpuFrame>(m, "GpuFrame",
                         "A decoded video frame still resident in GPU memory. Keeps the decoder's frame "
                         "pool slot and device alive until dropped, so release it promptly. The integers "
                         "are raw handles for interop; what they point to depends on hw_type "
                         "('vulkan': data[0] is an AVVkFrame*, 'cuda': one device pointer per plane, "
                         "'d3d11va': data[0] is an ID3D11Texture2D* and data[1] its array slice).")
        .def_property_readonly("hw_type", &GpuFrame::hw_type, "Hardware decoder type, e.g. 'vulkan', 'cuda', 'd3d11va'.")
        .def_property_readonly("format", &GpuFrame::format, "Pixel format of the frame itself, e.g. 'vulkan', 'cuda', 'd3d11'.")
        .def_property_readonly("sw_format", &GpuFrame::sw_format, "Underlying pixel layout on the GPU, e.g. 'nv12' or 'p010le'.")
        .def_property_readonly("width", [](const GpuFrame &f) { return f.frame->width; })
        .def_property_readonly("height", [](const GpuFrame &f) { return f.frame->height; })
        .def_property_readonly("pts", [](const GpuFrame &f) { return f.pts; }, "Presentation time in seconds.")
        .def_property_readonly("duration", [](const GpuFrame &f) { return f.duration; }, "Frame duration in seconds.")
        .def_property_readonly("data", &GpuFrame::data, "Raw per-plane pointers / handles (integers).")
        .def_property_readonly("linesize", &GpuFrame::linesize, "Row stride in bytes for each entry of `data`.")
        .def_property_readonly("device_hwctx", &GpuFrame::device_hwctx,
                               "Address of the hardware device context struct (AVVulkanDeviceContext*, "
                               "AVCUDADeviceContext*, ...) the frame lives on.");

    py::class_<CudaPlane, std::shared_ptr<CudaPlane>>(m, "CudaPlane",
                                                      "One plane of a CUDA frame. Implements __cuda_array_interface__, so "
                                                      "torch.as_tensor(plane, device='cuda') and cupy.asarray(plane) wrap it "
                                                      "without copying. It keeps the frame's device memory alive (and its slot in "
                                                      "the decoder's frame pool occupied) for as long as it, or anything made "
                                                      "from it, exists.")
        .def_property_readonly("__cuda_array_interface__", &CudaPlane::cuda_array_interface);

    py::class_<CudaFrame>(m, "CudaFrame",
                          "A decoded frame in CUDA device memory (device 0's primary context, shared with "
                          "PyTorch/CuPy), as NV12 or P010/P016 planes: `y` is (H, W) and `uv` is (H/2, W/2, 2) "
                          "with U and V interleaved. 8-bit samples are uint8; 10/16-bit ones are uint16 with the "
                          "value in the high bits. Colour conversion to RGB is left to the caller.")
        .def_readonly("width", &CudaFrame::width)
        .def_readonly("height", &CudaFrame::height)
        .def_readonly("pts", &CudaFrame::pts, "Presentation time in seconds.")
        .def_readonly("duration", &CudaFrame::duration, "Frame duration in seconds.")
        .def_readonly("sw_format", &CudaFrame::sw_format, "'nv12', 'p010le' or 'p016le'.")
        .def_readonly("y", &CudaFrame::y, "Luma plane, shape (height, width).")
        .def_readonly("uv", &CudaFrame::uv, "Interleaved chroma plane, shape (ceil(height/2), ceil(width/2), 2).");

    py::class_<FrameHandler>(m, "FrameHandler",
                             "Base class of the frame handlers, which pull decoded frames out of a StreamSession "
                             "in different forms (CPUFrameHandler, CUDAFrameHandler, GPUFrameHandler). Not "
                             "instantiable itself.")
        .def("get_frame", &FrameHandler::get_frame,
             py::arg("out") = py::none(),
             "Pull the next decoded video frame, or None if none was available yet. What is returned, and "
             "what `out` may be, depends on the subclass. Raises RuntimeError on decoding failure.");

    py::class_<CPUFrameHandler, FrameHandler>(m, "CPUFrameHandler")
        .def(py::init<StreamSession *>(), py::arg("stream_session"), py::keep_alive<1, 2>())
        .def("get_frame", &CPUFrameHandler::get_frame,
             py::arg("out") = py::none(),
             "Pull the next decoded video frame as a (height, width, 3) uint8 RGB array, or None if "
             "none was available yet. If `out` is given it must already have the frame's exact shape "
             "(C-contiguous, uint8, writable); the frame is written into it and `out` is returned, "
             "otherwise a new array is allocated. Raises RuntimeError on decoding failure and "
             "TypeError/ValueError for an unusable `out`.");

    py::class_<CUDAFrameHandler, FrameHandler>(m, "CUDAFrameHandler")
        .def(py::init<StreamSession *>(), py::arg("stream_session"), py::keep_alive<1, 2>())
        .def("get_frame", &CUDAFrameHandler::get_frame,
             py::arg("out") = py::none(),
             "Pull the next decoded video frame on the GPU, or None if none was available yet. Returns a "
             "CudaFrame whose NV12 planes torch and cupy can wrap without copying. If `out` is given - a "
             "writable, C-contiguous uint8 CUDA array such as a torch tensor or cupy array, shaped "
             "(height, width, 3) - the frame is instead converted to RGB on the GPU into it, `out` is "
             "returned, and the conversion has finished when this returns. Requires hardware_decoder='cuda' "
             "(RuntimeError otherwise); a bad `out` raises TypeError/ValueError.");

    py::class_<GPUFrameHandler, FrameHandler>(m, "GPUFrameHandler")
        .def(py::init<StreamSession *>(), py::arg("stream_session"), py::keep_alive<1, 2>())
        .def("get_frame", &GPUFrameHandler::get_frame,
             py::arg("out") = py::none(),
             "Pull the next decoded video frame without leaving GPU memory. Returns a GpuFrame, or None "
             "if none was available yet. If `out` is a GpuFrame it takes over the new frame (releasing "
             "the one it held) and is returned instead of a new GpuFrame being created. Raises "
             "RuntimeError if the session doesn't use a hardware decoder and TypeError if `out` isn't a GpuFrame.");

    py::class_<Settings>(m, "Settings")
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

    py::class_<StreamSessionConnectInfo>(m, "StreamSessionConnectInfo")
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

    py::class_<StreamSession>(m, "StreamSession")
        .def(py::init<const StreamSessionConnectInfo &>(), py::arg("connect_info"))
        .def("start", &StreamSession::Start, "Start the stream session.")
        .def("stop", &StreamSession::Stop, "Stop the stream session.")
        .def("go_to_bed", &StreamSession::GoToBed, "Go to bed.")
        .def("set_login_pin", &StreamSession::SetLoginPIN, py::arg("pin"), "Set the login PIN.")
        .def("go_home", &StreamSession::GoHome, "Go home.")
        .def("get_host", &StreamSession::GetHost, "Get the host.")
        .def("is_connected", &StreamSession::IsConnected, "Check if connected.")
        .def("is_connecting", &StreamSession::IsConnecting, "Check if connecting.")
        .def("get_measured_bitrate", &StreamSession::GetMeasuredBitrate, "Get the measured bitrate.")
        .def("get_average_packet_loss", &StreamSession::GetAveragePacketLoss, "Get the average packet loss.")
        .def("get_muted", &StreamSession::GetMuted, "Get the muted status.")
        .def("set_audio_volume", &StreamSession::SetAudioVolume, py::arg("volume"), "Set the audio volume.")
        .def("get_cant_display", &StreamSession::GetCantDisplay, "Get the cant display status.")
        .def("get_ffmpeg_decoder", &StreamSession::GetFfmpegDecoder, "Get the FFmpeg decoder.", py::return_value_policy::reference)
        .def("has_hardware_decoder", [](StreamSession &s) {
            ChiakiFfmpegDecoder *decoder = s.GetFfmpegDecoder();
            return decoder && decoder->hw_device_ctx;
        }, "Whether the video decoder is hardware-accelerated (i.e. get_frame_gpu() can return frames).")
        .def("get_video_profile", &StreamSession::GetVideoProfile,
             "The stream's video profile (width, height, max_fps, bitrate, codec): the requested one until the "
             "console answers, then the negotiated one. Known before the first frame arrives.")
        .def("hardware_decoder_type", [](StreamSession &s) -> std::string {
            ChiakiFfmpegDecoder *decoder = s.GetFfmpegDecoder();
            if (!decoder || !decoder->hw_device_ctx)
                return "";
            return av_hwdevice_get_type_name(reinterpret_cast<AVHWDeviceContext *>(decoder->hw_device_ctx->data)->type);
        }, "Type of hardware decoder in use ('vulkan', 'cuda', 'd3d11va', ...), or '' if decoding on the CPU.")
        .def("on_frame_available", &StreamSession::OnFfmpegFrameAvailable, "Retrieve the FFmpeg frame available event.", py::return_value_policy::reference)
        .def("on_session_quit", &StreamSession::OnSessionQuit, "Retrieve the session quit event.", py::return_value_policy::reference)
        .def("on_login_pin_requested", &StreamSession::OnLoginPINRequested, "Retrieve the login PIN requested event.", py::return_value_policy::reference)
        .def("on_data_holepunch_progress", &StreamSession::OnDataHolepunchProgress, "Retrieve the data holepunch progress event.", py::return_value_policy::reference)
        .def("on_auto_regist_succeeded", &StreamSession::OnAutoRegistSucceeded, "Retrieve the auto-registration succeeded event.", py::return_value_policy::reference)
        .def("on_nickname_received", &StreamSession::OnNicknameReceived, "Retrieve the nickname received event.", py::return_value_policy::reference)
        .def("on_connected_changed", &StreamSession::OnConnectedChanged, "Retrieve the connected changed event.", py::return_value_policy::reference)
        .def("on_measured_bitrate_changed", &StreamSession::OnMeasuredBitrateChanged, "Retrieve the measured bitrate changed event.", py::return_value_policy::reference)
        .def("on_average_packet_loss_changed", &StreamSession::OnAveragePacketLossChanged, "Retrieve the average packet loss changed event.", py::return_value_policy::reference)
        .def("on_cant_display_changed", &StreamSession::OnCantDisplayChanged, "Retrieve the cant display changed event.", py::return_value_policy::reference)
        .def("press_cross", &StreamSession::pressCross, "Press the cross button.")
        .def("release_cross", &StreamSession::releaseCross, "Release the cross button.")
        .def("press_circle", &StreamSession::pressCircle, "Press the circle button.")
        .def("release_circle", &StreamSession::releaseCircle, "Release the circle button.")
        .def("press_square", &StreamSession::pressSquare, "Press the square button.")
        .def("release_square", &StreamSession::releaseSquare, "Release the square button.")
        .def("press_triangle", &StreamSession::pressTriangle, "Press the triangle button.")
        .def("release_triangle", &StreamSession::releaseTriangle, "Release the triangle button.")
        .def("press_left", &StreamSession::pressLeft, "Press the left button.")
        .def("release_left", &StreamSession::releaseLeft, "Release the left button.")
        .def("press_right", &StreamSession::pressRight, "Press the right button.")
        .def("release_right", &StreamSession::releaseRight, "Release the right button.")
        .def("press_up", &StreamSession::pressUp, "Press the up button.")
        .def("release_up", &StreamSession::releaseUp, "Release the up button.")
        .def("press_down", &StreamSession::pressDown, "Press the down button.")
        .def("release_down", &StreamSession::releaseDown, "Release the down button.")
        .def("press_l1", &StreamSession::pressL1, "Press the L1 button.")
        .def("release_l1", &StreamSession::releaseL1, "Release the L1 button.")
        .def("press_r1", &StreamSession::pressR1, "Press the R1 button.")
        .def("release_r1", &StreamSession::releaseR1, "Release the R1 button.")
        .def("press_l3", &StreamSession::pressL3, "Press the L3 button.")
        .def("release_l3", &StreamSession::releaseL3, "Release the L3 button.")
        .def("press_r3", &StreamSession::pressR3, "Press the R3 button.")
        .def("release_r3", &StreamSession::releaseR3, "Release the R3 button.")
        .def("press_options", &StreamSession::pressOptions, "Press the options button.")
        .def("release_options", &StreamSession::releaseOptions, "Release the options button.")
        .def("press_create", &StreamSession::pressCreate, "Press the create button.")
        .def("release_create", &StreamSession::releaseCreate, "Release the create button.")
        .def("press_touchpad", &StreamSession::pressTouchpad, "Press the touchpad button.")
        .def("release_touchpad", &StreamSession::releaseTouchpad, "Release the touchpad button.")
        .def("press_ps", &StreamSession::pressPS, "Press the PS button.")
        .def("release_ps", &StreamSession::releasePS, "Release the PS button.")
        .def("set_l2", &StreamSession::setL2, py::arg("state"), "Set the L2 trigger state [0, 255].")
        .def("set_r2", &StreamSession::setR2, py::arg("state"), "Set the R2 trigger state [0, 255].")
        .def("set_left_x", &StreamSession::setLeftX, py::arg("x"), "Set the left analog's stick x value [0, 1023].")
        .def("set_left_y", &StreamSession::setLeftY, py::arg("y"), "Set the left analog's stick y value [0, 1023].")
        .def("set_left", &StreamSession::setLeft, py::arg("x"), py::arg("y"), "Set the left analog's stick x and y value [0, 1023].")
        .def("set_right_x", &StreamSession::setRightX, py::arg("x"), "Set the right analog's stick x value [0, 1023].")
        .def("set_right_y", &StreamSession::setRightY, py::arg("y"), "Set the right analog's stick y value [0, 1023].")
        .def("set_right", &StreamSession::setRight, py::arg("x"), py::arg("y"), "Set the right analog's stick x and y value [0, 1023].")

        .def("set_accelerometer_x", &StreamSession::setAccelerometerX, py::arg("x"), "Set the accelerometer x value [0, 1023].")
        .def("set_accelerometer_y", &StreamSession::setAccelerometerY, py::arg("y"), "Set the accelerometer y value [0, 1023].")
        .def("set_accelerometer_z", &StreamSession::setAccelerometerZ, py::arg("z"), "Set the accelerometer z value [0, 1023].")
        .def("set_accelerometer", &StreamSession::setAccelerometer, py::arg("x"), py::arg("y"), py::arg("z"), "Set the accelerometer x, y and z value [0, 1023].")

        .def("set_gyroscope_x", &StreamSession::setGyroscopeX, py::arg("x"), "Set the gyroscope x value [0, 1023].")
        .def("set_gyroscope_y", &StreamSession::setGyroscopeY, py::arg("y"), "Set the gyroscope y value [0, 1023].")
        .def("set_gyroscope_z", &StreamSession::setGyroscopeZ, py::arg("z"), "Set the gyroscope z value [0, 1023].")
        .def("set_gyroscope", &StreamSession::setGyroscope, py::arg("x"), py::arg("y"), py::arg("z"), "Set the gyroscope x, y and z value [0, 1023].")

        .def("set_orientation_x", &StreamSession::setOrientationX, py::arg("x"), "Set the orientation x value [0, 1023].")
        .def("set_orientation_y", &StreamSession::setOrientationY, py::arg("y"), "Set the orientation y value [0, 1023].")
        .def("set_orientation_z", &StreamSession::setOrientationZ, py::arg("z"), "Set the orientation z value [0, 1023].")
        .def("set_orientation_w", &StreamSession::setOrientationW, py::arg("w"), "Set the orientation w value [0, 1023].")
        .def("set_orientation", &StreamSession::setOrientation, py::arg("x"), py::arg("y"), py::arg("z"), py::arg("w"), "Set the orientation x, y, z and w value [0, 1023].")

        .def("send_feedback_state", &StreamSession::SendFeedbackState, "Send the feedback state.");

    py::enum_<ChiakiDiscoveryHostState>(m, "DiscoveryHostState")
        .value("Unknown", ChiakiDiscoveryHostState::CHIAKI_DISCOVERY_HOST_STATE_UNKNOWN)
        .value("Ready", ChiakiDiscoveryHostState::CHIAKI_DISCOVERY_HOST_STATE_READY)
        .value("Standby", ChiakiDiscoveryHostState::CHIAKI_DISCOVERY_HOST_STATE_STANDBY)
        .export_values();

    py::class_<HostMAC>(m, "HostMAC")
        .def("to_string", &HostMAC::ToString, "Get the MAC address as a hex string.")
        .def("__str__", &HostMAC::ToString);

    py::class_<DiscoveryHostWrapper>(m, "DiscoveryHost")
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

    py::class_<DiscoveryManager>(m, "DiscoveryManager")
        .def(py::init<>())
        .def("set_active", &DiscoveryManager::SetActive, py::arg("active"))
        .def("set_settings", &DiscoveryManager::SetSettings, py::arg("settings"))
        .def("send_wakeup", &DiscoveryManager::SendWakeup, py::arg("host"), py::arg("regist_key"), py::arg("ps5"))
        .def("get_active", &DiscoveryManager::GetActive)
        .def("discovery_service_hosts", &DiscoveryManager::GetHosts)
        .def("update_manual_services", &DiscoveryManager::DiscoveryServiceHosts, py::arg("hosts"))
        .def("hosts_updated", &DiscoveryManager::UpdateManualServices);
}