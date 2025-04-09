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

#include "timer.h"
#include "exception.h"
#include "sessionlog.h"
#include "controllermanager.h"
#include "settings.h"
#include "elapsed_timer.h"
#include "event_source.h"

#include <chiaki/session.h>
#include <chiaki/opusdecoder.h>
#include <chiaki/opusencoder.h>
#include <chiaki/ffmpegdecoder.h>

#include <pybind11/pybind11.h>

namespace py = pybind11;

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
        py::bytes morning,
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
        // double measuredBitrate = 0.0;
        // double averagePacketLoss = 0.0;
        bool muted = false;
        // bool cantDisplay = false;

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
		// int haptics_handheld;
		// float rumble_multiplier;
		// int ps5_rumble_intensity;
		// int ps5_trigger_intensity;
		uint8_t led_color[3];
        std::unordered_map<int, Controller *> controllers;
        std::queue<uint16_t> rumble_haptics;
		// bool rumble_haptics_connected;
		// bool rumble_haptics_on;
		float PS_TOUCHPAD_MAX_X, PS_TOUCHPAD_MAX_Y;
		ChiakiControllerState keyboard_state;
		ChiakiControllerState touch_state;
		std::map<int, uint8_t> touch_tracker;
		int8_t mouse_touch_id;
		ChiakiControllerState dpad_touch_state;
		uint16_t dpad_touch_increment;
		// float trigger_override;
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
		// size_t audio_out_sample_size;
		// bool audio_out_drain_queue;
		// size_t haptics_buffer_size;
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

        EventSource<bool> FfmpegFrameAvailable;
        EventSource<ChiakiQuitReason> SessionQuit;
        EventSource<bool> LoginPINRequested;
        EventSource<bool> DataHolepunchProgress;
        EventSource<const ChiakiRegisteredHost &> AutoRegistSucceeded;
        EventSource<std::string> NicknameReceived;
        EventSource<bool> ConnectedChanged;
        EventSource<double> MeasuredBitrateChanged;
        EventSource<double> AveragePacketLossChanged;
        EventSource<bool> CantDisplayChanged;

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

        const EventSource<bool> &OnFfmpegFrameAvailable() { return FfmpegFrameAvailable; }
        const EventSource<ChiakiQuitReason> &OnSessionQuit() { return SessionQuit; }
        const EventSource<bool> &OnLoginPINRequested() { return LoginPINRequested; }
        const EventSource<bool> &OnDataHolepunchProgress() { return DataHolepunchProgress; }
        const EventSource<const ChiakiRegisteredHost &> &OnAutoRegistSucceeded() { return AutoRegistSucceeded; }
        const EventSource<std::string> &OnNicknameReceived() { return NicknameReceived; }
        const EventSource<bool> &OnConnectedChanged() { return ConnectedChanged; }
        const EventSource<double> &OnMeasuredBitrateChanged() { return MeasuredBitrateChanged; }
        const EventSource<double> &OnAveragePacketLossChanged() { return AveragePacketLossChanged; }
        const EventSource<bool> &OnCantDisplayChanged() { return CantDisplayChanged; }

        void pressCross() { controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_CROSS; SendFeedbackState(); }
        void releaseCross() { controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_CROSS; SendFeedbackState(); }

        void pressCircle() { controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_MOON; SendFeedbackState(); }
        void releaseCircle() { controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_MOON; SendFeedbackState(); }

        void pressSquare() { controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_BOX; SendFeedbackState(); }
        void releaseSquare() { controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_BOX; SendFeedbackState(); }

        void pressTriangle() { controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_PYRAMID; SendFeedbackState(); }
        void releaseTriangle() { controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_PYRAMID; SendFeedbackState(); }

        void pressLeft() { controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_DPAD_LEFT; SendFeedbackState(); }
        void releaseLeft() { controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_DPAD_LEFT; SendFeedbackState(); }

        void pressRight() { controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_DPAD_RIGHT; SendFeedbackState(); }
        void releaseRight() { controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_DPAD_RIGHT; SendFeedbackState(); }

        void pressUp() { controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_DPAD_UP; SendFeedbackState(); }
        void releaseUp() { controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_DPAD_UP; SendFeedbackState(); }
 
        void pressDown() { controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_DPAD_DOWN; SendFeedbackState(); }
        void releaseDown() { controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_DPAD_DOWN; SendFeedbackState(); }
 
        void pressL1() { controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_L1; SendFeedbackState(); }
        void releaseL1() { controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_L1; SendFeedbackState(); }
 
        void pressR1() { controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_R1; SendFeedbackState(); }
        void releaseR1() { controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_R1; SendFeedbackState(); }
 
        void pressL3() { controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_L3; SendFeedbackState(); }
        void releaseL3() { controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_L3; SendFeedbackState(); }
 
        void pressR3() { controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_R3; SendFeedbackState(); }
        void releaseR3() { controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_R3; SendFeedbackState(); }
 
        void pressOptions() { controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_OPTIONS; SendFeedbackState(); }
        void releaseOptions() { controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_OPTIONS; SendFeedbackState(); }
 
        void pressCreate() { controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_SHARE; SendFeedbackState(); }
        void releaseCreate() { controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_SHARE; SendFeedbackState(); }
 
        void pressTouchpad() { controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_TOUCHPAD; SendFeedbackState(); }
        void releaseTouchpad() { controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_TOUCHPAD; SendFeedbackState(); }
 
        void pressPS() { controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_PS; SendFeedbackState(); }
        void releasePS() { controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_PS; SendFeedbackState(); }

        void setL2(uint8_t state) { controller_state.l2_state = state; SendFeedbackState(); }

        void setR2(uint8_t state) { controller_state.r2_state = state; SendFeedbackState(); }

        void setLeftX(int16_t x) { controller_state.left_x = x; SendFeedbackState(); }
        void setLeftY(int16_t y) { controller_state.left_y = y; SendFeedbackState(); }
        void setLeft(int16_t x, int16_t y) { controller_state.left_x = x; controller_state.left_y = y; SendFeedbackState(); }

        void setRightX(int16_t x) { controller_state.right_x = x; SendFeedbackState(); }
        void setRightY(int16_t y) { controller_state.right_y = y; SendFeedbackState(); }
        void setRight(int16_t x, int16_t y) { controller_state.right_x = x; controller_state.right_y = y; SendFeedbackState(); }

        void setAccelerometerX(float x) { controller_state.accel_x = x; SendFeedbackState(); }
        void setAccelerometerY(float y) { controller_state.accel_y = y; SendFeedbackState(); }
        void setAccelerometerZ(float z) { controller_state.accel_z = z; SendFeedbackState(); }
        void setAccelerometer(float x, float y, float z) { controller_state.accel_x = x; controller_state.accel_y = y; controller_state.accel_z = z; SendFeedbackState(); }

        void setGyroscopeX(float x) { controller_state.gyro_x = x; SendFeedbackState(); }
        void setGyroscopeY(float y) { controller_state.gyro_y = y; SendFeedbackState(); }
        void setGyroscopeZ(float z) { controller_state.gyro_z = z; SendFeedbackState(); }
        void setGyroscope(float x, float y, float z) { controller_state.gyro_x = x; controller_state.gyro_y = y; controller_state.gyro_z = z; SendFeedbackState(); }

        void setOrientationX(float x) { controller_state.orient_x = x; SendFeedbackState(); }
        void setOrientationY(float y) { controller_state.orient_y = y; SendFeedbackState(); }
        void setOrientationZ(float z) { controller_state.orient_z = z; SendFeedbackState(); }
        void setOrientationW(float w) { controller_state.orient_w = w; SendFeedbackState(); }
        void setOrientation(float x, float y, float z, float w) { controller_state.orient_x = x; controller_state.orient_y = y; controller_state.orient_z = z; controller_state.orient_w = w; SendFeedbackState(); }

        ChiakiControllerState controller_state;

        void SendFeedbackState()
        {
            ChiakiControllerState state;
            chiaki_controller_state_set_idle(&state);

            chiaki_controller_state_or(&state, &state, &controller_state);
            std::cout << "SendFeedbackState: " << state.buttons << std::endl;
            // chiaki_controller_state_or(&state, &state, &keyboard_state);
            // chiaki_controller_state_or(&state, &state, &touch_state);

            if (input_block)
            {
                // Only unblock input after all buttons were released
                if (input_block == 2 && !state.buttons)
                    input_block = 0;
                else
                {
                    // chiaki_controller_state_set_idle(&state);
                    // chiaki_controller_state_set_idle(&keyboard_state);
                }
            }
            if ((dpad_touch_shortcut1 || dpad_touch_shortcut2 || dpad_touch_shortcut3 || dpad_touch_shortcut4) && (!dpad_touch_shortcut1 || (state.buttons & dpad_touch_shortcut1)) && (!dpad_touch_shortcut2 || (state.buttons & dpad_touch_shortcut2)) && (!dpad_touch_shortcut3 || (state.buttons & dpad_touch_shortcut3)) && (!dpad_touch_shortcut4 || (state.buttons & dpad_touch_shortcut4)))
            {
                if (!dpad_regular_touch_switched)
                {
                    dpad_regular_touch_switched = true;
                    dpad_regular = !dpad_regular;
                }
            }
            else
                dpad_regular_touch_switched = false;
            /*if (dpad_touch_increment && !dpad_regular && (state.buttons & (CHIAKI_CONTROLLER_BUTTON_DPAD_DOWN | CHIAKI_CONTROLLER_BUTTON_DPAD_LEFT | CHIAKI_CONTROLLER_BUTTON_DPAD_RIGHT | CHIAKI_CONTROLLER_BUTTON_DPAD_UP)))
            {
                HandleDpadTouchEvent(&state);
            }
            else
            {
                if (dpad_touch_id >= 0 && !dpad_touch_stop_timer->isActive())
                    dpad_touch_stop_timer->start(NEW_DPAD_TOUCH_INTERVAL_MS);
            }*/
            // chiaki_controller_state_or(&state, &state, &dpad_touch_state);
            chiaki_session_set_controller_state(&session, &state);
        }
};

#endif // CHIAKI_PY_STREAMSESSION_H
