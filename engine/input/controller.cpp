/**===========================================================================
 File   :  [controller.cpp]
 Author : Terry Yeung
 Date   : 2025/11/5
-----------------------------------------------------------------------------
 Xbox/PC �R���g���[���[���́iXInput�j����

 �g����:
  - �N������ Controller_Initialize()
  - ���t���[���擪�� Controller_Update()
  - �K�v�ɉ����� IsPressed/IsTrigger/IsRelease / GetLeftStick �Ȃǂ�Q��
============================================================================**/
#include "controller.h"

#include <Xinput.h>
#pragma comment(lib, "xinput9_1_0.lib") // ���ɂ���Ă� XInput.lib ���K�v
// #pragma comment(lib, "XInput.lib")

using namespace DirectX;

namespace {
    // ����/�O�t���[���� XINPUT_STATE
    static XINPUT_STATE g_curr[CONTROLLER_MAX] {};
    static XINPUT_STATE g_prev[CONTROLLER_MAX] {};
    static bool         g_connected[CONTROLLER_MAX] {};

    // ���K���ς݃A�i���O�l
    static XMFLOAT2 g_leftStick[CONTROLLER_MAX] {};
    static XMFLOAT2 g_rightStick[CONTROLLER_MAX] {};
    static float    g_leftTrigger[CONTROLLER_MAX] {};
    static float    g_rightTrigger[CONTROLLER_MAX] {};

    // �f�b�h�]�[��
    static short g_deadzoneLeftStick = XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;   // 7849
    static short g_deadzoneRightStick = XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE;  // 8689
    static BYTE  g_deadzoneTrigger = XINPUT_GAMEPAD_TRIGGER_THRESHOLD;     // 30

#if defined(_DEBUG) && defined(GAME_RUNTIME)
    static bool     g_injectActive = false;
    static XMFLOAT2 g_injectLeftStick{ 0.0f, 0.0f };
#endif

    inline bool TestButton(WORD buttons, ControllerButton btn) {
        return (buttons & static_cast<WORD>(btn)) != 0;
    }

    // [-1,1] ���K�� with �f�b�h�]�[���i�ă}�b�v�j
    inline float NormalizeThumb(short v, short dz) {
        const int iv = static_cast<int>(v);
        const int ad = (iv >= 0) ? iv : -iv;
        if (ad <= dz) return 0.0f;

        const int MAX_VAL = 32767; // SHORT �̍ő�
        const float sign = (iv >= 0) ? 1.0f : -1.0f;
        const float norm = float(ad - dz) / float(MAX_VAL - dz); // �f�b�h�]�[���O�� 0..1 �ɍă}�b�v
        float out = norm * sign;
        // �N�����v
        if (out > 1.0f) out = 1.0f;
        if (out < -1.0f) out = -1.0f;
        return out;
    }

    // [0,1] ���K�� with �f�b�h�]�[��
    inline float NormalizeTrigger(BYTE v, BYTE dz) {
        if (v <= dz) return 0.0f;
        const float norm = float(v - dz) / float(255 - dz);
        return (norm < 0.0f) ? 0.0f : (norm > 1.0f ? 1.0f : norm);
    }
} // anonymous

#if defined(_DEBUG) && defined(GAME_RUNTIME)
void Controller_TestInjectLeftStick(float lx, float ly) {
    g_injectLeftStick = { lx, ly };
    g_injectActive = true;
}

void Controller_ClearInjection() {
    g_injectLeftStick = { 0.0f, 0.0f };
    g_injectActive = false;
}

bool Controller_IsInjectionActive() {
    return g_injectActive;
}
#endif

void Controller_Initialize() {
#if defined(_DEBUG) && defined(GAME_RUNTIME)
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
#if defined(_DEBUG) && defined(GAME_RUNTIME)
    Controller_ClearInjection();
#endif
    // ����AXInput �͖����I�ȉ���s�v
}

