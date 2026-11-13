/**===========================================================================
 File   :  [controller.cpp]
 Author : Terry Yeung
 Date   : 2025/11/5
-----------------------------------------------------------------------------
 Xbox/PC controller input (XInput) implementation

 Usage:
  - Call Controller_Initialize() at startup
  - Call Controller_Update() at the start of each frame
  - Then query IsPressed/IsTrigger/IsRelease / GetLeftStick / etc. as needed
============================================================================**/
#include "controller.h"

#if defined(_DEBUG)
#include "controller_test_inject.h"
static_assert(static_cast<unsigned short>(ControllerButton::DPadUp) == ivy::testing::kInjectButtonDpadUp, "dpad up");
static_assert(static_cast<unsigned short>(ControllerButton::DPadDown) == ivy::testing::kInjectButtonDpadDown, "dpad down");
static_assert(static_cast<unsigned short>(ControllerButton::DPadLeft) == ivy::testing::kInjectButtonDpadLeft, "dpad left");
static_assert(static_cast<unsigned short>(ControllerButton::DPadRight) == ivy::testing::kInjectButtonDpadRight, "dpad right");
static_assert(static_cast<unsigned short>(ControllerButton::Start) == ivy::testing::kInjectButtonStart, "start");
static_assert(static_cast<unsigned short>(ControllerButton::Back) == ivy::testing::kInjectButtonBack, "back");
static_assert(static_cast<unsigned short>(ControllerButton::LeftThumb) == ivy::testing::kInjectButtonLeftThumb, "left thumb");
static_assert(static_cast<unsigned short>(ControllerButton::RightThumb) == ivy::testing::kInjectButtonRightThumb, "right thumb");
static_assert(static_cast<unsigned short>(ControllerButton::LB) == ivy::testing::kInjectButtonLB, "lb");
static_assert(static_cast<unsigned short>(ControllerButton::RB) == ivy::testing::kInjectButtonRB, "rb");
static_assert(static_cast<unsigned short>(ControllerButton::A) == ivy::testing::kInjectButtonA, "a");
static_assert(static_cast<unsigned short>(ControllerButton::B) == ivy::testing::kInjectButtonB, "b");
static_assert(static_cast<unsigned short>(ControllerButton::X) == ivy::testing::kInjectButtonX, "x");
static_assert(static_cast<unsigned short>(ControllerButton::Y) == ivy::testing::kInjectButtonY, "y");
#endif

#include <Xinput.h>
#pragma comment(lib, "xinput9_1_0.lib") // Some environments need XInput.lib instead
// #pragma comment(lib, "XInput.lib")

using namespace DirectX;

namespace {
    // Current / previous-frame XINPUT_STATE
    static XINPUT_STATE g_curr[CONTROLLER_MAX] {};
    static XINPUT_STATE g_prev[CONTROLLER_MAX] {};
    static bool         g_connected[CONTROLLER_MAX] {};

    // Normalized analog values
    static XMFLOAT2 g_leftStick[CONTROLLER_MAX] {};
    static XMFLOAT2 g_rightStick[CONTROLLER_MAX] {};
    static float    g_leftTrigger[CONTROLLER_MAX] {};
    static float    g_rightTrigger[CONTROLLER_MAX] {};

    // Dead zone
    static short g_deadzoneLeftStick = XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;   // 7849
    static short g_deadzoneRightStick = XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE;  // 8689
    static BYTE  g_deadzoneTrigger = XINPUT_GAMEPAD_TRIGGER_THRESHOLD;     // 30

#if defined(_DEBUG)
    static ivy::testing::ControllerInjectOverlay g_inject;
#endif

    inline bool TestButton(WORD buttons, ControllerButton btn) {
        return (buttons & static_cast<WORD>(btn)) != 0;
    }

    // Normalize to [-1,1] with dead zone (remap)
    inline float NormalizeThumb(short v, short dz) {
        const int iv = static_cast<int>(v);
        const int ad = (iv >= 0) ? iv : -iv;
        if (ad <= dz) return 0.0f;

        const int MAX_VAL = 32767; // SHORT max
        const float sign = (iv >= 0) ? 1.0f : -1.0f;
        const float norm = float(ad - dz) / float(MAX_VAL - dz); // Remap outside dead zone to 0..1
        float out = norm * sign;
        // Clamp
        if (out > 1.0f) out = 1.0f;
        if (out < -1.0f) out = -1.0f;
        return out;
    }

