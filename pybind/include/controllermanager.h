// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef CHIAKI_PY_CONTROLLERMANAGER_H
#define CHIAKI_PY_CONTROLLERMANAGER_H

#include <set>
#include <map>
#include <string>
#include <cstdint>
#include <functional>
#include <tuple>
#include <chiaki/controller.h>
#include <chiaki/orientation.h>

#define PS_TOUCHPAD_MAXX 1920
#define PS_TOUCHPAD_MAXY 1079

class Controller;

class ControllerManager
{
	friend class Controller;

	private:
		std::map<int, Controller *> open_controllers;
		bool creating_controller_mapping;
		bool joystick_allow_background_events;
		bool is_app_active;
		bool moved;
		uint8_t dualsense_intensity;

		void ControllerClosed(Controller *controller);
		void CheckMoved();

	public:
		static ControllerManager *GetInstance();

		ControllerManager();
		~ControllerManager();
		void SetButtonsByPos();
		void SetAllowJoystickBackgroundEvents(bool enabled);
		void SetIsAppActive(bool active);
		void SetDualSenseIntensity(uint8_t intensity) { dualsense_intensity = intensity; };
		uint8_t GetDualSenseIntensity() { return dualsense_intensity; };
		void creatingControllerMapping(bool creating_controller_mapping);
		std::set<int> GetAvailableControllers();
        Controller *OpenController(int device_id);
        void UpdateAvailableControllers();
        void HandleEvents();

        std::function<void()> AvailableControllersUpdated;
        std::function<void()> ControllerMoved;
};

class Controller
{
	friend class ControllerManager;

	private:
		Controller(int device_id, ControllerManager *manager);
		int ref;
		ControllerManager *manager;
		int id;
		ChiakiOrientationTracker orientation_tracker;
		ChiakiControllerState state;
		bool updating_mapping_button;
		bool enable_analog_stick_mapping;
		bool is_dualsense;
		bool is_handheld;
		bool is_steam_virtual;
		bool is_steam_virtual_unmasked;
		bool is_dualsense_edge;
		bool has_led;
		bool micbutton_push;
		uint16_t firmware_version;

	public:
		~Controller();

		void Ref();
		void Unref();

		bool IsConnected();
		int GetDeviceID();
		std::string GetName();
		std::string GetVIDPIDString();
		std::string GetType();
		bool IsPS();
		std::string GetGUIDString();
		ChiakiControllerState GetState();
		void SetRumble(uint8_t left, uint8_t right);
		void SetTriggerEffects(uint8_t type_left, const uint8_t *data_left, uint8_t type_right, const uint8_t *data_right);
		void SetDualsenseMic(bool on);
		void SetHapticRumble(uint16_t left, uint16_t right);
		void StartUpdatingMapping();
		void IsUpdatingMappingButton(bool is_updating_mapping_button);
		void EnableAnalogStickMapping(bool enabled);
		void ChangeLEDColor(const uint8_t *led_color);
		bool IsDualSense();
		bool IsHandheld();
		bool IsSteamVirtual();
		bool IsSteamVirtualUnmasked();
		bool IsDualSenseEdge();
		void resetMotionControls();

		std::function<void()> StateChanged;
		std::function<void()> MicButtonPush;
        std::function<void(std::string button)> NewButtonMapping;
        std::function<void(Controller *controller)> UpdatingControllerMapping;
};
#endif // CHIAKI_PY_CONTROLLERMANAGER_H
