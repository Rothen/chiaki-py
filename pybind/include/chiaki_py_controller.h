// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef CHIAKI_PY_CONTROLLER_H
#define CHIAKI_PY_CONTROLLER_H

// #include "pylog.h"

#include <set>
#include <map>
#include <string>
#include <cstdint>
#include <functional>
#include <tuple>

#include <chiaki/session.h>
#include <chiaki/orientation.h>

#define PS_TOUCHPAD_MAXX 1920
#define PS_TOUCHPAD_MAXY 1079

class Controller
{
	public:
        Controller()
        {
            input_block = 0;
            chiaki_controller_state_set_idle(&controller_state);
        }
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

        void SendFeedbackState()
        {
            // GilReleaseIfHeld release; // chiaki takes locks its threads may hold while waiting for the GIL to log
            ChiakiControllerState state;
            chiaki_controller_state_set_idle(&state);

            chiaki_controller_state_or(&state, &state, &controller_state);
            // CHIAKI_LOGV(GetChiakiLog(), "SendFeedbackState: %u", state.buttons);
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
            // chiaki_session_set_controller_state(&session, &state);
        }

    private:
        ChiakiControllerState controller_state;
        int input_block;
        unsigned int dpad_touch_shortcut1;
        unsigned int dpad_touch_shortcut2;
        unsigned int dpad_touch_shortcut3;
        unsigned int dpad_touch_shortcut4;
        bool dpad_regular_touch_switched;
        bool dpad_regular;
};
#endif // CHIAKI_PY_CONTROLLER_H
