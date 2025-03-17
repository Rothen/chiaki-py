// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include "settings.h"

#include <variant>
#include <chiaki/config.h>

#define SETTINGS_VERSION 2

Settings::Settings() : rumbleHapticsIntensity(RumbleHapticsIntensity::Normal),
                       buttonsByPosition(false),
                       startMicUnmuted(false),
                       hapticOverride(1.0),
                       resolutionLocalPS4(CHIAKI_VIDEO_RESOLUTION_PRESET_720p),
                       resolutionRemotePS4(CHIAKI_VIDEO_RESOLUTION_PRESET_720p),
                       resolutionLocalPS5(CHIAKI_VIDEO_RESOLUTION_PRESET_1080p),
                       resolutionRemotePS5(CHIAKI_VIDEO_RESOLUTION_PRESET_720p),
                       fpsLocalPS4(CHIAKI_VIDEO_FPS_PRESET_60),
                       fpsRemotePS4(CHIAKI_VIDEO_FPS_PRESET_60),
                       fpsLocalPS5(CHIAKI_VIDEO_FPS_PRESET_60),
                       fpsRemotePS5(CHIAKI_VIDEO_FPS_PRESET_60),
                       bitrateLocalPS4(0),
                       bitrateRemotePS4(0),
                       bitrateLocalPS5(0),
                       bitrateRemotePS5(0),
                       codecPS4(CHIAKI_CODEC_H264),
                       codecLocalPS5(CHIAKI_CODEC_H265),
                       codecRemotePS5(CHIAKI_CODEC_H265),
                       audioBufferSizeRaw(0),
                       decoder(Decoder::Ffmpeg),
                       hardwareDecoder("vulkan"),
                       packetLossMax(0.05),
                       audioVolume(100), // SDL_MIX_MAXVOLUME,
                       dpadTouchIncrement(30),
                       dpadTouchShortcut1(9),
                       dpadTouchShortcut2(10),
                       dpadTouchShortcut3(7),
                       dpadTouchShortcut4(0),
                       displayTargetContrast(0),
                       displayTargetPeak(0),
                       displayTargetTrc(0),
                       displayTargetPrim(0)
{ }

unsigned int Settings::GetAudioBufferSize() const
{
    unsigned int v = GetAudioBufferSizeRaw();
    return v ? v : GetAudioBufferSizeDefault();
}

ChiakiConnectVideoProfile Settings::GetVideoProfileLocalPS4()
{
    ChiakiConnectVideoProfile profile = {};
    chiaki_connect_video_profile_preset(&profile, GetResolutionLocalPS4(), GetFPSLocalPS4());
    unsigned int bitrate = GetBitrateLocalPS4();
    if (bitrate)
        profile.bitrate = bitrate;
    profile.codec = GetCodecPS4();
    return profile;
}

ChiakiConnectVideoProfile Settings::GetVideoProfileRemotePS4()
{
    ChiakiConnectVideoProfile profile = {};
    chiaki_connect_video_profile_preset(&profile, GetResolutionRemotePS4(), GetFPSRemotePS4());
    unsigned int bitrate = GetBitrateRemotePS4();
    if (bitrate)
        profile.bitrate = bitrate;
    profile.codec = GetCodecPS4();
    return profile;
}

ChiakiConnectVideoProfile Settings::GetVideoProfileLocalPS5()
{
    ChiakiConnectVideoProfile profile = {};
    chiaki_connect_video_profile_preset(&profile, GetResolutionLocalPS5(), GetFPSLocalPS5());
    unsigned int bitrate = GetBitrateLocalPS5();
    if (bitrate)
        profile.bitrate = bitrate;
    profile.codec = GetCodecLocalPS5();
    return profile;
}

ChiakiConnectVideoProfile Settings::GetVideoProfileRemotePS5()
{
    ChiakiConnectVideoProfile profile = {};
    chiaki_connect_video_profile_preset(&profile, GetResolutionRemotePS5(), GetFPSRemotePS5());
    unsigned int bitrate = GetBitrateRemotePS5();
    if (bitrate)
        profile.bitrate = bitrate;
    profile.codec = GetCodecRemotePS5();
    return profile;
}

