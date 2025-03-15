// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef CHIAKI_PY_SETTINGS_H
#define CHIAKI_PY_SETTINGS_H

#include <unordered_map>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <list>
#include <algorithm>
#include <functional>
#include <vector>
#include <chiaki/session.h>

enum class ControllerButtonExt
{
    // must not overlap with ChiakiControllerButton and ChiakiControllerAnalogButton
    ANALOG_STICK_LEFT_X_UP = (1 << 18),
    ANALOG_STICK_LEFT_X_DOWN = (1 << 19),
    ANALOG_STICK_LEFT_Y_UP = (1 << 20),
    ANALOG_STICK_LEFT_Y_DOWN = (1 << 21),
    ANALOG_STICK_RIGHT_X_UP = (1 << 22),
    ANALOG_STICK_RIGHT_X_DOWN = (1 << 23),
    ANALOG_STICK_RIGHT_Y_UP = (1 << 24),
    ANALOG_STICK_RIGHT_Y_DOWN = (1 << 25),
    ANALOG_STICK_LEFT_X = (1 << 26),
    ANALOG_STICK_LEFT_Y = (1 << 27),
    ANALOG_STICK_RIGHT_X = (1 << 28),
    ANALOG_STICK_RIGHT_Y = (1 << 29),
    MISC1 = (1 << 30),
};

enum class RumbleHapticsIntensity
{
	Off,
	VeryWeak,
	Weak,
	Normal,
	Strong,
	VeryStrong
};

enum class Decoder
{
	Ffmpeg,
	Pi
};

class Settings
{
	private:
		// std::string time_format;
		// std::map<HostMAC, HiddenHost> hidden_hosts;
		// std::map<HostMAC, RegisteredHost> registered_hosts;
		// std::map<std::string, RegisteredHost> nickname_registered_hosts;
		// std::map<std::string, std::string> controller_mappings;
		// std::list<std::string> profiles;
		// size_t ps4s_registered;
		// std::map<int, ManualHost> manual_hosts;
		// int manual_hosts_id_next;

        // Settings
        ChiakiDisableAudioVideo audioVideoDisabled;
        bool logVerbose;
        uint32_t logLevelMask;
        RumbleHapticsIntensity rumbleHapticsIntensity;
        bool buttonsByPosition;
        bool startMicUnmuted;
        float hapticOverride;
        ChiakiVideoResolutionPreset resolutionLocalPS4;
        ChiakiVideoResolutionPreset resolutionRemotePS4;
        ChiakiVideoResolutionPreset resolutionLocalPS5;
        ChiakiVideoResolutionPreset resolutionRemotePS5;
        ChiakiVideoFPSPreset fpsLocalPS4;
        ChiakiVideoFPSPreset fpsRemotePS4;
        ChiakiVideoFPSPreset fpsLocalPS5;
        ChiakiVideoFPSPreset fpsRemotePS5;
        unsigned int bitrateLocalPS4;
        unsigned int bitrateRemotePS4;
        unsigned int bitrateLocalPS5;
        unsigned int bitrateRemotePS5;
        ChiakiCodec codecPS4;
        ChiakiCodec codecLocalPS5;
        ChiakiCodec codecRemotePS5;
        int displayTargetContrast;
        int displayTargetPeak;
        int displayTargetTrc;
        int displayTargetPrim;
        Decoder decoder;
        std::string hardwareDecoder;
        float packetLossMax;
        int audioVolume;
        unsigned int audioBufferSizeDefault;
        unsigned int audioBufferSizeRaw;
        unsigned int audioBufferSize;
        std::string audioOutDevice;
        std::string audioInDevice;
        std::string psnAuthToken;
        bool dpadTouchEnabled;
        uint16_t dpadTouchIncrement;
        unsigned int dpadTouchShortcut1;
        unsigned int dpadTouchShortcut2;
        unsigned int dpadTouchShortcut3;
        unsigned int dpadTouchShortcut4;
        std::string psnAccountId;
        ChiakiConnectVideoProfile videoProfileLocalPS4;
        ChiakiConnectVideoProfile videoProfileRemotePS4;
        ChiakiConnectVideoProfile videoProfileLocalPS5;
        ChiakiConnectVideoProfile videoProfileRemotePS5;
        static std::string chiakiControllerButtonName;
        std::map<int, int> controllerMapping;
        std::map<int, int> controllerMappingForDecoding;

	public:
		explicit Settings();

		ChiakiDisableAudioVideo GetAudioVideoDisabled() const { return audioVideoDisabled;  }

        bool GetLogVerbose() const { return logVerbose; }
        void SetLogVerbose(bool logVerbose) { this->logVerbose = logVerbose; }
        uint32_t GetLogLevelMask() const { uint32_t mask = CHIAKI_LOG_ALL; if (!GetLogVerbose()) { mask &= ~CHIAKI_LOG_VERBOSE; } return mask; };

