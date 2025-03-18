// SPDX - License - Identifier : LicenseRef - AGPL - 3.0 - only - OpenSSL

#include "../../lib/src/utils.h"
#include "utils.h"
#include "streamsession.h"
#include "settings.h"
#include "controllermanager.h"

#include <ios>
#include <cstring>
#include <iostream>
#include <string>
#include <locale>
#include <codecvt>
#include <fmt/core.h>

#include <chiaki/base64.h>
#include <chiaki/streamconnection.h>
#include <chiaki/remote/holepunch.h>
#include <chiaki/session.h>
// #include <chiaki/chiaki_time.h>

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
    #define CP_ACP 0
    #define CP_UTF8 65001
#endif

#define SETSU_UPDATE_INTERVAL_MS 4
#define STEAMDECK_UPDATE_INTERVAL_MS 4
#define STEAMDECK_HAPTIC_INTERVAL_MS 10 // check every interval
#define NEW_DPAD_TOUCH_INTERVAL_MS 500
#define DPAD_TOUCH_UPDATE_INTERVAL_MS 10
#define STEAMDECK_HAPTIC_PACKETS_PER_ANALYSIS 4 // send packets every interval * packets per analysis
#define RUMBLE_HAPTICS_PACKETS_PER_RUMBLE 3
#define STEAMDECK_HAPTIC_SAMPLING_RATE 3000
// DualShock4 touchpad is 1920 x 942
#define PS4_TOUCHPAD_MAX_X 1920.0f
#define PS4_TOUCHPAD_MAX_Y 942.0f
// DualSense touchpad is 1919 x 1079
#define PS5_TOUCHPAD_MAX_X 1919.0f
#define PS5_TOUCHPAD_MAX_Y 1079.0f
#define SESSION_RETRY_SECONDS 20
#define HAPTIC_RUMBLE_MIN_STRENGTH 100

#define MICROPHONE_SAMPLES 480
#define DUALSENSE_AUDIO_DEVICE_NEEDLE "Wireless Controller"

    static bool isLocalAddress(std::string host)
{
    if (host.find(".") != std::string::npos)
    {
        if (host.rfind("10.") == 0)
            return true;
        else if (host.rfind("192.168.") == 0)
            return true;
        for (int j = 16; j < 32; j++)
        {
            if (host.rfind(std::string("172.") + fmt::format("{:x}", j) + std::string(".")))
                return true;
        }
    }
    else if (host.find(":") != std::string::npos)
    {
        if (host.rfind("FC"/*, Qt::CaseInsensitive*/) == 0)
            return true;
        if (host.rfind("FD"/*, Qt::CaseInsensitive*/) == 0)
            return true;
    }
    return false;
}