    // Normalize to [0,1] with dead zone
    inline float NormalizeTrigger(BYTE v, BYTE dz) {
        if (v <= dz) return 0.0f;
        const float norm = float(v - dz) / float(255 - dz);
        return (norm < 0.0f) ? 0.0f : (norm > 1.0f ? 1.0f : norm);
    }
} // anonymous

#if defined(_DEBUG)
void Controller_TestInjectLeftStick(float lx, float ly) {
    g_inject.SetLeftStick(lx, ly);
}

void Controller_TestInjectRightStick(float rx, float ry) {
    g_inject.SetRightStick(rx, ry);
}

void Controller_TestInjectTriggers(float left, float right) {
    g_inject.SetTriggers(left, right);
}

void Controller_TestInjectButtons(const ControllerButton* buttons, int count) {
    unsigned short mask = 0;
    if (buttons && count > 0) {
        for (int i = 0; i < count; ++i)
            mask = static_cast<unsigned short>(mask | static_cast<unsigned short>(buttons[i]));
    }
    g_inject.SetButtons(mask);
}

void Controller_TestInjectSample(float lx, float ly, float rx, float ry, float lt, float rt, unsigned short buttons) {
    ivy::testing::ControllerInjectSample sample;
    sample.lx = lx;
    sample.ly = ly;
    sample.rx = rx;
    sample.ry = ry;
    sample.lt = lt;
    sample.rt = rt;
    sample.buttons = buttons;
    g_inject.Apply(sample);
}

void Controller_ClearInjection() {
    g_inject.Clear();
}

bool Controller_IsInjectionActive() {
    return g_inject.Active();
}
#endif

void Controller_Initialize() {
#if defined(_DEBUG)
    Controller_ClearInjection();
#endif
    ZeroMemory(g_curr, sizeof(g_curr));
    ZeroMemory(g_prev, sizeof(g_prev));
    ZeroMemory(g_connected, sizeof(g_connected));
    for (int i = 0; i < CONTROLLER_MAX; ++i) {
        g_leftStick[i] = XMFLOAT2(0, 0);
        g_rightStick[i] = XMFLOAT2(0, 0);
        g_leftTrigger[i] = 0.0f;
        g_rightTrigger[i] = 0.0f;
    }
}

void Controller_Finalize() {
#if defined(_DEBUG)
    Controller_ClearInjection();
#endif
    // XInput needs no explicit teardown currently
}

void Controller_Update() {
    for (DWORD i = 0; i < CONTROLLER_MAX; ++i) {
        // Save previous frame
        g_prev[i] = g_curr[i];

        XINPUT_STATE st {};
        DWORD hr = XInputGetState(i, &st);
        if (hr == ERROR_SUCCESS) {
            g_connected[i] = true;
            g_curr[i] = st;

            // Normalize analog axes
            const XINPUT_GAMEPAD& gp = st.Gamepad;
            g_leftStick[i].x = NormalizeThumb(gp.sThumbLX, g_deadzoneLeftStick);
            g_leftStick[i].y = NormalizeThumb(gp.sThumbLY, g_deadzoneLeftStick);
            g_rightStick[i].x = NormalizeThumb(gp.sThumbRX, g_deadzoneRightStick);
            g_rightStick[i].y = NormalizeThumb(gp.sThumbRY, g_deadzoneRightStick);

            g_leftTrigger[i] = NormalizeTrigger(gp.bLeftTrigger, g_deadzoneTrigger);
            g_rightTrigger[i] = NormalizeTrigger(gp.bRightTrigger, g_deadzoneTrigger);
        }
        else {
            // Disconnected/error: treat as disconnected and zero out
            g_connected[i] = false;
            ZeroMemory(&g_curr[i], sizeof(XINPUT_STATE));
            g_leftStick[i] = XMFLOAT2(0, 0);
            g_rightStick[i] = XMFLOAT2(0, 0);
            g_leftTrigger[i] = 0.0f;
            g_rightTrigger[i] = 0.0f;
        }
    }
}