void Controller_Update() {
    for (DWORD i = 0; i < CONTROLLER_MAX; ++i) {
        // �O�t���[����ۑ�
        g_prev[i] = g_curr[i];

        XINPUT_STATE st {};
        DWORD hr = XInputGetState(i, &st);
        if (hr == ERROR_SUCCESS) {
            g_connected[i] = true;
            g_curr[i] = st;

            // �A�i���O���K��
            const XINPUT_GAMEPAD& gp = st.Gamepad;
            g_leftStick[i].x = NormalizeThumb(gp.sThumbLX, g_deadzoneLeftStick);
            g_leftStick[i].y = NormalizeThumb(gp.sThumbLY, g_deadzoneLeftStick);
            g_rightStick[i].x = NormalizeThumb(gp.sThumbRX, g_deadzoneRightStick);
            g_rightStick[i].y = NormalizeThumb(gp.sThumbRY, g_deadzoneRightStick);

            g_leftTrigger[i] = NormalizeTrigger(gp.bLeftTrigger, g_deadzoneTrigger);
            g_rightTrigger[i] = NormalizeTrigger(gp.bRightTrigger, g_deadzoneTrigger);
        }
        else {
            // ���ڑ�/�G���[: �ؒf��ԂƂ��ă[����
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
#if defined(_DEBUG) && defined(GAME_RUNTIME)
    if (padIndex == 0 && g_injectActive) return true;
#endif
    return g_connected[padIndex];
}

bool Controller_IsPressed(ControllerButton button, int padIndex) {
#if defined(_DEBUG) && defined(GAME_RUNTIME)
    if (padIndex == 0 && g_injectActive) return false;
#endif
    if (!Controller_IsConnected(padIndex)) return false;
    return TestButton(g_curr[padIndex].Gamepad.wButtons, button);
}

bool Controller_IsTrigger(ControllerButton button, int padIndex) {
#if defined(_DEBUG) && defined(GAME_RUNTIME)
    if (padIndex == 0 && g_injectActive) return false;
#endif
    if (!Controller_IsConnected(padIndex)) return false;
    const WORD now = g_curr[padIndex].Gamepad.wButtons;
    const WORD prev = g_prev[padIndex].Gamepad.wButtons;
    const WORD b = static_cast<WORD>(button);
    return ((now & b) != 0) && ((prev & b) == 0);
}

bool Controller_IsRelease(ControllerButton button, int padIndex) {
#if defined(_DEBUG) && defined(GAME_RUNTIME)
    if (padIndex == 0 && g_injectActive) return false;
#endif
    if (!Controller_IsConnected(padIndex)) return false;
    const WORD now = g_curr[padIndex].Gamepad.wButtons;
    const WORD prev = g_prev[padIndex].Gamepad.wButtons;
    const WORD b = static_cast<WORD>(button);
    return ((now & b) == 0) && ((prev & b) != 0);
}

XMFLOAT2 Controller_GetLeftStick(int padIndex) {
#if defined(_DEBUG) && defined(GAME_RUNTIME)
    if (padIndex == 0 && g_injectActive) return g_injectLeftStick;
#endif
    if (!Controller_IsConnected(padIndex)) return XMFLOAT2(0, 0);
    return g_leftStick[padIndex];
}

XMFLOAT2 Controller_GetRightStick(int padIndex) {
#if defined(_DEBUG) && defined(GAME_RUNTIME)
    if (padIndex == 0 && g_injectActive) return XMFLOAT2(0, 0);
#endif
    if (!Controller_IsConnected(padIndex)) return XMFLOAT2(0, 0);
    return g_rightStick[padIndex];
}

float Controller_GetLeftTrigger(int padIndex) {
#if defined(_DEBUG) && defined(GAME_RUNTIME)
    if (padIndex == 0 && g_injectActive) return 0.0f;
#endif
    if (!Controller_IsConnected(padIndex)) return 0.0f;
    return g_leftTrigger[padIndex];
}

float Controller_GetRightTrigger(int padIndex) {
#if defined(_DEBUG) && defined(GAME_RUNTIME)
    if (padIndex == 0 && g_injectActive) return 0.0f;
#endif
    if (!Controller_IsConnected(padIndex)) return 0.0f;
    return g_rightTrigger[padIndex];
}

void Controller_SetStickDeadzone(short left, short right) {
    // �͈̓N�����v
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