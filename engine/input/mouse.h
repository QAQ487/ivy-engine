/**===========================================================================

 File   :  [mouse.h]

 Author : Terry Yeung
 Date   : 2025/11/4
 -----------------------------------------------------------------------------

 ===========================================================================**/
#ifndef MOUSE_H
#define MOUSE_H

#include <windows.h>

void Mouse_Initialize();
void Mouse_ResetFrame();              // call once per frame (resets delta)
void Mouse_ProcessMessage(UINT msg, WPARAM wParam, LPARAM lParam, HWND hWnd);

void Mouse_GetDelta(int& dx, int& dy); // read delta then reset to zero
bool Mouse_IsLeftDown();
bool Mouse_IsRightDown();
bool Mouse_IsMiddleDown();

int Mouse_GetWheelDelta();  // positive = scroll up; resets after read
void Mouse_ClearWheel();

int Mouse_GetX();
int Mouse_GetY();

// Back-buffer / render-space coordinates
void Mouse_GetPosition(int& x, int& y);

// Raw HWND client coordinates (SetCursorPos / ClientToScreen)
void Mouse_GetClientPosition(int& x, int& y);

#endif // !MOUSE_H
