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

// Pulls the next decoded video frame (if any) into `target`, a caller-owned
// NumPy array. Returns (height, width) of the frame actually written if one
// was available, or None if no new frame was available yet (not an error).
// Raises RuntimeError on genuine failure. The (height, width) result lets
// callers size/slice their buffer correctly without having to separately
// track the negotiated stream resolution.
py::object get_frame(StreamSession &session, bool disable_zero_copy, py::array_t<uint8_t> target)
{
    ChiakiFfmpegDecoder *decoder = session.GetFfmpegDecoder();
    if (!decoder)
        throw std::runtime_error("Session has no FFmpeg decoder");

    int32_t frames_lost;
    AVFrame *frame = chiaki_ffmpeg_decoder_pull_frame(decoder, &frames_lost).frame;
    if (!frame)
        return py::none();

    // Owns whatever `frame` currently points to. Holding it by reference
    // means reassigning `frame` (hardware transfer, below) keeps the guard
    // in sync automatically instead of requiring a manual update at every
    // reassignment site.
    struct AVFrameGuard
    {
        AVFrame *&frame;
        ~AVFrameGuard() { if (frame) av_frame_free(&frame); }
    } frame_guard{frame};

    // Unlike chiaki-ng's Qt GUI (which can render Vulkan/D3D11/VAAPI frames
    // straight from the GPU and only transfers to CPU as a fallback), this
    // function always has to hand back CPU-readable bytes for NumPy, so any
    // hardware-resident frame must be transferred - there's no format for
    // which skipping the transfer would still leave us with readable data.
    // `disable_zero_copy` is accepted for API compatibility but currently
    // has no effect, since that "zero copy" GPU-rendering path doesn't
    // exist here.
    (void)disable_zero_copy;
    if (frame->hw_frames_ctx)
    {
        AVFrame *sw_frame = av_frame_alloc();
        if (!sw_frame)
            throw std::runtime_error("Failed to allocate software frame");

        if (av_hwframe_transfer_data(sw_frame, frame, 0) < 0)
        {
            av_frame_free(&sw_frame);
            throw std::runtime_error("Failed to transfer frame from hardware");
        }

        av_frame_copy_props(sw_frame, frame);
        av_frame_free(&frame); // frame_guard.frame is now null, nothing double-freed
        frame = sw_frame;      // frame_guard now owns sw_frame
    }

    // Holds the RGB conversion output, when needed. av_image_alloc uses a
    // raw malloc'd buffer rather than the refcounted AVBufferRef pool, so it
    // needs its own explicit free rather than av_frame_free/av_frame_unref.
    AVFrame *rgb_frame = nullptr;
    struct RgbFrameGuard
    {
        AVFrame *&frame;
        ~RgbFrameGuard()
        {
            if (frame)
            {
                av_freep(&frame->data[0]);
                av_frame_free(&frame);
            }
        }
    } rgb_frame_guard{rgb_frame};

    AVFrame *output = frame;

    if (frame->format == AV_PIX_FMT_NV12)
    {
        struct SwsContext *sws_ctx = sws_getContext(
            frame->width, frame->height, (AVPixelFormat)frame->format,
            frame->width, frame->height, AV_PIX_FMT_RGB24,
            SWS_BILINEAR, nullptr, nullptr, nullptr);
        if (!sws_ctx)
            throw std::runtime_error("Failed to create SwsContext");

        struct SwsContextGuard
        {
            SwsContext *ctx;
            ~SwsContextGuard() { sws_freeContext(ctx); }
        } sws_guard{sws_ctx};

        rgb_frame = av_frame_alloc();
        if (!rgb_frame)
            throw std::runtime_error("Failed to allocate RGB frame");

        rgb_frame->format = AV_PIX_FMT_RGB24;
        rgb_frame->width = frame->width;
        rgb_frame->height = frame->height;

        if (av_image_alloc(rgb_frame->data, rgb_frame->linesize, rgb_frame->width,
                        rgb_frame->height, AV_PIX_FMT_RGB24, 1) < 0)
        {
            av_frame_free(&rgb_frame);
            throw std::runtime_error("Failed to allocate RGB image buffer");
        }

        sws_scale(
            sws_ctx,
            frame->data, frame->linesize, 0, frame->height,
            rgb_frame->data, rgb_frame->linesize);

        output = rgb_frame;
    }

    if (output->format != AV_PIX_FMT_RGB24 && output->format != AV_PIX_FMT_GRAY8 && output->format != AV_PIX_FMT_YUV420P)
    {
        const char *name = av_get_pix_fmt_name((AVPixelFormat)output->format);
        throw std::runtime_error("Unsupported pixel format for NumPy conversion: " + std::string(name ? name : "unknown"));
    }

    int height = output->height;
    int width = output->width;
    int data_size = av_image_get_buffer_size((AVPixelFormat)output->format, width, height, 1);
    if (data_size <= 0)
        throw std::runtime_error("Failed to get image buffer size");

    py::buffer_info array_buf = target.request();
    if (array_buf.size < data_size)
        throw std::runtime_error("Target buffer is too small for frame data");

    av_image_copy_to_buffer(static_cast<uint8_t *>(array_buf.ptr), data_size, output->data, output->linesize, (AVPixelFormat)output->format, width, height, 1);

    return py::make_tuple(height, width);
}

