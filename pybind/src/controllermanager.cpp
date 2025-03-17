// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include "controllermanager.h"
#include <cassert>

/* PS5 trigger effect documentation:
   https://controllers.fandom.com/wiki/Sony_DualSense#FFB_Trigger_Modes

   Taken from SDL2, licensed under the zlib license,
   Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>
   https://github.com/libsdl-org/SDL/blob/release-2.24.1/test/testgamecontroller.c#L263-L289
*/
typedef struct
{
    uint8_t ucEnableBits1;                /* 0 */
    uint8_t ucEnableBits2;                /* 1 */
    uint8_t ucRumbleRight;                /* 2 */
    uint8_t ucRumbleLeft;                 /* 3 */
    uint8_t ucHeadphoneVolume;            /* 4 */
    uint8_t ucSpeakerVolume;              /* 5 */
    uint8_t ucMicrophoneVolume;           /* 6 */
    uint8_t ucAudioEnableBits;            /* 7 */
    uint8_t ucMicLightMode;               /* 8 */
    uint8_t ucAudioMuteBits;              /* 9 */
    uint8_t rgucRightTriggerEffect[11];   /* 10 */
    uint8_t rgucLeftTriggerEffect[11];    /* 21 */
    uint8_t rgucUnknown1[6];              /* 32 */
    uint8_t ucEnableBits3;                /* 38 */
    uint8_t rgucUnknown2[2];              /* 39 */
    uint8_t ucLedAnim;                    /* 41 */
    uint8_t ucLedBrightness;              /* 42 */
    uint8_t ucPadLights;                  /* 43 */
    uint8_t ucLedRed;                     /* 44 */
    uint8_t ucLedGreen;                   /* 45 */
    uint8_t ucLedBlue;                    /* 46 */
} DS5EffectsState_t;

static std::set<std::string> chiaki_motion_controller_guids{
	// Sony on Linux
	"03000000341a00003608000011010000",
	"030000004c0500006802000010010000",
	"030000004c0500006802000010810000",
	"030000004c0500006802000011010000",
	"030000004c0500006802000011810000",
	"030000006f0e00001402000011010000",
	"030000008f0e00000300000010010000",
	"050000004c0500006802000000010000",
	"050000004c0500006802000000800000",
	"050000004c0500006802000000810000",
	"05000000504c415953544154494f4e00",
	"060000004c0500006802000000010000",
	"030000004c050000a00b000011010000",
	"030000004c050000a00b000011810000",
	"030000004c050000c405000011010000",
	"030000004c050000c405000011810000",
	"030000004c050000cc09000000010000",
	"030000004c050000cc09000011010000",
	"030000004c050000cc09000011810000",
	"03000000c01100000140000011010000",
	"050000004c050000c405000000010000",
	"050000004c050000c405000000810000",
	"050000004c050000c405000001800000",
	"050000004c050000cc09000000010000",
	"050000004c050000cc09000000810000",
	"050000004c050000cc09000001800000",
	// Sony on iOS
	"050000004c050000cc090000df070000",
	// Sony on Android
	"050000004c05000068020000dfff3f00",
	"030000004c050000cc09000000006800",
	"050000004c050000c4050000fffe3f00",
	"050000004c050000cc090000fffe3f00",
	"050000004c050000cc090000ffff3f00",
	"35643031303033326130316330353564",
	// Sony on Mac OSx
	"030000004c050000cc09000000000000",
	"030000004c0500006802000000000000",
	"030000004c0500006802000000010000",
	"030000004c050000a00b000000010000",
	"030000004c050000c405000000000000",
	"030000004c050000c405000000010000",
	"030000004c050000cc09000000010000",
	"03000000c01100000140000000010000",
	// Sony on Windows
	"030000004c050000a00b000000000000",
	"030000004c050000c405000000000000",
	"030000004c050000cc09000000000000",
	"03000000250900000500000000000000",
	"030000004c0500006802000000000000",
	"03000000632500007505000000000000",
	"03000000888800000803000000000000",
	"030000008f0e00001431000000000000",
};

static std::set<std::tuple<uint16_t, uint16_t>> chiaki_dualsense_controller_ids({
	// in format (vendor id, product id)
	std::tuple<uint16_t, uint16_t>(0x054c, 0x0ce6), // DualSense controller
});

static std::set<std::tuple<uint16_t, uint16_t>> chiaki_dualsense_edge_controller_ids({
	// in format (vendor id, product id)
	std::tuple<uint16_t, uint16_t>(0x054c, 0x0df2), // DualSense Edge controller
});

static std::set<std::tuple<uint16_t, uint16_t>> chiaki_handheld_controller_ids({
	// in format (vendor id, product id)
	std::tuple<uint16_t, uint16_t>(0x28de, 0x1205), // Steam Deck
	std::tuple<uint16_t, uint16_t>(0x0b05, 0x1abe), // Rog Ally
	std::tuple<uint16_t, uint16_t>(0x17ef, 0x6182), // Legion Go
	std::tuple<uint16_t, uint16_t>(0x0db0, 0x1901), // MSI Claw
});