StreamSessionConnectInfo::StreamSessionConnectInfo(
    Settings *settings,
    ChiakiTarget target,
    std::string host,
    std::string nickname,
    std::string &regist_key,
    std::vector<uint8_t> &morning,
    std::string initial_login_pin,
    std::string duid,
    bool auto_regist,
    bool fullscreen,
    bool zoom,
    bool stretch)
    : settings(settings)
{
    key_map = settings->GetControllerMappingForDecoding();
    decoder = settings->GetDecoder();
    hw_decoder = settings->GetHardwareDecoder();
    hw_device_ctx = nullptr;
    audio_out_device = settings->GetAudioOutDevice();
    audio_in_device = settings->GetAudioInDevice();
    log_level_mask = settings->GetLogLevelMask();
    audio_volume = settings->GetAudioVolume();
    log_file = CreateLogFilename();
    // local connection
    if (duid.empty() && isLocalAddress(host))
        video_profile = chiaki_target_is_ps5(target) ? settings->GetVideoProfileLocalPS5() : settings->GetVideoProfileLocalPS4();
    // remote connection
    else
        video_profile = chiaki_target_is_ps5(target) ? settings->GetVideoProfileRemotePS5() : settings->GetVideoProfileRemotePS4();
    this->target = target;
    this->nickname = std::move(nickname);
    this->host = std::move(host);

    std::memset(this->regist_key, '\0', CHIAKI_SESSION_AUTH_SIZE); // Zero out first
    strncpy(this->regist_key, regist_key.c_str(), CHIAKI_SESSION_AUTH_SIZE - 1);

    std::memset(this->morning, 0, 0x10);
    std::memcpy(this->morning, morning.data(), morning.size());

    this->initial_login_pin = std::move(initial_login_pin);
    audio_buffer_size = settings->GetAudioBufferSize();
    this->fullscreen = fullscreen;
    this->zoom = zoom;
    this->stretch = stretch;
    this->enable_keyboard = false; // TODO: from settings
    this->enable_dualsense = true;
    this->rumble_haptics_intensity = settings->GetRumbleHapticsIntensity();
    this->buttons_by_pos = settings->GetButtonsByPosition();
    this->start_mic_unmuted = settings->GetStartMicUnmuted();
    this->packet_loss_max = settings->GetPacketLossMax();
    this->audio_video_disabled = settings->GetAudioVideoDisabled();
    this->haptic_override = settings->GetHapticOverride();
    this->psn_token = settings->GetPsnAuthToken();
    this->psn_account_id = settings->GetPsnAccountId();
    this->duid = std::move(duid);
    this->auto_regist = auto_regist;
    this->dpad_touch_increment = settings->GetDpadTouchEnabled() ? settings->GetDpadTouchIncrement() : 0;
    this->dpad_touch_shortcut1 = settings->GetDpadTouchShortcut1();
    if (this->dpad_touch_shortcut1 > 0)
        this->dpad_touch_shortcut1 = 1 << (this->dpad_touch_shortcut1 - 1);
    this->dpad_touch_shortcut2 = settings->GetDpadTouchShortcut2();
    if (this->dpad_touch_shortcut2 > 0)
        this->dpad_touch_shortcut2 = 1 << (this->dpad_touch_shortcut2 - 1);
    this->dpad_touch_shortcut3 = settings->GetDpadTouchShortcut3();
    if (this->dpad_touch_shortcut3 > 0)
        this->dpad_touch_shortcut3 = 1 << (this->dpad_touch_shortcut3 - 1);
    this->dpad_touch_shortcut4 = settings->GetDpadTouchShortcut4();
    if (this->dpad_touch_shortcut4 > 0)
        this->dpad_touch_shortcut4 = 1 << (this->dpad_touch_shortcut4 - 1);
}

static void AudioSettingsCb(uint32_t channels, uint32_t rate, void *user);
static void AudioFrameCb(int16_t *buf, size_t samples_count, void *user);
static void HapticsFrameCb(uint8_t *buf, size_t buf_size, void *user);
static void CantDisplayCb(void *user, bool cant_display);
static void EventCb(ChiakiEvent *event, void *user);
static void FfmpegFrameCb(ChiakiFfmpegDecoder *decoder, void *user);

static const std::string base64_chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

std::vector<uint8_t> fromBase64(const std::string &base64Str)
{
    std::vector<uint8_t> decoded;
    int val = 0, valb = -8;

    for (char c : base64Str)
    {
        if (c == '=')
            break;
        int pos = base64_chars.find(c);
        if (pos == std::string::npos)
            continue;

        val = (val << 6) + pos;
        valb += 6;
        if (valb >= 0)
        {
            decoded.push_back((val >> valb) & 0xFF);
            valb -= 8;
        }
    }
    return decoded;
}