bool Controller_IsConnected(int padIndex) {
    if (padIndex < 0 || padIndex >= CONTROLLER_MAX) return false;
#if defined(_DEBUG)
    if (padIndex == 0 && g_inject.Active()) return true;
#endif
    return g_connected[padIndex];
}

bool Controller_IsPressed(ControllerButton button, int padIndex) {
#if defined(_DEBUG)
    if (padIndex == 0 && g_inject.Active())
        return g_inject.Pressed(static_cast<unsigned short>(button));
#endif
    if (!Controller_IsConnected(padIndex)) return false;
    return TestButton(g_curr[padIndex].Gamepad.wButtons, button);
}

bool Controller_IsTrigger(ControllerButton button, int padIndex) {
#if defined(_DEBUG)
    if (padIndex == 0 && g_inject.Active())
        return g_inject.PressedEdge(static_cast<unsigned short>(button));
#endif
    if (!Controller_IsConnected(padIndex)) return false;
    const WORD now = g_curr[padIndex].Gamepad.wButtons;
    const WORD prev = g_prev[padIndex].Gamepad.wButtons;
    const WORD b = static_cast<WORD>(button);
    return ((now & b) != 0) && ((prev & b) == 0);
}

bool Controller_IsRelease(ControllerButton button, int padIndex) {
#if defined(_DEBUG)
    if (padIndex == 0 && g_inject.Active())
        return g_inject.ReleasedEdge(static_cast<unsigned short>(button));
#endif
    if (!Controller_IsConnected(padIndex)) return false;
    const WORD now = g_curr[padIndex].Gamepad.wButtons;
    const WORD prev = g_prev[padIndex].Gamepad.wButtons;
    const WORD b = static_cast<WORD>(button);
    return ((now & b) == 0) && ((prev & b) != 0);
}

XMFLOAT2 Controller_GetLeftStick(int padIndex) {
#if defined(_DEBUG)
    if (padIndex == 0 && g_inject.Active()) return XMFLOAT2(g_inject.LeftX(), g_inject.LeftY());
#endif
    if (!Controller_IsConnected(padIndex)) return XMFLOAT2(0, 0);
    return g_leftStick[padIndex];
}

XMFLOAT2 Controller_GetRightStick(int padIndex) {
#if defined(_DEBUG)
    if (padIndex == 0 && g_inject.Active()) return XMFLOAT2(g_inject.RightX(), g_inject.RightY());
#endif
    if (!Controller_IsConnected(padIndex)) return XMFLOAT2(0, 0);
    return g_rightStick[padIndex];
}

float Controller_GetLeftTrigger(int padIndex) {
#if defined(_DEBUG)
    if (padIndex == 0 && g_inject.Active()) return g_inject.LeftTrigger();
#endif
    if (!Controller_IsConnected(padIndex)) return 0.0f;
    return g_leftTrigger[padIndex];
}

float Controller_GetRightTrigger(int padIndex) {
#if defined(_DEBUG)
    if (padIndex == 0 && g_inject.Active()) return g_inject.RightTrigger();
#endif
    if (!Controller_IsConnected(padIndex)) return 0.0f;
    return g_rightTrigger[padIndex];
}

void Controller_SetStickDeadzone(short left, short right) {
    // Clamp to valid range
    if (left < 0) left = 0;  if (left > 32767) left = 32767;
    if (right < 0) right = 0;  if (right > 32767) right = 32767;
    g_deadzoneLeftStick = left;
    g_deadzoneRightStick = right;
}

void Controller_SetTriggerDeadzone(BYTE trigger) {
    if (trigger > 255) trigger = 255;
    g_deadzoneTrigger = trigger;
}

void Controller_SetVibration(float leftMotor, float rightMotor, int padIndex) {
    if (!Controller_IsConnected(padIndex)) return;
    if (leftMotor < 0.0f) leftMotor = 0.0f;
    if (rightMotor < 0.0f) rightMotor = 0.0f;
    if (leftMotor > 1.0f) leftMotor = 1.0f;
    if (rightMotor > 1.0f) rightMotor = 1.0f;

    XINPUT_VIBRATION vib {};
    vib.wLeftMotorSpeed = static_cast<WORD>(leftMotor * 65535.0f + 0.5f);
    vib.wRightMotorSpeed = static_cast<WORD>(rightMotor * 65535.0f + 0.5f);
    XInputSetState(padIndex, &vib);
}