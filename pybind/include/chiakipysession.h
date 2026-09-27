// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef CHIAKI_PY_SESSION_H
#define CHIAKI_PY_SESSION_H

#include <string>
#include <map>
#include <vector>
#include <unordered_map>
#include <queue>
#include <tuple>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>

#include "timer.h"
#include "exception.h"
#include "sessionlog.h"
#include "chiaki_py_controller.h"
#include "settings.h"
#include "elapsed_timer.h"
#include "event_source.h"
#include "audio_handler.h"
#include "pylog.h"

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

// Everything ChiakiPySession needs to open a connection to an already-registered console.
// Built from the registration a prior Backend.register_host() produced (`host`, `nickname`,
// `regist_key`, `morning` i.e. the RP key, `target`) plus `settings`; the pythonic
// `chiaki_py.Session` builds one of these from a `HostRegistration` when it is created.
struct ChiakiPySessionConnectInfo
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

	ChiakiPySessionConnectInfo() {}
    ChiakiPySessionConnectInfo(
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

// A live or about-to-be-started connection to a console: start()/stop() it, feed it
// controller/motion input with the press_*/release_*/set_* methods, and pull decoded video
// through a FrameHandler built around it (see FrameHandler.get_frame() and the CpuFrameHandler/
// CudaFrameHandler/VulkanFrameHandler subclasses). The on_*() methods each return an event
// source frames/state changes can be subscribed to. Usually built and driven indirectly, via
// chiaki_py.Session, rather than used directly.
class ChiakiPySession
{
	friend class ChiakiPySessionPrivate;

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
        Timer packet_loss_timer; // its thread reads members of this object: stopped first in the destructor
        RumbleHapticsIntensity rumble_haptics_intensity;
		bool start_mic_unmuted;
		bool session_started;

		ChiakiFfmpegDecoder *ffmpeg_decoder;
        void TriggerFfmpegFrameAvailable();
        void TriggerAudioFrameAvailable(int16_t *buf, size_t samples_count);
        void InitAudio(unsigned int channels, unsigned int rate);
        std::string audio_out_device_name;
		std::string audio_in_device_name;
		// bool audio_out_drain_queue;
		// size_t haptics_buffer_size;

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

        AudioHandler audio_handler;

        EventSource<bool> AudioFrameAvailable;
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
		explicit ChiakiPySession(const ChiakiPySessionConnectInfo &connect_info);
		~ChiakiPySession();

		bool IsConnected()	{ return connected; } // Whether the session is connected.
		bool IsConnecting()	{ return connect_timer.isValid(); } // Whether the session is connecting.

        // The stream's video profile (width, height, max_fps, bitrate, codec): the requested one until the
        // console answers, then the negotiated one. Known before the first frame arrives.
        ChiakiConnectVideoProfile GetVideoProfile() const { return session.connect_info.video_profile; }

        // The AudioHandler queueing the session's decoded audio.
        AudioHandler &GetAudioHandler() { return audio_handler; }

        void Start(); // Start the stream session.
		void Stop(); // Stop the stream session.
		void GoToBed(); // Put the console into rest mode.
		void SetLoginPIN(const std::string &pin); // Answer the console's login PIN request.
		void GoHome(); // Go to the console's home screen.

		std::string GetHost() { return host; } // The console's address.
		bool GetConnected() { return connected; }
		double GetMeasuredBitrate()	{ return measured_bitrate; } // The measured bitrate in Mbit/s.
		double GetAveragePacketLoss()	{ return average_packet_loss; } // The average packet loss, 0 to 1.
		bool GetMuted()	{ return muted; } // Whether the microphone is muted.
		void SetAudioVolume(int volume) { audio_volume = volume; } // Stored only: playback does not apply it yet.
		bool GetCantDisplay()	{ return cant_display; } // Whether the console refused to stream what is on screen (e.g. HDCP-protected video).
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
        ChiakiFfmpegDecoder *GetFfmpegDecoder()	{ return ffmpeg_decoder; } // The FFmpeg decoder.

        // Whether the video decoder is hardware-accelerated, i.e. CudaFrameHandler/VulkanFrameHandler can return frames.
        bool HasHardwareDecoder();

        // Type of hardware decoder in use ('vulkan', 'cuda', 'd3d11va', ...), or '' if decoding on the CPU.
        std::string HardwareDecoderType();

        const EventSource<bool> &OnAudioFrameAvailable() { return AudioFrameAvailable; } // Fires whenever decoded audio was queued.
        const EventSource<bool> &OnFrameAvailable() { return FfmpegFrameAvailable; } // Fires whenever a decoded video frame is ready.
        const EventSource<ChiakiQuitReason> &OnSessionQuit() { return SessionQuit; } // Fires with the QuitReason when the session ended.
        const EventSource<bool> &OnLoginPINRequested() { return LoginPINRequested; } // Fires when the console asks for its login PIN, see set_login_pin().
        const EventSource<bool> &OnDataHolepunchProgress() { return DataHolepunchProgress; } // Fires as a PSN (holepunch) connection progresses.
        const EventSource<const ChiakiRegisteredHost &> &OnAutoRegistSucceeded() { return AutoRegistSucceeded; } // Fires with the RegisteredHost once auto-registration succeeded.
        const EventSource<std::string> &OnNicknameReceived() { return NicknameReceived; } // Fires with the console's nickname once it is known.
        const EventSource<bool> &OnConnectedChanged() { return ConnectedChanged; } // Fires when is_connected() changes.
        const EventSource<double> &OnMeasuredBitrateChanged() { return MeasuredBitrateChanged; } // Fires when get_measured_bitrate() changes.
        const EventSource<double> &OnAveragePacketLossChanged() { return AveragePacketLossChanged; } // Fires when get_average_packet_loss() changes.
        const EventSource<bool> &OnCantDisplayChanged() { return CantDisplayChanged; } // Fires when get_cant_display() changes.

        void pressCross() { controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_CROSS; SendFeedbackState(); } // Press the cross button.
        void releaseCross() { controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_CROSS; SendFeedbackState(); } // Release the cross button.

        void pressCircle() { controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_MOON; SendFeedbackState(); } // Press the circle button.
        void releaseCircle() { controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_MOON; SendFeedbackState(); } // Release the circle button.

        void pressSquare() { controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_BOX; SendFeedbackState(); } // Press the square button.
        void releaseSquare() { controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_BOX; SendFeedbackState(); } // Release the square button.

        void pressTriangle() { controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_PYRAMID; SendFeedbackState(); } // Press the triangle button.
        void releaseTriangle() { controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_PYRAMID; SendFeedbackState(); } // Release the triangle button.

        void pressLeft() { controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_DPAD_LEFT; SendFeedbackState(); } // Press the left button.
        void releaseLeft() { controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_DPAD_LEFT; SendFeedbackState(); } // Release the left button.

        void pressRight() { controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_DPAD_RIGHT; SendFeedbackState(); } // Press the right button.
        void releaseRight() { controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_DPAD_RIGHT; SendFeedbackState(); } // Release the right button.

        void pressUp() { controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_DPAD_UP; SendFeedbackState(); } // Press the up button.
        void releaseUp() { controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_DPAD_UP; SendFeedbackState(); } // Release the up button.
 
        void pressDown() { controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_DPAD_DOWN; SendFeedbackState(); } // Press the down button.
        void releaseDown() { controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_DPAD_DOWN; SendFeedbackState(); } // Release the down button.
 
        void pressL1() { controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_L1; SendFeedbackState(); } // Press the L1 button.
        void releaseL1() { controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_L1; SendFeedbackState(); } // Release the L1 button.
 
        void pressR1() { controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_R1; SendFeedbackState(); } // Press the R1 button.
        void releaseR1() { controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_R1; SendFeedbackState(); } // Release the R1 button.
 
        void pressL3() { controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_L3; SendFeedbackState(); } // Press the L3 button.
        void releaseL3() { controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_L3; SendFeedbackState(); } // Release the L3 button.
 
        void pressR3() { controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_R3; SendFeedbackState(); } // Press the R3 button.
        void releaseR3() { controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_R3; SendFeedbackState(); } // Release the R3 button.
 
        void pressOptions() { controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_OPTIONS; SendFeedbackState(); } // Press the options button.
        void releaseOptions() { controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_OPTIONS; SendFeedbackState(); } // Release the options button.
 
        void pressCreate() { controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_SHARE; SendFeedbackState(); } // Press the create button.
        void releaseCreate() { controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_SHARE; SendFeedbackState(); } // Release the create button.
 
        void pressTouchpad() { controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_TOUCHPAD; SendFeedbackState(); } // Press the touchpad button.
        void releaseTouchpad() { controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_TOUCHPAD; SendFeedbackState(); } // Release the touchpad button.
 
        void pressPS() { controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_PS; SendFeedbackState(); } // Press the PS button.
        void releasePS() { controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_PS; SendFeedbackState(); } // Release the PS button.

        void setL2(uint8_t state) { controller_state.l2_state = state; SendFeedbackState(); } // Set the L2 trigger state [0, 255].

        void setR2(uint8_t state) { controller_state.r2_state = state; SendFeedbackState(); } // Set the R2 trigger state [0, 255].

        void setLeftX(int16_t x) { controller_state.left_x = x; SendFeedbackState(); } // Set the left stick's x value [-32768, 32767], 0 is centered.
        void setLeftY(int16_t y) { controller_state.left_y = y; SendFeedbackState(); } // Set the left stick's y value [-32768, 32767], 0 is centered.
        void setLeft(int16_t x, int16_t y) { controller_state.left_x = x; controller_state.left_y = y; SendFeedbackState(); } // Set the left stick's x and y values [-32768, 32767], 0 is centered.

        void setRightX(int16_t x) { controller_state.right_x = x; SendFeedbackState(); } // Set the right stick's x value [-32768, 32767], 0 is centered.
        void setRightY(int16_t y) { controller_state.right_y = y; SendFeedbackState(); } // Set the right stick's y value [-32768, 32767], 0 is centered.
        void setRight(int16_t x, int16_t y) { controller_state.right_x = x; controller_state.right_y = y; SendFeedbackState(); } // Set the right stick's x and y values [-32768, 32767], 0 is centered.

        void setAccelerometerX(float x) { controller_state.accel_x = x; SendFeedbackState(); } // Set the accelerometer x value in g [-5, 5] (at rest: x=0, y=1, z=0).
        void setAccelerometerY(float y) { controller_state.accel_y = y; SendFeedbackState(); } // Set the accelerometer y value in g [-5, 5] (at rest: x=0, y=1, z=0).
        void setAccelerometerZ(float z) { controller_state.accel_z = z; SendFeedbackState(); } // Set the accelerometer z value in g [-5, 5] (at rest: x=0, y=1, z=0).
        void setAccelerometer(float x, float y, float z) { controller_state.accel_x = x; controller_state.accel_y = y; controller_state.accel_z = z; SendFeedbackState(); } // Set the accelerometer x, y and z values in g [-5, 5] (at rest: 0, 1, 0).

        void setGyroscopeX(float x) { controller_state.gyro_x = x; SendFeedbackState(); } // Set the gyroscope x value in rad/s [-30, 30] (at rest: 0).
        void setGyroscopeY(float y) { controller_state.gyro_y = y; SendFeedbackState(); } // Set the gyroscope y value in rad/s [-30, 30] (at rest: 0).
        void setGyroscopeZ(float z) { controller_state.gyro_z = z; SendFeedbackState(); } // Set the gyroscope z value in rad/s [-30, 30] (at rest: 0).
        void setGyroscope(float x, float y, float z) { controller_state.gyro_x = x; controller_state.gyro_y = y; controller_state.gyro_z = z; SendFeedbackState(); } // Set the gyroscope x, y and z values in rad/s [-30, 30] (at rest: 0).

        void setOrientationX(float x) { controller_state.orient_x = x; SendFeedbackState(); } // Set the orientation quaternion's x component [-1, 1] (at rest: x=y=z=0, w=1).
        void setOrientationY(float y) { controller_state.orient_y = y; SendFeedbackState(); } // Set the orientation quaternion's y component [-1, 1] (at rest: x=y=z=0, w=1).
        void setOrientationZ(float z) { controller_state.orient_z = z; SendFeedbackState(); } // Set the orientation quaternion's z component [-1, 1] (at rest: x=y=z=0, w=1).
        void setOrientationW(float w) { controller_state.orient_w = w; SendFeedbackState(); } // Set the orientation quaternion's w component [-1, 1] (at rest: x=y=z=0, w=1).
        void setOrientation(float x, float y, float z, float w) { controller_state.orient_x = x; controller_state.orient_y = y; controller_state.orient_z = z; controller_state.orient_w = w; SendFeedbackState(); } // Set the orientation as a unit quaternion (x, y, z, w) (at rest: 0, 0, 0, 1).

        ChiakiControllerState controller_state;

        // Send the current controller state to the console. The press_*/release_*/set_* methods do this themselves.
        void SendFeedbackState()
        {
            GilReleaseIfHeld release; // chiaki takes locks its threads may hold while waiting for the GIL to log
            ChiakiControllerState state;
            chiaki_controller_state_set_idle(&state);

            chiaki_controller_state_or(&state, &state, &controller_state);
            CHIAKI_LOGV(GetChiakiLog(), "SendFeedbackState: %u", state.buttons);
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

#endif // CHIAKI_PY_SESSION_H
