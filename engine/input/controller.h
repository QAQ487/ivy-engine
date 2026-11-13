/**===========================================================================
 File   :  [controller.h]
 Author : Terry Yeung
 Date   : 2025/11/5
-----------------------------------------------------------------------------
 Xbox/PC controller input (XInput) header
 - Initialize / update
 - Connection status
 - Button press / trigger / release
 - Analog input (sticks/triggers): normalize and apply dead zone
 - Vibration
============================================================================**/
#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <windows.h>
#include <DirectXMath.h>

// Max controllers to poll (XInput supports up to 4)
constexpr int CONTROLLER_MAX = 4;

// Button enum (maps to XINPUT_GAMEPAD_*)
enum class ControllerButton : WORD {
    DPadUp = 0x0001, // XINPUT_GAMEPAD_DPAD_UP
    DPadDown = 0x0002, // XINPUT_GAMEPAD_DPAD_DOWN
    DPadLeft = 0x0004, // XINPUT_GAMEPAD_DPAD_LEFT
    DPadRight = 0x0008, // XINPUT_GAMEPAD_DPAD_RIGHT
    Start = 0x0010, // XINPUT_GAMEPAD_START
    Back = 0x0020, // XINPUT_GAMEPAD_BACK
    LeftThumb = 0x0040, // XINPUT_GAMEPAD_LEFT_THUMB
    RightThumb = 0x0080, // XINPUT_GAMEPAD_RIGHT_THUMB
	// LT = 0x0040,
	// RT = 0x0080,
	LB = 0x0100,
	RB = 0x0200,
    A = 0x1000, // XINPUT_GAMEPAD_A
    B = 0x2000, // XINPUT_GAMEPAD_B
    X = 0x4000, // XINPUT_GAMEPAD_X
    Y = 0x8000  // XINPUT_GAMEPAD_Y
};

// Initialize / shut down
void Controller_Initialize();
void Controller_Finalize();

// Per-frame update (call once each frame)
void Controller_Update();

// Connection status
bool Controller_IsConnected(int padIndex = 0);

// Button state
bool Controller_IsPressed(ControllerButton button, int padIndex = 0);
bool Controller_IsTrigger(ControllerButton button, int padIndex = 0);  // Pressed this frame (edge)
bool Controller_IsRelease(ControllerButton button, int padIndex = 0);  // Released this frame (edge)

// Sticks (dead zone applied, normalized)
// Returns [-1,1]; X right-positive, Y up-positive (matches DirectXMath)
DirectX::XMFLOAT2 Controller_GetLeftStick(int padIndex = 0);
DirectX::XMFLOAT2 Controller_GetRightStick(int padIndex = 0);

// Triggers (dead zone applied, normalized)
// Returns [0,1]
float Controller_GetLeftTrigger(int padIndex = 0);
float Controller_GetRightTrigger(int padIndex = 0);

// Dead-zone settings
void Controller_SetStickDeadzone(short left, short right);
void Controller_SetTriggerDeadzone(BYTE trigger);

// Vibration (0.0f-1.0f)
void Controller_SetVibration(float leftMotor, float rightMotor, int padIndex = 0);

#if defined(_DEBUG)
void Controller_TestInjectLeftStick(float lx, float ly);
void Controller_TestInjectRightStick(float rx, float ry);
void Controller_TestInjectTriggers(float left, float right);
void Controller_TestInjectButtons(const ControllerButton* buttons, int count);
void Controller_TestInjectSample(float lx, float ly, float rx, float ry, float lt, float rt, unsigned short buttons);
void Controller_ClearInjection();
bool Controller_IsInjectionActive();
#endif

#endif // CONTROLLER_H