std::string Settings::GetChiakiControllerButtonName(int button)
{
	switch(button)
	{
		case CHIAKI_CONTROLLER_BUTTON_CROSS      : return "Cross";
		case CHIAKI_CONTROLLER_BUTTON_MOON       : return "Moon";
		case CHIAKI_CONTROLLER_BUTTON_BOX        : return "Box";
		case CHIAKI_CONTROLLER_BUTTON_PYRAMID    : return "Pyramid";
		case CHIAKI_CONTROLLER_BUTTON_DPAD_LEFT  : return "D-Pad Left";
		case CHIAKI_CONTROLLER_BUTTON_DPAD_RIGHT : return "D-Pad Right";
		case CHIAKI_CONTROLLER_BUTTON_DPAD_UP    : return "D-Pad Up";
		case CHIAKI_CONTROLLER_BUTTON_DPAD_DOWN  : return "D-Pad Down";
		case CHIAKI_CONTROLLER_BUTTON_L1         : return "L1";
		case CHIAKI_CONTROLLER_BUTTON_R1         : return "R1";
		case CHIAKI_CONTROLLER_BUTTON_L3         : return "L3";
		case CHIAKI_CONTROLLER_BUTTON_R3         : return "R3";
		case CHIAKI_CONTROLLER_BUTTON_OPTIONS    : return "Options";
		case CHIAKI_CONTROLLER_BUTTON_SHARE      : return "Share";
		case CHIAKI_CONTROLLER_BUTTON_TOUCHPAD   : return "Touchpad";
		case CHIAKI_CONTROLLER_BUTTON_PS         : return "PS";
		case CHIAKI_CONTROLLER_ANALOG_BUTTON_L2  : return "L2";
		case CHIAKI_CONTROLLER_ANALOG_BUTTON_R2  : return "R2";
		case static_cast<int>(ControllerButtonExt::ANALOG_STICK_LEFT_X_UP)    : return "Left Stick Right";
		case static_cast<int>(ControllerButtonExt::ANALOG_STICK_LEFT_Y_UP)    : return "Left Stick Up";
		case static_cast<int>(ControllerButtonExt::ANALOG_STICK_RIGHT_X_UP)   : return "Right Stick Right";
		case static_cast<int>(ControllerButtonExt::ANALOG_STICK_RIGHT_Y_UP)   : return "Right Stick Up";
		case static_cast<int>(ControllerButtonExt::ANALOG_STICK_LEFT_X_DOWN)  : return "Left Stick Left";
		case static_cast<int>(ControllerButtonExt::ANALOG_STICK_LEFT_Y_DOWN)  : return "Left Stick Down";
		case static_cast<int>(ControllerButtonExt::ANALOG_STICK_RIGHT_X_DOWN) : return "Right Stick Left";
		case static_cast<int>(ControllerButtonExt::ANALOG_STICK_RIGHT_Y_DOWN) : return "Right Stick Down";
		case static_cast<int>(ControllerButtonExt::ANALOG_STICK_LEFT_X) : return "Left Stick X";
		case static_cast<int>(ControllerButtonExt::ANALOG_STICK_LEFT_Y) : return "Left Stick Y";
		case static_cast<int>(ControllerButtonExt::ANALOG_STICK_RIGHT_X) : return "Right Stick X";
		case static_cast<int>(ControllerButtonExt::ANALOG_STICK_RIGHT_Y) : return "Right Stick Y";
		case static_cast<int>(ControllerButtonExt::MISC1) : return "MIC";
		default: return "Unknown";
	}
}

void Settings::SetControllerButtonMapping(int chiaki_button, int key)
{
	/*auto button_name = replace(GetChiakiControllerButtonName(chiaki_button), " ", "_").toLower();
    std::transform(data.begin(), data.end(), data.begin(),
                   [](unsigned char c)
                   { return std::tolower(c); });
    settings.setValue("keymap/" + button_name, QKeySequence(key).toString());*/
}

std::map<int, int> Settings::GetControllerMapping()
{
	// Initialize with default values
	std::map<int, int> result =
	{
		{CHIAKI_CONTROLLER_BUTTON_CROSS     , 0},
		{CHIAKI_CONTROLLER_BUTTON_MOON      , 1},
		{CHIAKI_CONTROLLER_BUTTON_BOX       , 2},
		{CHIAKI_CONTROLLER_BUTTON_PYRAMID   , 3},
		{CHIAKI_CONTROLLER_BUTTON_DPAD_LEFT , 4},
		{CHIAKI_CONTROLLER_BUTTON_DPAD_RIGHT, 5},
		{CHIAKI_CONTROLLER_BUTTON_DPAD_UP   , 6},
		{CHIAKI_CONTROLLER_BUTTON_DPAD_DOWN , 7},
		{CHIAKI_CONTROLLER_BUTTON_L1        , 8},
		{CHIAKI_CONTROLLER_BUTTON_R1        , 9},
		{CHIAKI_CONTROLLER_BUTTON_L3        , 10},
		{CHIAKI_CONTROLLER_BUTTON_R3        , 11},
		{CHIAKI_CONTROLLER_BUTTON_OPTIONS   , 12},
		{CHIAKI_CONTROLLER_BUTTON_SHARE     , 13},
		{CHIAKI_CONTROLLER_BUTTON_TOUCHPAD  , 14},
		{CHIAKI_CONTROLLER_BUTTON_PS        , 15},
		{CHIAKI_CONTROLLER_ANALOG_BUTTON_L2 , 16},
		{CHIAKI_CONTROLLER_ANALOG_BUTTON_R2 , 17},
		{static_cast<int>(ControllerButtonExt::ANALOG_STICK_LEFT_X_UP)   , 18},
		{static_cast<int>(ControllerButtonExt::ANALOG_STICK_LEFT_X_DOWN) , 19},
		{static_cast<int>(ControllerButtonExt::ANALOG_STICK_LEFT_Y_UP)   , 20},
		{static_cast<int>(ControllerButtonExt::ANALOG_STICK_LEFT_Y_DOWN) , 21},
		{static_cast<int>(ControllerButtonExt::ANALOG_STICK_RIGHT_X_UP)  , 22},
		{static_cast<int>(ControllerButtonExt::ANALOG_STICK_RIGHT_X_DOWN), 23},
		{static_cast<int>(ControllerButtonExt::ANALOG_STICK_RIGHT_Y_UP)  , 24},
		{static_cast<int>(ControllerButtonExt::ANALOG_STICK_RIGHT_Y_DOWN), 25}
	};

	/*// Then fill in from settings
	auto chiaki_buttons = result.keys();
	for(auto chiaki_button : chiaki_buttons)
	{
		auto button_name = GetChiakiControllerButtonName(chiaki_button).replace(' ', '_').toLower();
		if(settings.contains("keymap/" + button_name))
			result[static_cast<int>(chiaki_button)] = QKeySequence(settings.value("keymap/" + button_name).toString())[0].key();
	}*/

	return result;
}

std::map<int, int> Settings::GetControllerMappingForDecoding()
{
	auto map = GetControllerMapping();
	std::map<int, int> result;
	for(auto it = map.begin(); it != map.end(); ++it)
	{
		result[it->first] = it->second;
	}
	return result;
}