StreamSession::StreamSession(const StreamSessionConnectInfo &connect_info)
    : log(this, connect_info.log_level_mask, connect_info.log_file),
      session_started(false),
      ffmpeg_decoder(nullptr),
      holepunch_session(nullptr),
      haptics_resampler_buf(nullptr)
      // haptics_handheld(0),
      // rumble_multiplier(1),
      // ps5_rumble_intensity(0x00),
      // ps5_trigger_intensity(0x00),
      // rumble_haptics_connected(false),
      // rumble_haptics_on(false)
{

#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
    connected = false;
    muted = true;
    mic_connected = false;
    allow_unmute = false;
    dpad_regular = true;
    dpad_regular_touch_switched = false;
    rumble_haptics_intensity = RumbleHapticsIntensity::Off;
    input_block = 0;
    memset(led_color, 0, sizeof(led_color));
    ChiakiErrorCode err;
    ffmpeg_decoder = new ChiakiFfmpegDecoder;
    ChiakiLogSniffer sniffer;
    chiaki_log_sniffer_init(&sniffer, CHIAKI_LOG_ALL, GetChiakiLog());
    err = chiaki_ffmpeg_decoder_init(ffmpeg_decoder,
                                        chiaki_log_sniffer_get_log(&sniffer),
                                        chiaki_target_is_ps5(connect_info.target) ? connect_info.video_profile.codec : CHIAKI_CODEC_H264,
                                        connect_info.hw_decoder.empty() ? NULL : connect_info.hw_decoder.c_str(),
                                        connect_info.hw_device_ctx, FfmpegFrameCb, this);
    if (err != CHIAKI_ERR_SUCCESS)
    {
        std::string log = std::string(chiaki_log_sniffer_get_buffer(&sniffer));
        chiaki_log_sniffer_fini(&sniffer);
        throw ChiakiException("Failed to initialize FFMPEG Decoder:\n" + log);
    }
    chiaki_log_sniffer_fini(&sniffer);
    ffmpeg_decoder->log = GetChiakiLog();
    audio_volume = connect_info.audio_volume;
    start_mic_unmuted = connect_info.start_mic_unmuted;
    audio_out_device_name = connect_info.audio_out_device;
    audio_in_device_name = connect_info.audio_in_device;

    chiaki_opus_decoder_init(&opus_decoder, log.GetChiakiLog());
    chiaki_opus_encoder_init(&opus_encoder, log.GetChiakiLog());
    audio_buffer_size = connect_info.audio_buffer_size;

    host = connect_info.host;

    ChiakiConnectInfo chiaki_connect_info = {};
    chiaki_connect_info.ps5 = chiaki_target_is_ps5(connect_info.target);
    chiaki_connect_info.host = connect_info.host.c_str();
    chiaki_connect_info.video_profile = connect_info.video_profile;
    chiaki_connect_info.video_profile_auto_downgrade = true;
    chiaki_connect_info.enable_keyboard = false;
    chiaki_connect_info.enable_dualsense = connect_info.enable_dualsense;
    chiaki_connect_info.packet_loss_max = connect_info.packet_loss_max;
    chiaki_connect_info.auto_regist = connect_info.auto_regist;
    chiaki_connect_info.audio_video_disabled = connect_info.audio_video_disabled;

    dpad_touch_shortcut1 = connect_info.dpad_touch_shortcut1;
    dpad_touch_shortcut2 = connect_info.dpad_touch_shortcut2;
    dpad_touch_shortcut3 = connect_info.dpad_touch_shortcut3;
    dpad_touch_shortcut4 = connect_info.dpad_touch_shortcut4;
    haptic_override = connect_info.haptic_override;
    if (connect_info.duid.empty())
    {
        // if (connect_info.regist_key.size() != sizeof(chiaki_connect_info.regist_key))
        //     throw ChiakiException("RegistKey invalid");
        memcpy(chiaki_connect_info.regist_key, connect_info.regist_key, sizeof(chiaki_connect_info.regist_key));

        // if (connect_info.morning.size() != sizeof(chiaki_connect_info.morning))
        //     throw ChiakiException("Morning invalid");
        memcpy(chiaki_connect_info.morning, connect_info.morning, sizeof(chiaki_connect_info.morning));
    }

    if (chiaki_connect_info.ps5)
    {
        PS_TOUCHPAD_MAX_X = PS5_TOUCHPAD_MAX_X;
        PS_TOUCHPAD_MAX_Y = PS5_TOUCHPAD_MAX_Y;
    }
    else
    {
        PS_TOUCHPAD_MAX_X = PS4_TOUCHPAD_MAX_X;
        PS_TOUCHPAD_MAX_Y = PS4_TOUCHPAD_MAX_Y;
    }

    chiaki_controller_state_set_idle(&keyboard_state);
    chiaki_controller_state_set_idle(&touch_state);
    touch_tracker = std::map<int, uint8_t>();
    mouse_touch_id = -1;
    dpad_touch_id = -1;
    chiaki_controller_state_set_idle(&dpad_touch_state);
    dpad_touch_value = std::tuple<uint16_t, uint16_t>(0, 0);
    dpad_touch_increment = connect_info.dpad_touch_increment;
    /*dpad_touch_timer = new QTimer(this);
    connect(dpad_touch_timer, &QTimer::timeout, this, &StreamSession::DpadSendFeedbackState);
    dpad_touch_timer->setInterval(DPAD_TOUCH_UPDATE_INTERVAL_MS);
    dpad_touch_stop_timer = new QTimer(this);
    dpad_touch_stop_timer->setSingleShot(true);
    connect(dpad_touch_stop_timer, &QTimer::timeout, this, [this]
            {
		if(dpad_touch_id >= 0)
		{
			dpad_touch_timer->stop();
			chiaki_controller_state_stop_touch(&dpad_touch_state, (uint8_t)dpad_touch_id);
			dpad_touch_id = -1;
			SendFeedbackState();
		} });*/
    // If duid isn't empty connect with psn
    chiaki_connect_info.holepunch_session = NULL;
    if (!connect_info.duid.empty())
    {
        err = InitiatePsnConnection(connect_info.psn_token);
        if (err != CHIAKI_ERR_SUCCESS)
            throw ChiakiException("Psn Connection Failed " + fromLocal8Bit(chiaki_error_string(err)));
        chiaki_connect_info.holepunch_session = holepunch_session;
        std::vector<uint8_t> psn_account_id = fromBase64(connect_info.psn_account_id);
        if (psn_account_id.size() != CHIAKI_PSN_ACCOUNT_ID_SIZE)
        {
            throw ChiakiException(std::string("Invalid Account-ID The PSN Account-ID must be exactly %1 bytes encoded as base64."));
        }
        memcpy(chiaki_connect_info.psn_account_id, psn_account_id.data(), CHIAKI_PSN_ACCOUNT_ID_SIZE);
    }
    err = chiaki_session_init(&session, &chiaki_connect_info, GetChiakiLog());
    if (err != CHIAKI_ERR_SUCCESS)
        throw ChiakiException("Chiaki Session Init failed: " + fromLocal8Bit(chiaki_error_string(err)));
    ChiakiCtrlDisplaySink display_sink;
    display_sink.user = this;
    display_sink.cantdisplay_cb = CantDisplayCb;
    chiaki_session_ctrl_set_display_sink(&session, &display_sink);
    chiaki_opus_decoder_set_cb(&opus_decoder, AudioSettingsCb, AudioFrameCb, this);
    ChiakiAudioSink audio_sink;
    chiaki_opus_decoder_get_sink(&opus_decoder, &audio_sink);
    chiaki_session_set_audio_sink(&session, &audio_sink);
    ChiakiAudioHeader audio_header;
    chiaki_audio_header_set(&audio_header, 2, 16, MICROPHONE_SAMPLES * 100, MICROPHONE_SAMPLES);
    chiaki_opus_encoder_header(&audio_header, &opus_encoder, &session);

    if (connect_info.enable_dualsense)
    {
        ChiakiAudioSink haptics_sink;
        haptics_sink.user = this;
        haptics_sink.frame_cb = HapticsFrameCb;
        chiaki_session_set_haptics_sink(&session, &haptics_sink);
    }
    chiaki_session_set_video_sample_cb(&session, chiaki_ffmpeg_decoder_video_sample_cb, ffmpeg_decoder);

    chiaki_session_set_event_cb(&session, EventCb, this);
    key_map = connect_info.key_map;
    if (connect_info.enable_dualsense)
    {
        rumble_haptics_intensity = connect_info.rumble_haptics_intensity;
    }

    Timer *packet_loss_timer = new Timer();
    packet_loss_timer->setInterval(200);
    packet_loss_timer->start([this]() {
        if (packet_loss_history.size() > 10)
            packet_loss_history.erase(packet_loss_history.begin());

        // Simulated packet loss value (replace with real data)
        double new_packet_loss = 0.05;
        packet_loss_history.push_back(new_packet_loss);

        double packet_loss = 0;
        for (double v : packet_loss_history)
            packet_loss += v / packet_loss_history.size();

        if (packet_loss != average_packet_loss) {
            average_packet_loss = packet_loss;
            std::cout << "Average Packet Loss Changed: " << average_packet_loss << std::endl;
            AveragePacketLossChanged();
        }
    });
}

