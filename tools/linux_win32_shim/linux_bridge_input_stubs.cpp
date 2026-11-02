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
#if defined(_DEBUG) && defined(GAME_RUNTIME)
void Keyboard_TestInject(const Keyboard_Keys*, int) {}
void Keyboard_ClearInjection() {}
bool Keyboard_IsInjectionActive() { return false; }
#endif

void Mouse_Initialize() {}
void Mouse_ResetFrame() {}
void Mouse_ProcessMessage(UINT, WPARAM, LPARAM, HWND) {}
void Mouse_GetDelta(int& dx, int& dy) { dx = 0; dy = 0; }
bool Mouse_IsLeftDown() { return false; }
bool Mouse_IsRightDown() { return false; }
bool Mouse_IsMiddleDown() { return false; }
int Mouse_GetWheelDelta() { return 0; }
void Mouse_ClearWheel() {}
int Mouse_GetX() { return 0; }
int Mouse_GetY() { return 0; }
void Mouse_GetPosition(int& x, int& y) { x = 0; y = 0; }
void Mouse_GetClientPosition(int& x, int& y) { x = 0; y = 0; }
#if defined(_DEBUG) && defined(GAME_RUNTIME)
void Mouse_TestInject(int, int, unsigned int, int) {}
void Mouse_ClearInjection() {}
bool Mouse_IsInjectionActive() { return false; }
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
#if defined(_DEBUG) && defined(GAME_RUNTIME)
void Controller_TestInjectLeftStick(float, float) {}
void Controller_TestInjectRightStick(float, float) {}
void Controller_TestInjectTriggers(float, float) {}
void Controller_TestInjectButtons(const ControllerButton*, int) {}
void Controller_TestInjectSample(float, float, float, float, float, float, unsigned short) {}
void Controller_ClearInjection() {}
bool Controller_IsInjectionActive() { return false; }
#endif
