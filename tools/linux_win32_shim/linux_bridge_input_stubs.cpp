// Input API stubs so runtime_test_bridge.cpp links on Linux without XInput/Win32 input.
#include "engine/input/controller.h"
#include "engine/input/keyboard.h"
#include "engine/input/mouse.h"

void Keyboard_Initialize(void) {}
void Keyboard_Reset(void) {}
bool Keyboard_IsKeyDown(Keyboard_Keys) { return false; }
bool Keyboard_IsKeyUp(Keyboard_Keys) { return false; }
bool Keyboard_IsKeyDownTrigger(Keyboard_Keys) { return false; }
void Keyboard_ProcessMessage(UINT, WPARAM, LPARAM) {}
#if defined(_DEBUG)
void Keyboard_TestInject(const Keyboard_Keys*, int) {}
void Keyboard_ClearInjection() {}
bool Keyboard_IsInjectionActive() { return false; }
#endif

#if defined(_DEBUG)
static bool g_stubMouseInject = false;
static bool g_stubMouseAbs = false;
static int g_stubMouseAbsX = 0;
static int g_stubMouseAbsY = 0;
#endif
void Mouse_Initialize() {
#if defined(_DEBUG)
    g_stubMouseInject = false;
    g_stubMouseAbs = false;
    g_stubMouseAbsX = 0;
    g_stubMouseAbsY = 0;
#endif
}
void Mouse_ResetFrame() {}
void Mouse_ProcessMessage(UINT, WPARAM, LPARAM, HWND) {}
void Mouse_GetDelta(int& dx, int& dy) { dx = 0; dy = 0; }
bool Mouse_IsLeftDown() { return false; }
bool Mouse_IsRightDown() { return false; }
bool Mouse_IsMiddleDown() { return false; }
int Mouse_GetWheelDelta() { return 0; }
void Mouse_ClearWheel() {}
int Mouse_GetX() {
    int x = 0, y = 0;
    Mouse_GetPosition(x, y);
    return x;
}
int Mouse_GetY() {
    int x = 0, y = 0;
    Mouse_GetPosition(x, y);
    return y;
}
void Mouse_GetPosition(int& x, int& y) {
#if defined(_DEBUG)
    if (g_stubMouseInject && g_stubMouseAbs) {
        x = g_stubMouseAbsX;
        y = g_stubMouseAbsY;
        return;
    }
#endif
    x = 0;
    y = 0;
}
void Mouse_GetClientPosition(int& x, int& y) { x = 0; y = 0; }
#if defined(_DEBUG)
void Mouse_TestInject(int, int, unsigned int, int, bool hasAbsolute, int absX, int absY) {
    g_stubMouseInject = true;
    g_stubMouseAbs = hasAbsolute;
    g_stubMouseAbsX = absX;
    g_stubMouseAbsY = absY;
}
void Mouse_ClearInjection() {
    g_stubMouseInject = false;
    g_stubMouseAbs = false;
    g_stubMouseAbsX = 0;
    g_stubMouseAbsY = 0;
}
bool Mouse_IsInjectionActive() { return g_stubMouseInject; }
#endif

void Controller_Initialize() {}
void Controller_Finalize() {}
void Controller_Update() {}
bool Controller_IsConnected(int) { return false; }
bool Controller_IsPressed(ControllerButton, int) { return false; }
bool Controller_IsTrigger(ControllerButton, int) { return false; }
bool Controller_IsRelease(ControllerButton, int) { return false; }
DirectX::XMFLOAT2 Controller_GetLeftStick(int) { return {0.0f, 0.0f}; }
DirectX::XMFLOAT2 Controller_GetRightStick(int) { return {0.0f, 0.0f}; }
float Controller_GetLeftTrigger(int) { return 0.0f; }
float Controller_GetRightTrigger(int) { return 0.0f; }
void Controller_SetStickDeadzone(short, short) {}
void Controller_SetTriggerDeadzone(BYTE) {}
void Controller_SetVibration(float, float, int) {}
#if defined(_DEBUG)
void Controller_TestInjectLeftStick(float, float) {}
void Controller_TestInjectRightStick(float, float) {}
void Controller_TestInjectTriggers(float, float) {}
void Controller_TestInjectButtons(const ControllerButton*, int) {}
void Controller_TestInjectSample(float, float, float, float, float, float, unsigned short) {}
void Controller_ClearInjection() {}
bool Controller_IsInjectionActive() { return false; }
#endif