StreamSession::~StreamSession()
{
    /*if (audio_out)
        SDL_CloseAudioDevice(audio_out);
    if (audio_in)
        SDL_CloseAudioDevice(audio_in);*/
    if (session_started)
        chiaki_session_join(&session);
    chiaki_session_fini(&session);
    chiaki_opus_decoder_fini(&opus_decoder);
    chiaki_opus_encoder_fini(&opus_encoder);
    if (ffmpeg_decoder)
    {
        chiaki_ffmpeg_decoder_fini(ffmpeg_decoder);
        delete ffmpeg_decoder;
    }
    /*if (dpad_touch_stop_timer)
    {
        delete dpad_touch_stop_timer;
        dpad_touch_stop_timer = nullptr;
    }
    if (dpad_touch_timer)
    {
        delete dpad_touch_timer;
        dpad_touch_timer = nullptr;
    }
    if (haptics_output > 0)
    {
        SDL_CloseAudioDevice(haptics_output);
        haptics_output = 0;
    }*/
    if (haptics_resampler_buf)
    {
        free(haptics_resampler_buf);
        haptics_resampler_buf = nullptr;
    }
}

void StreamSession::Start()
{
    if (!connect_timer.isValid())
        connect_timer.start();
    ChiakiErrorCode err = chiaki_session_start(&session);
    if (err != CHIAKI_ERR_SUCCESS)
    {
        session_started = true;
        throw ChiakiException("Chiaki Session Start failed");
    }
}