        RumbleHapticsIntensity GetRumbleHapticsIntensity() const { return rumbleHapticsIntensity; };
        void SetRumbleHapticsIntensity(RumbleHapticsIntensity rumbleHapticsIntensity) { this->rumbleHapticsIntensity = rumbleHapticsIntensity; }

        bool GetButtonsByPosition() const { return buttonsByPosition; }
        void SetButtonsByPosition(bool buttonsByPosition) { this->buttonsByPosition = buttonsByPosition; }

        bool GetStartMicUnmuted() const { return startMicUnmuted; }
        void SetStartMicUnmuted(bool startMicUnmuted) { this->startMicUnmuted = startMicUnmuted; }

        float GetHapticOverride() const { return hapticOverride; }
        void SetHapticOverride(float hapticOverride) { this->hapticOverride = hapticOverride; }

        ChiakiVideoResolutionPreset GetResolutionLocalPS4() const { return resolutionLocalPS4; }
        ChiakiVideoResolutionPreset GetResolutionRemotePS4() const { return resolutionRemotePS4; }
		ChiakiVideoResolutionPreset GetResolutionLocalPS5() const { return resolutionLocalPS5; }
		ChiakiVideoResolutionPreset GetResolutionRemotePS5() const { return resolutionRemotePS5; }
		void SetResolutionLocalPS4(ChiakiVideoResolutionPreset resolutionLocalPS4) { this->resolutionLocalPS4 = resolutionLocalPS4; }
		void SetResolutionRemotePS4(ChiakiVideoResolutionPreset resolutionRemotePS4) { this->resolutionRemotePS4 = resolutionRemotePS4; }
		void SetResolutionLocalPS5(ChiakiVideoResolutionPreset resolutionLocalPS5) { this->resolutionLocalPS5 = resolutionLocalPS5; }
		void SetResolutionRemotePS5(ChiakiVideoResolutionPreset resolutionRemotePS5) { this->resolutionRemotePS5 = resolutionRemotePS5; }

		/**
		 * @return 0 if set to "automatic"
		 */
		ChiakiVideoFPSPreset GetFPSLocalPS4() const { return fpsLocalPS4; }
		ChiakiVideoFPSPreset GetFPSRemotePS4() const { return fpsRemotePS4; }
		ChiakiVideoFPSPreset GetFPSLocalPS5() const { return fpsLocalPS5; }
		ChiakiVideoFPSPreset GetFPSRemotePS5() const { return fpsRemotePS5; }
		void SetFPSLocalPS4(ChiakiVideoFPSPreset fpsLocalPS4) { this->fpsLocalPS4 = fpsLocalPS4; };
		void SetFPSRemotePS4(ChiakiVideoFPSPreset fpsRemotePS4) { this->fpsRemotePS4 = fpsRemotePS4; };
		void SetFPSLocalPS5(ChiakiVideoFPSPreset fpsLocalPS5) { this->fpsLocalPS5 = fpsLocalPS5; };
		void SetFPSRemotePS5(ChiakiVideoFPSPreset fpsRemotePS5) { this->fpsRemotePS5 = fpsRemotePS5; };

		unsigned int GetBitrateLocalPS4() const { return bitrateLocalPS4; }
		unsigned int GetBitrateRemotePS4() const { return bitrateRemotePS4; }
		unsigned int GetBitrateLocalPS5() const { return bitrateLocalPS5; }
		unsigned int GetBitrateRemotePS5() const { return bitrateRemotePS5; }
		void SetBitrateLocalPS4(unsigned int bitrateLocalPS4) { this->bitrateLocalPS4 = bitrateLocalPS4; }
		void SetBitrateRemotePS4(unsigned int bitrateRemotePS4) { this->bitrateRemotePS4 = bitrateRemotePS4; }
		void SetBitrateLocalPS5(unsigned int bitrateLocalPS5) { this->bitrateLocalPS5 = bitrateLocalPS5; }
		void SetBitrateRemotePS5(unsigned int bitrateRemotePS5) { this->bitrateRemotePS5 = bitrateRemotePS5; }

		ChiakiCodec GetCodecPS4() const { return codecPS4; }
		ChiakiCodec GetCodecLocalPS5() const { return codecLocalPS5; }
		ChiakiCodec GetCodecRemotePS5() const { return codecRemotePS5; }
		void SetCodecPS4(ChiakiCodec codecPS4) { this->codecPS4 = codecPS4; }
		void SetCodecLocalPS5(ChiakiCodec codecLocalPS5) { this->codecLocalPS5 = codecLocalPS5; }
		void SetCodecRemotePS5(ChiakiCodec codecRemotePS5) { this->codecRemotePS5 = codecRemotePS5; }

		int GetDisplayTargetContrast() const { return displayTargetContrast; }
		void SetDisplayTargetContrast(int displayTargetContrast) { this->displayTargetContrast = displayTargetContrast; }