PYBIND11_MODULE(chiaki_py, m)
{
    m.doc() = "Python bindings for Chiaki CLI commands";

#ifdef _WIN32
    // Winsock must be initialized process-wide before any class in this
    // module opens a socket (DiscoveryManager, Backend, StreamSession, ...).
    // This used to happen only inside StreamSession's constructor, so any
    // other class used without first constructing a StreamSession would
    // fail with "failed to create socket".
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
    // Log must be registered before Bitstream: Bitstream's constructor and
    // `log` property are typed in terms of it, and pybind11 bakes the
    // registered Python type name into a def()'d function's signature at
    // bind time - if the type isn't registered yet, it falls back to the
    // raw (and here, wrong - "LogWrapper" vs. the registered name "Log")
    // C++ type name, which is what stub generators like pybind11-stubgen
    // then pick up as the type hint. Runtime conversion is unaffected
    // (that resolves the type registry dynamically per call, well after
    // the whole module has finished importing), so this is a typehint-only
    // fix, not a crash fix.
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

    // These three were used as Settings getter/setter types without ever
    // being registered, which doesn't fail to compile (pybind11 only
    // resolves the caster at call time) but crashes every call at runtime
    // with "Unable to convert function return value to a Python type!".
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

    // Opaque handle - StreamSession::GetFfmpegDecoder() returns one only so
    // it can be threaded back into get_frame() internally; there's nothing
    // useful to call on it from Python. Registering it with no methods is
    // enough to make it a real bindable type instead of crashing on return,
    // same reasoning as the three enums above.
    py::class_<ChiakiFfmpegDecoder>(m, "FfmpegDecoder");

    // Settings::GetVideoProfile*() return this plain struct without it ever
    // being registered, same crash-on-return bug as the three enums above.
    py::class_<ChiakiConnectVideoProfile>(m, "ChiakiConnectVideoProfile")
        .def_readwrite("width", &ChiakiConnectVideoProfile::width)
        .def_readwrite("height", &ChiakiConnectVideoProfile::height)
        .def_readwrite("max_fps", &ChiakiConnectVideoProfile::max_fps)
        .def_readwrite("bitrate", &ChiakiConnectVideoProfile::bitrate)
        .def_readwrite("codec", &ChiakiConnectVideoProfile::codec);

    m.def("get_frame", &get_frame,
          py::arg("session"),
          py::arg("disable_zero_copy"),
          py::arg("target"),
          "Pull the next decoded video frame into `target`. Returns (height, width) of "
          "the frame written, or None if none was available yet. Raises RuntimeError on failure.");

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

    // get_host_mac() below returns a HostMAC without it ever being
    // registered, same "Unable to convert function return value to a
    // Python type!" crash as the enums above.
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