void StreamSession::Stop()
{
    chiaki_session_stop(&session);
}

void StreamSession::GoToBed()
{
    chiaki_session_goto_bed(&session);
}

void StreamSession::SetLoginPIN(const std::string &pin)
{
    std::vector<uint8_t> data(pin.begin(), pin.end());
    chiaki_session_set_login_pin(&session, (const uint8_t *)data.data(), data.size());
}

void StreamSession::GoHome()
{
    chiaki_session_go_home(&session);
}

void StreamSession::Event(ChiakiEvent *event)
{
    switch (event->type)
    {
    case CHIAKI_EVENT_CONNECTED:
        connect_timer.invalidate();
        connected = true;
        ConnectedChanged();
        break;
    case CHIAKI_EVENT_QUIT:
        if (!connected && !holepunch_session && chiaki_quit_reason_is_error(event->quit.reason) && connect_timer.elapsed() < SESSION_RETRY_SECONDS * 1000)
        {
            Timer::singleShot(1000, [this]() { this->Start(); });
            return;
        }
        connected = false;
        ConnectedChanged();
        SessionQuit(event->quit.reason, event->quit.reason_str ? std::string(event->quit.reason_str) : std::string());
        break;
    case CHIAKI_EVENT_REGIST:
        AutoRegistSucceeded(event->host);
        break;
    case CHIAKI_EVENT_LOGIN_PIN_REQUEST:
        LoginPINRequested(event->login_pin_request.pin_incorrect);
        break;
    case CHIAKI_EVENT_HOLEPUNCH:
        DataHolepunchProgress(event->data_holepunch.finished);
        break;
    case CHIAKI_EVENT_NICKNAME_RECEIVED:
        NicknameReceived(event->server_nickname);
        break;
    case CHIAKI_EVENT_RUMBLE:
    {
        
        break;
    }
    case CHIAKI_EVENT_LED_COLOR:
    {
        
        break;
    }
    case CHIAKI_EVENT_MOTION_RESET:
    {
        
        break;
    }
    case CHIAKI_EVENT_HAPTIC_INTENSITY:
    {
        
        break;
    }
    case CHIAKI_EVENT_TRIGGER_INTENSITY:
    {
        
        break;
    }
    case CHIAKI_EVENT_TRIGGER_EFFECTS:
    {
        
        break;
    }
    default:
        break;
    }
}