		int GetDisplayTargetPeak() const { return displayTargetPeak; }
		void SetDisplayTargetPeak(int displayTargetPeak) { this->displayTargetPeak = displayTargetPeak; }

		int GetDisplayTargetTrc() const { return displayTargetTrc; }
		void SetDisplayTargetTrc(int displayTargetTrc) { this->displayTargetTrc = displayTargetTrc; }

		int GetDisplayTargetPrim() const { return displayTargetPrim; }
		void SetDisplayTargetPrim(int displayTargetPrim) { this->displayTargetPrim = displayTargetPrim; }

        Decoder GetDecoder() const { return decoder; }
        void SetDecoder(Decoder decoder) { this->decoder = decoder; }

        std::string GetHardwareDecoder() const { return hardwareDecoder; };
        void SetHardwareDecoder(const std::string hardwareDecoder) { this->hardwareDecoder = hardwareDecoder; };

        float GetPacketLossMax() const { return packetLossMax;}
        void SetPacketLossMax(float packetLossMax) { this->packetLossMax = packetLossMax; }

        int GetAudioVolume() const { return audioVolume; }
        void SetAudioVolume(int audioVolume) { this->audioVolume = audioVolume; }

        unsigned int GetAudioBufferSizeDefault() const { return 9600; }

        /**
         * @return 0 if set to "automatic"
         */
        unsigned int GetAudioBufferSizeRaw() const { return audioBufferSize; }

        /**
         * @return actual audioBufferSize to be used, default value if GetAudioBufferSizeRaw() would return 0
         */
        unsigned int GetAudioBufferSize() const;
        void SetAudioBufferSize(unsigned int audioBufferSize) { this->audioBufferSize = audioBufferSize; }

        std::string GetAudioOutDevice() const { return audioOutDevice; }
		void SetAudioOutDevice(std::string audioOutDevice) { this->audioOutDevice = audioOutDevice; }

		std::string GetAudioInDevice() const { return audioInDevice; }
        void SetAudioInDevice(std::string audioInDevice) { this->audioInDevice = audioInDevice; }

        std::string GetPsnAuthToken() const { return psnAuthToken; }
        void SetPsnAuthToken(std::string psnAuthToken) { this->psnAuthToken = psnAuthToken; }

        bool GetDpadTouchEnabled() const { return dpadTouchEnabled; }
        void SetDpadTouchEnabled(bool dpadTouchEnabled) { this->dpadTouchEnabled = dpadTouchEnabled; }

		uint16_t GetDpadTouchIncrement() const { return dpadTouchIncrement; }
        void SetDpadTouchIncrement(uint16_t dpadTouchIncrement) { this->dpadTouchIncrement = dpadTouchIncrement; }

		unsigned int GetDpadTouchShortcut1() const { return dpadTouchShortcut1; }
		void SetDpadTouchShortcut1(unsigned int dpadTouchShortcut1) { this->dpadTouchShortcut1 = dpadTouchShortcut1; }

		unsigned int GetDpadTouchShortcut2() const { return dpadTouchShortcut2; }
		void SetDpadTouchShortcut2(unsigned int dpadTouchShortcut2) { this->dpadTouchShortcut2 = dpadTouchShortcut2; }

		unsigned int GetDpadTouchShortcut3() const { return dpadTouchShortcut3; }
		void SetDpadTouchShortcut3(unsigned int dpadTouchShortcut3) { this->dpadTouchShortcut3 = dpadTouchShortcut3; }

		unsigned int GetDpadTouchShortcut4() const { return dpadTouchShortcut4; }
		void SetDpadTouchShortcut4(unsigned int dpadTouchShortcut4) { this->dpadTouchShortcut4 = dpadTouchShortcut4; }

		std::string GetPsnAccountId() const { return psnAccountId; }
        void SetPsnAccountId(std::string psnAccountId) { this->psnAccountId = psnAccountId; }

        ChiakiConnectVideoProfile GetVideoProfileLocalPS4();
		ChiakiConnectVideoProfile GetVideoProfileRemotePS4();
		ChiakiConnectVideoProfile GetVideoProfileLocalPS5();
		ChiakiConnectVideoProfile GetVideoProfileRemotePS5();

        static std::string GetChiakiControllerButtonName(int button);
        void SetControllerButtonMapping(int chiaki_button, int key);
        std::map<int, int> GetControllerMapping();
		std::map<int, int> GetControllerMappingForDecoding();

		/*std::function<void()> RegisteredHostsUpdated;
		std::function<void()> HiddenHostsUpdated;
		std::function<void()> ManualHostsUpdated;
		std::function<void()> ControllerMappingsUpdated;
		std::function<void()> CurrentProfileChanged;
		std::function<void()> ProfilesUpdated;
		std::function<void()> PlaceboSettingsUpdated;*/
};

#endif // CHIAKI_PY_SETTINGS_H
