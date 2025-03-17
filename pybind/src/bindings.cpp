// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
#include <time.h>
#include "streamsession.h"
#include "settings.h"
#include "av_frame.h"
#include "core/common.h"
#include "core/audio.h"
#include "core/ecdh.h"
#include "core/fec.h"
#include "core/log.h"
// #include "core/session.h"
// #include "core/takion.h"
// #include "core/remote/holepunch.h"

#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <set>
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
#include <libavutil/imgutils.h>
#include <libavutil/hwcontext.h>
#include <libswscale/swscale.h>
}

namespace py = pybind11;

// Function to return a NumPy array
py::object get_frame(StreamSession &session, bool disable_zero_copy, py::array_t<uint8_t> target)
{
    // Retrieve the FFmpeg decoder
    ChiakiFfmpegDecoder *decoder = session.GetFfmpegDecoder();

    if (!decoder)
    {
        return py::str("Session has no FFmpeg decoder");
    }

    int32_t frames_lost;
    AVFrame *frame = chiaki_ffmpeg_decoder_pull_frame(decoder, &frames_lost);
    if (!frame)
    {
        return py::str("Failed to pull frame from FFmpeg decoder");
    }

    // Ensure proper cleanup if an error occurs
    struct AVFrameGuard
    {
        AVFrame *frame;
        ~AVFrameGuard() { av_frame_unref(frame); }
    } frame_guard{frame};

    // Handle hardware decoding cases
    static const std::set<int> zero_copy_formats = {AV_PIX_FMT_VULKAN,
                                                    AV_PIX_FMT_D3D11
#ifdef __linux__
                                                    ,
                                                    AV_PIX_FMT_VAAPI
#endif
    };

    if ((zero_copy_formats.find(frame->format) != zero_copy_formats.end() || disable_zero_copy))
    {
        AVFrame *sw_frame = av_frame_alloc();
        if (!sw_frame)
        {
            return py::str("Failed to allocate software frame");
        }

        if (av_hwframe_transfer_data(sw_frame, frame, 0) < 0)
        {
            av_frame_free(&sw_frame);
            return py::str("Failed to transfer frame from hardware (D3D11)");
        }

        av_frame_copy_props(sw_frame, frame);
        av_frame_unref(frame);
        frame = sw_frame;
        frame_guard.frame = frame; // Ensure cleanup
    }

    if (frame->format == AV_PIX_FMT_NV12) {
        // Step 1: Initialize SwsContext
        struct SwsContext *sws_ctx = sws_getContext(
            frame->width, frame->height, (AVPixelFormat)frame->format,
            frame->width, frame->height, AV_PIX_FMT_RGB24,
            SWS_BILINEAR, nullptr, nullptr, nullptr);

        if (!sws_ctx)
        {
            av_frame_free(&frame);
            return py::str("Failed to create SwsContext");
        }

        // Step 2: Allocate Target Frame
        AVFrame *rgb_frame = av_frame_alloc();
        if (!rgb_frame)
        {
            sws_freeContext(sws_ctx);
            return py::str("Failed to allocate RGB frame");
        }

        rgb_frame->format = AV_PIX_FMT_RGB24;
        rgb_frame->width = frame->width;
        rgb_frame->height = frame->height;

        if (av_image_alloc(rgb_frame->data, rgb_frame->linesize, rgb_frame->width,
                        rgb_frame->height, AV_PIX_FMT_RGB24, 1) < 0)
        {
            av_frame_free(&rgb_frame);
            sws_freeContext(sws_ctx);
            return py::str("Failed to allocate RGB image buffer");
        }

        // Step 3: Perform the Conversion
        sws_scale(
            sws_ctx,
            frame->data, frame->linesize, 0, frame->height,
            rgb_frame->data, rgb_frame->linesize);

        // Clean up the old frame and replace it with the new one
        av_frame_free(&frame);
        sws_freeContext(sws_ctx);
        frame = rgb_frame;
    }

    // Ensure the frame is in a readable format
    if (frame->format != AV_PIX_FMT_RGB24 && frame->format != AV_PIX_FMT_GRAY8 && frame->format != AV_PIX_FMT_YUV420P) // AV_PIX_FMT_D3D11
    {
        return py::str("Unsupported pixel format for NumPy conversion");
    }

    int height = frame->height;
    int width = frame->width;
    // int channels = (frame->format == AV_PIX_FMT_RGB24 || frame->format == AV_PIX_FMT_YUV420P) ? 3 : 1;
    int data_size = av_image_get_buffer_size((AVPixelFormat)frame->format, width, height, 1);

    if (data_size <= 0)
    {
        return py::str("Failed to get image buffer size");
    }

    // Copy data into a buffer
    std::vector<uint8_t> buffer(data_size);
    
    // Request buffer info from the NumPy array
    py::buffer_info array_buf = target.request();

    av_image_copy_to_buffer(static_cast<uint8_t *>(array_buf.ptr), data_size, frame->data, frame->linesize, (AVPixelFormat)frame->format, width, height, 1);

    return py::str("Success");
}