void StreamSession::CantDisplayMessage(bool cant_display)
{
    this->cant_display = cant_display;
    CantDisplayChanged(cant_display);
}

ChiakiErrorCode StreamSession::InitiatePsnConnection(std::string psn_token)
{
    ChiakiLog *log = GetChiakiLog();
    holepunch_session = chiaki_holepunch_session_init(psn_token.data(), log);
    if (!holepunch_session)
    {
        CHIAKI_LOGE(log, "!! Failed to initialize session");
        return CHIAKI_ERR_MEMORY;
    }
    return CHIAKI_ERR_SUCCESS;
}

ChiakiErrorCode StreamSession::ConnectPsnConnection(std::string duid, bool ps5)
{
    ChiakiLog *log = GetChiakiLog();
    if (ps5)
        CHIAKI_LOGI(log, "Duid: %s", duid.data());
    size_t duid_len = duid.size();
    const size_t duid_bytes_len{duid_len / 2};
    size_t duid_bytes_lenr = duid_bytes_len;
    std::vector<uint8_t> duid_bytes(duid_bytes_len);
    memset(duid_bytes.data(), 0, duid_bytes_len);
    parse_hex(duid_bytes.data(), &duid_bytes_lenr, duid.data(), duid_len);
    if (duid_bytes_len != duid_bytes_lenr)
    {
        CHIAKI_LOGE(log, "Couldn't convert duid string to bytes got size mismatch");
        return CHIAKI_ERR_INVALID_DATA;
    }
    ChiakiHolepunchConsoleType console_type = ps5 ? CHIAKI_HOLEPUNCH_CONSOLE_TYPE_PS5 : CHIAKI_HOLEPUNCH_CONSOLE_TYPE_PS4;
    ChiakiErrorCode err = chiaki_holepunch_upnp_discover(holepunch_session);
    if (err != CHIAKI_ERR_SUCCESS)
    {
        CHIAKI_LOGE(log, "!! Failed to run upnp discover");
        return err;
    }
    err = chiaki_holepunch_session_create(holepunch_session);
    if (err != CHIAKI_ERR_SUCCESS)
    {
        CHIAKI_LOGE(log, "!! Failed to create session");
        return err;
    }
    CHIAKI_LOGI(log, ">> Created session");
    err = holepunch_session_create_offer(holepunch_session);
    if (err != CHIAKI_ERR_SUCCESS)
    {
        CHIAKI_LOGE(log, "!! Failed to create offer msg for ctrl connection");
        return err;
    }
    CHIAKI_LOGI(log, ">> Created offer msg for ctrl connection");
    err = chiaki_holepunch_session_start(holepunch_session, duid_bytes.data(), console_type);
    if (err != CHIAKI_ERR_SUCCESS)
    {
        CHIAKI_LOGE(log, "!! Failed to start session");
        return err;
    }
    CHIAKI_LOGI(log, ">> Started session");

    err = chiaki_holepunch_session_punch_hole(holepunch_session, CHIAKI_HOLEPUNCH_PORT_TYPE_CTRL);
    if (err != CHIAKI_ERR_SUCCESS)
    {
        CHIAKI_LOGE(log, "!! Failed to punch hole for control connection.");
        return err;
    }
    CHIAKI_LOGI(log, ">> Punched hole for control connection!");
    return err;
}

