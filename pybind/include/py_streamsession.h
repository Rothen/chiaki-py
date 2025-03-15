// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef CHIAKI_PY_STREAMSESSION_H
#define CHIAKI_PY_STREAMSESSION_H

#include <string>
#include <map>
#include <vector>
#include <unordered_map>
#include <queue>
#include <tuple>
#include <thread>
#include <chrono>
#include <atomic>

#include "py_timer.h"
#include "py_exception.h"
#include "py_sessionlog.h"
#include "py_controllermanager.h"
#include "py_settings.h"
#include "py_elapsed_timer.h"

#include <chiaki/session.h>
#include <chiaki/opusdecoder.h>
#include <chiaki/opusencoder.h>
#include <chiaki/ffmpegdecoder.h>

class KeyEvent {
    public:
        int key() { return 0; }
        bool isAutoRepeat() { return true; }
        enum Type { KeyPress, KeyRelease };
        Type type() { return KeyPress; }
};

class ChiakiException: public Exception
{
	public:
		explicit ChiakiException(const std::string &msg) : Exception(msg) {};
};

struct StreamSessionConnectInfo
{
	Settings *settings;
	std::map<int, int> key_map;
	Decoder decoder;
	std::string hw_decoder;
	AVBufferRef *hw_device_ctx;
	std::string audio_out_device;
	std::string audio_in_device;
	uint32_t log_level_mask;
	std::string log_file;
	ChiakiTarget target;
	std::string host;
	std::string nickname;
    char regist_key[CHIAKI_SESSION_AUTH_SIZE]; // must be completely filled (pad with \0)
    uint8_t morning[0x10];
    std::string initial_login_pin;
	ChiakiConnectVideoProfile video_profile;
	double packet_loss_max;
	unsigned int audio_buffer_size;
	int audio_volume;
	bool fullscreen;
	bool zoom;
	bool stretch;
	bool enable_keyboard;
	bool enable_dualsense;
	bool auto_regist;
	float haptic_override;
	ChiakiDisableAudioVideo audio_video_disabled;
	RumbleHapticsIntensity rumble_haptics_intensity;
	bool buttons_by_pos;
	bool start_mic_unmuted;
	std::string duid;
	std::string psn_token;
	std::string psn_account_id;
	uint16_t dpad_touch_increment;
	unsigned int dpad_touch_shortcut1;
	unsigned int dpad_touch_shortcut2;
	unsigned int dpad_touch_shortcut3;
	unsigned int dpad_touch_shortcut4;

	StreamSessionConnectInfo() {}
    StreamSessionConnectInfo(
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
        bool stretch);
};

class StreamSession
{
	friend class StreamSessionPrivate;

	private:
        std::string host;
        bool connected = false;
        double measuredBitrate = 0.0;
        double averagePacketLoss = 0.0;
        bool muted = false;
        bool cantDisplay = false;

		SessionLog log;
		ChiakiSession session;
		ChiakiOpusDecoder opus_decoder;
		ChiakiOpusEncoder opus_encoder;
		bool mic_connected;
		bool allow_unmute;
		int input_block;
		int audio_volume;
		double measured_bitrate = 0;
		double average_packet_loss = 0;
		std::list<double> packet_loss_history;
		bool cant_display = false;
		int haptics_handheld;
		float rumble_multiplier;
		int ps5_rumble_intensity;
		int ps5_trigger_intensity;
		uint8_t led_color[3];
        std::unordered_map<int, Controller *> controllers;
        std::queue<uint16_t> rumble_haptics;
		bool rumble_haptics_connected;
		bool rumble_haptics_on;
		float PS_TOUCHPAD_MAX_X, PS_TOUCHPAD_MAX_Y;
		ChiakiControllerState keyboard_state;
		ChiakiControllerState touch_state;
		std::map<int, uint8_t> touch_tracker;
		int8_t mouse_touch_id;
		ChiakiControllerState dpad_touch_state;
		uint16_t dpad_touch_increment;
		float trigger_override;
		float haptic_override;
		bool dpad_regular;
		bool dpad_regular_touch_switched;
		unsigned int dpad_touch_shortcut1;
		unsigned int dpad_touch_shortcut2;
		unsigned int dpad_touch_shortcut3;
		unsigned int dpad_touch_shortcut4;
		int8_t dpad_touch_id;
		std::tuple<uint16_t, uint16_t> dpad_touch_value;
        std::atomic<bool> dpad_touch_running{true};
        std::atomic<bool> dpad_touch_stop_running{true};
        Timer double_tap_timer;
        RumbleHapticsIntensity rumble_haptics_intensity;
		bool start_mic_unmuted;
		bool session_started;

		ChiakiFfmpegDecoder *ffmpeg_decoder;
		void TriggerFfmpegFrameAvailable();
		std::string audio_out_device_name;
		std::string audio_in_device_name;
		size_t audio_out_sample_size;
		bool audio_out_drain_queue;
		size_t haptics_buffer_size;
		unsigned int audio_buffer_size;
		ChiakiHolepunchSession holepunch_session;
		uint8_t *haptics_resampler_buf;
		std::map<int, int> key_map;
        ElapsedTimer connect_timer;

		void CantDisplayMessage(bool cant_display);
		ChiakiErrorCode InitiatePsnConnection(std::string psn_token);

        std::function<void(ChiakiEvent *)> OnEvent;
        std::function<void()> OnUpdateGamepads;

        void Event(ChiakiEvent *event);
        void UpdateGamepads()
        {
            if (OnUpdateGamepads) OnUpdateGamepads();
        }

    public:
		explicit StreamSession(const StreamSessionConnectInfo &connect_info);
		~StreamSession();

		bool IsConnected()	{ return connected; }
		bool IsConnecting()	{ return connect_timer.isValid(); }

		void Start();
		void Stop();
		void GoToBed();
		void SetLoginPIN(const std::string &pin);
		void GoHome();
		std::string GetHost() { return host; }
		bool GetConnected() { return connected; }
		double GetMeasuredBitrate()	{ return measured_bitrate; }
		double GetAveragePacketLoss()	{ return average_packet_loss; }
		bool GetMuted()	{ return muted; }
		void SetAudioVolume(int volume) { audio_volume = volume; }
		bool GetCantDisplay()	{ return cant_display; }
		ChiakiErrorCode ConnectPsnConnection(std::string duid, bool ps5);
		void CancelPsnConnection(bool stop_thread);

		ChiakiLog *GetChiakiLog()				{ return log.GetChiakiLog(); }
		std::list<Controller *> GetControllers()
        {
            std::list<Controller *> controller_list{controllers.size()};
            for (auto &host : controllers)
            {
                controller_list.push_back(host.second);
            }
            return controller_list;
        }
        ChiakiFfmpegDecoder *GetFfmpegDecoder()	{ return ffmpeg_decoder; }

		std::function<void()> FfmpegFrameAvailable;
        std::function<void(ChiakiQuitReason, const std::string &)> SessionQuit;
		std::function<void(bool)> LoginPINRequested;
		std::function<void(bool)> DataHolepunchProgress;
		std::function<void(const ChiakiRegisteredHost &)> AutoRegistSucceeded;
		std::function<void(std::string)> NicknameReceived;
		std::function<void()> ConnectedChanged;
		std::function<void()> MeasuredBitrateChanged;
		std::function<void()> AveragePacketLossChanged;
        std::function<void(bool)> CantDisplayChanged;
};

#endif // CHIAKI_PY_STREAMSESSION_H