static std::set<std::tuple<uint16_t, uint16_t>> chiaki_steam_virtual_controller_ids({
	// in format (vendor id, product id)
	std::tuple<uint16_t, uint16_t>(0x28de, 0x11ff), // Steam Virtual Controller
});

static ControllerManager *instance = nullptr;

#define UPDATE_INTERVAL_MS 4
#define MOVE_CHECK_MS 1000

ControllerManager *ControllerManager::GetInstance()
{
	if(!instance)
		instance = new ControllerManager();
	return instance;
}

ControllerManager::ControllerManager() : 
    creating_controller_mapping(false),
	joystick_allow_background_events(true),
    is_app_active(true),
    dualsense_intensity(0x00)
{
	UpdateAvailableControllers();
}

ControllerManager::~ControllerManager()
{
}

void ControllerManager::SetAllowJoystickBackgroundEvents(bool enabled)
{
	this->joystick_allow_background_events = enabled;
}

void ControllerManager::CheckMoved()
{
	if(this->moved)
	{
		this->moved = false;
        ControllerMoved();
    }
}

void ControllerManager::SetIsAppActive(bool active)
{
	this->is_app_active = active;
}

void ControllerManager::SetButtonsByPos()
{
}

void ControllerManager::UpdateAvailableControllers()
{
}

void ControllerManager::creatingControllerMapping(bool creating_controller_mapping)
{
	this->creating_controller_mapping = creating_controller_mapping;
}

void ControllerManager::HandleEvents()
{
}

std::set<int> ControllerManager::GetAvailableControllers()
{
	return {};
}

Controller *ControllerManager::OpenController(int device_id)
{
	Controller *controller = open_controllers.at(device_id);
	if(!controller)
	{
		controller = new Controller(device_id, this);
		open_controllers[device_id] = controller;
	}
	controller->Ref();
	return controller;
}

void ControllerManager::ControllerClosed(Controller *controller)
{
	open_controllers.erase(controller->GetDeviceID());
}

Controller::Controller(int device_id, ControllerManager *manager) : ref(0),
                                                                    updating_mapping_button(false),
                                                                    enable_analog_stick_mapping(false)
                                                                    /*micbutton_push(false),
                                                                    is_dualsense(false),
                                                                    is_dualsense_edge(false),
                                                                    has_led(false),
                                                                    firmware_version(0),
                                                                    is_handheld(false),
                                                                    is_steam_virtual(false),
                                                                    is_steam_virtual_unmasked(false),*/
{
	this->id = device_id;
	this->manager = manager;
	chiaki_orientation_tracker_init(&this->orientation_tracker);
	chiaki_controller_state_set_idle(&this->state);
}

Controller::~Controller()
{
	assert(ref == 0);
}

void Controller::StartUpdatingMapping()
{
	UpdatingControllerMapping(this);
}

void Controller::IsUpdatingMappingButton(bool is_updating_mapping_button)
{
	this->updating_mapping_button = is_updating_mapping_button;
}

void Controller::EnableAnalogStickMapping(bool enabled)
{
	this->enable_analog_stick_mapping = enabled;
}

void Controller::Ref()
{
	ref++;
}

void Controller::Unref()
{
	if(--ref == 0)
	{
		manager->ControllerClosed(this);
		// deleteLater();
	}
}

bool Controller::IsConnected()
{
	return false;
}

int Controller::GetDeviceID()
{
	return -1;
}

std::string Controller::GetType()
{
	return std::string();
}

bool Controller::IsPS()
{
	return false;
}

std::string Controller::GetGUIDString()
{
	return std::string();
}

std::string Controller::GetName()
{
	return std::string();
}

std::string Controller::GetVIDPIDString()
{
	return std::string();
}

ChiakiControllerState Controller::GetState()
{
	return state;
}

void Controller::SetRumble(uint8_t left, uint8_t right)
{
}

void Controller::ChangeLEDColor(const uint8_t *led_color)
{
}

void Controller::SetTriggerEffects(uint8_t type_left, const uint8_t *data_left, uint8_t type_right, const uint8_t *data_right)
{
}

void Controller::SetDualsenseMic(bool on)
{
}

void Controller::SetHapticRumble(uint16_t left, uint16_t right)
{
}

bool Controller::IsDualSense()
{
	return false;
}

bool Controller::IsDualSenseEdge()
{
	return false;
}

bool Controller::IsHandheld()
{
	return false;
}

bool Controller::IsSteamVirtual()
{
	return false;
}

bool Controller::IsSteamVirtualUnmasked()
{
	return false;
}

void Controller::resetMotionControls()
{
}