PYBIND11_MODULE(chiaki_py, m)
{
    m.doc() = "Python bindings for Chiaki CLI commands";

    auto m_core = m.def_submodule("core", "The core submodule.");
    // auto m_core_takion = m_core.def_submodule("takion", "The takion submodule.");
    auto m_core_common = m_core.def_submodule("common", "The common submodule.");
    auto m_core_audio = m_core.def_submodule("audio", "The audio submodule.");
    auto m_core_ecdh = m_core.def_submodule("ecdh", "The ecdh submodule.");
    auto m_core_fec = m_core.def_submodule("fec", "The fec submodule.");
    auto m_core_log = m_core.def_submodule("log", "The log submodule.");
    // auto m_core_session = m_core.def_submodule("session", "The session submodule.");

    // auto m_remote = m.def_submodule("remote", "The remote submodule.");
    // auto m_remote_holepunch = m.def_submodule("holepunch", "The holepunch submodule.");

    // init_core_takion(m_core_takion);
    init_core_common(m_core_common);
    init_core_audio(m_core_audio);
    init_core_ecdh(m_core_ecdh);
    init_core_fec(m_core_fec);
    init_core_log(m_core_log);
    // init_core_session(m_core_session);
    // init_core_remote_holepunch(m_remote_holepunch);

    py::class_<PyAVFrame>(m, "AVFrame")
        .def(py::init<>())
        .def("width", &PyAVFrame::width)
        .def("set_width", &PyAVFrame::set_width)
        .def("height", &PyAVFrame::height)
        .def("set_height", &PyAVFrame::set_height)
        .def("format", &PyAVFrame::format)
        .def("set_format", &PyAVFrame::set_format)
        .def("pts", &PyAVFrame::pts)
        .def("set_pts", &PyAVFrame::set_pts)
        .def("data", &PyAVFrame::data)
        .def("to_numpy", &PyAVFrame::to_numpy, "Convert frame data to numpy array");

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

    m.def("get_frame", &get_frame,
          py::arg("session"),
          py::arg("disable_zero_copy"),
          py::arg("target"),
          "Get the next frame from the session.");

    py::class_<Settings>(m, "Settings")
        .def(py::init<>())
        .def("get_audio_video_disabled", &Settings::GetAudioVideoDisabled, "Get the audio/video disabled.")
        .def("get_log_verbose", &Settings::GetLogVerbose, "Get the log verbose.")
        .def("set_log_verbose", &Settings::SetLogVerbose, py::arg("log_verbose") , "Set the log verbose.")
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
        .def(py::init<Settings *, ChiakiTarget, std::string, std::string, std::string &,
                      std::vector<uint8_t> &, std::string, std::string, bool, bool, bool, bool>(),
             py::arg("settings"),
             py::arg("target"),
             py::arg("host"),
             py::arg("nickname"),
             py::arg("regist_key"),
             py::arg("morning"),
             py::arg("initial_login_pin"),
             py::arg("duid"),
             py::arg("auto_regist"),
             py::arg("fullscreen"),
             py::arg("zoom"),
             py::arg("stretch"));

    py::class_<StreamSession>(m, "StreamSession")
        .def(py::init<const StreamSessionConnectInfo &>(), py::arg("connect_info"))
        .def("start", &StreamSession::Start)
        .def("stop", &StreamSession::Stop)
        .def("go_to_bed", &StreamSession::GoToBed)
        .def("set_login_pin", &StreamSession::SetLoginPIN)
        .def("go_home", &StreamSession::GoHome)
        .def("get_host", &StreamSession::GetHost)
        .def("is_connected", &StreamSession::IsConnected)
        .def("is_connecting", &StreamSession::IsConnecting)
        .def("get_measured_bitrate", &StreamSession::GetMeasuredBitrate)
        .def("get_average_packet_loss", &StreamSession::GetAveragePacketLoss)
        .def("get_muted", &StreamSession::GetMuted)
        .def("set_audio_volume", &StreamSession::SetAudioVolume)
        .def("get_cant_display", &StreamSession::GetCantDisplay)
        .def("get_ffmpeg_decoder", &StreamSession::GetFfmpegDecoder)
        .def_readwrite("ffmpeg_frame_available", &StreamSession::FfmpegFrameAvailable)
        .def_readwrite("session_quit", &StreamSession::SessionQuit)
        .def_readwrite("login_pin_requested", &StreamSession::LoginPINRequested)
        .def_readwrite("data_holepunch_progress", &StreamSession::DataHolepunchProgress)
        .def_readwrite("auto_regist_succeeded", &StreamSession::AutoRegistSucceeded)
        .def_readwrite("nickname_received", &StreamSession::NicknameReceived)
        .def_readwrite("connected_changed", &StreamSession::ConnectedChanged)
        .def_readwrite("measured_bitrate_changed", &StreamSession::MeasuredBitrateChanged)
        .def_readwrite("average_packet_loss_changed", &StreamSession::AveragePacketLossChanged)
        .def_readwrite("cant_display_changed", &StreamSession::CantDisplayChanged);
}