void StreamSession::CancelPsnConnection(bool stop_thread)
{
    chiaki_holepunch_main_thread_cancel(holepunch_session, stop_thread);
}

void StreamSession::TriggerFfmpegFrameAvailable()
{
    FfmpegFrameAvailable();
    if (measured_bitrate != session.stream_connection.measured_bitrate)
    {
        measured_bitrate = session.stream_connection.measured_bitrate;
        MeasuredBitrateChanged();
    }
}

class StreamSessionPrivate
{
public:
    static void InitAudio(StreamSession *session, uint32_t channels, uint32_t rate)
    {
        // QMetaObject::invokeMethod(session, "InitAudio", Qt::ConnectionType::BlockingQueuedConnection, Q_ARG(unsigned int, channels), Q_ARG(unsigned int, rate));
    }

    static void InitMic(StreamSession *session, uint32_t channels, uint32_t rate)
    {
        // QMetaObject::invokeMethod(session, "InitMic", Qt::ConnectionType::QueuedConnection, Q_ARG(unsigned int, channels), Q_ARG(unsigned int, rate));
    }

    static void PushAudioFrame(StreamSession *session, int16_t *buf, size_t samples_count) { /*session->PushAudioFrame(buf, samples_count);*/ }
    static void PushHapticsFrame(StreamSession *session, uint8_t *buf, size_t buf_size) { /*session->PushHapticsFrame(buf, buf_size);*/ }
    static void CantDisplayMessage(StreamSession *session, bool cant_display) { session->CantDisplayMessage(cant_display); }
    static void Event(StreamSession *session, ChiakiEvent *event) { session->Event(event); }
    static void TriggerFfmpegFrameAvailable(StreamSession *session) { session->TriggerFfmpegFrameAvailable(); }
};

static void AudioSettingsCb(uint32_t channels, uint32_t rate, void *user)
{
    auto session = reinterpret_cast<StreamSession *>(user);
    StreamSessionPrivate::InitAudio(session, channels, rate);
}

static void AudioFrameCb(int16_t *buf, size_t samples_count, void *user)
{
    auto session = reinterpret_cast<StreamSession *>(user);
    StreamSessionPrivate::PushAudioFrame(session, buf, samples_count);
}

static void HapticsFrameCb(uint8_t *buf, size_t buf_size, void *user)
{
    auto session = reinterpret_cast<StreamSession *>(user);
    StreamSessionPrivate::PushHapticsFrame(session, buf, buf_size);
}

static void CantDisplayCb(void *user, bool cant_display)
{
    auto session = reinterpret_cast<StreamSession *>(user);
    StreamSessionPrivate::CantDisplayMessage(session, cant_display);
}

static void EventCb(ChiakiEvent *event, void *user)
{
    auto session = reinterpret_cast<StreamSession *>(user);
    StreamSessionPrivate::Event(session, event);
}

static void FfmpegFrameCb(ChiakiFfmpegDecoder *decoder, void *user)
{
    auto session = reinterpret_cast<StreamSession *>(user);
    StreamSessionPrivate::TriggerFfmpegFrameAvailable(session);
}