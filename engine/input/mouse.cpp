/**===========================================================================

 File   :  [mouse.cpp]

 Author : Terry Yeung
 Date   : 2025/11/4
 -----------------------------------------------------------------------------

 ===========================================================================**/
#include "mouse.h"
#include <windowsx.h> // GET_X_LPARAM / GET_Y_LPARAM

float GetClientWidth();
float GetClientHeight();
float GetBackBufferWidth();
float GetBackBufferHeight();
void Renderer_ClientToBackbuffer(float clientX, float clientY, float& backbufferX, float& backbufferY);

#if defined(_DEBUG) && defined(GAME_RUNTIME)
static bool g_mouseInjectionActive = false;
static int g_injectedDx = 0;
static int g_injectedDy = 0;
static unsigned int g_injectedButtons = 0;
static int g_injectedWheel = 0;
static bool g_injectedHasAbsolute = false;
static int g_injectedAbsX = 0;
static int g_injectedAbsY = 0;

void Mouse_TestInject(int dx, int dy, unsigned int buttonsMask, int wheel,
                      bool hasAbsolute, int absX, int absY) {
    g_mouseInjectionActive = true;
    g_injectedDx = dx;
    g_injectedDy = dy;
    g_injectedButtons = buttonsMask;
    g_injectedWheel = wheel;
    g_injectedHasAbsolute = hasAbsolute;
    g_injectedAbsX = absX;
    g_injectedAbsY = absY;
}

void Mouse_ClearInjection() {
    g_mouseInjectionActive = false;
    g_injectedDx = 0;
    g_injectedDy = 0;
    g_injectedButtons = 0;
    g_injectedWheel = 0;
    g_injectedHasAbsolute = false;
    g_injectedAbsX = 0;
    g_injectedAbsY = 0;
}

bool Mouse_IsInjectionActive() {
    return g_mouseInjectionActive;
}
#endif

namespace {
    // Client coordinates from the latest Win32 message.
    static int g_lastClientX = 0, g_lastClientY = 0;
    static bool g_hasLast = false;
    static HWND g_hwnd = nullptr;

    // Accumulated movement delta for the current frame (client pixels).
    static int g_dx = 0, g_dy = 0;

    // Button state.
    static bool g_lDown = false;
    static bool g_rDown = false;
    static bool g_mDown = false;

    // Mouse wheel delta
    static int g_wheelDelta = 0;

    // Whether the cursor is inside the client area.
    static bool g_inClient = true;

    void UpdateClientPositionFromLParam(LPARAM lParam) {
        g_lastClientX = GET_X_LPARAM(lParam);
        g_lastClientY = GET_Y_LPARAM(lParam);
        g_hasLast = true;
        g_inClient = true;
    }

    void RefreshClientPositionFromCursor() {
        if (!g_hwnd) return;
        POINT pos;
        if (GetCursorPos(&pos) && ScreenToClient(g_hwnd, &pos)) {
            g_lastClientX = pos.x;
            g_lastClientY = pos.y;
            g_hasLast = true;
        }
    }

    void ClientToBackbufferInt(int clientX, int clientY, int& bufX, int& bufY) {
        float bx = 0.0f;
        float by = 0.0f;
        Renderer_ClientToBackbuffer((float)clientX, (float)clientY, bx, by);
        bufX = (int)bx;
        bufY = (int)by;
    }
}

void Mouse_Initialize() {
#if defined(_DEBUG) && defined(GAME_RUNTIME)
    Mouse_ClearInjection();
#endif
    g_lastClientX = g_lastClientY = 0;
    g_hasLast = false;
    g_hwnd = nullptr;
    g_dx = g_dy = 0;
    g_lDown = g_rDown = g_mDown = false;
    g_wheelDelta = 0;
    g_inClient = true;
}

void Mouse_ResetFrame() {
    // Delta is reset by Mouse_GetDelta.
}

void Mouse_ProcessMessage(UINT msg, WPARAM wParam, LPARAM lParam, HWND hWnd) {
    if (hWnd)
        g_hwnd = hWnd;

    switch (msg) {
        case WM_MOUSEMOVE:
        {
            const int x = GET_X_LPARAM(lParam);
            const int y = GET_Y_LPARAM(lParam);
            if (g_hasLast) {
                g_dx += (x - g_lastClientX);
                g_dy += (y - g_lastClientY);
            }
            g_lastClientX = x;
            g_lastClientY = y;
            g_hasLast = true;
            g_inClient = true;
            break;
        }
        case WM_LBUTTONDOWN: UpdateClientPositionFromLParam(lParam); g_lDown = true; break;
        case WM_LBUTTONUP:   UpdateClientPositionFromLParam(lParam); g_lDown = false; break;
        case WM_RBUTTONDOWN: UpdateClientPositionFromLParam(lParam); g_rDown = true; break;
        case WM_RBUTTONUP:   UpdateClientPositionFromLParam(lParam); g_rDown = false; break;
        case WM_MBUTTONDOWN: UpdateClientPositionFromLParam(lParam); g_mDown = true; break;
        case WM_MBUTTONUP:   UpdateClientPositionFromLParam(lParam); g_mDown = false; break;

        case WM_MOUSEWHEEL:
        {
            g_wheelDelta += GET_WHEEL_DELTA_WPARAM(wParam);
            break;
        }

        case WM_MOUSELEAVE:
            g_inClient = false;
            break;
        case WM_MOUSEHOVER:
        case WM_NCMOUSEMOVE:
            g_inClient = true;
            break;

        default:
            break;
    }
}

void Mouse_GetDelta(int& dx, int& dy) {
#if defined(_DEBUG) && defined(GAME_RUNTIME)
    if (g_mouseInjectionActive) {
        dx = g_injectedDx;
        dy = g_injectedDy;
        g_injectedDx = 0;
        g_injectedDy = 0;
        return;
    }
#endif
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    if (GetClientWidth() > 0.0f && GetClientHeight() > 0.0f) {
        scaleX = GetBackBufferWidth() / GetClientWidth();
        scaleY = GetBackBufferHeight() / GetClientHeight();
    }
    dx = (int)(g_dx * scaleX);
    dy = (int)(g_dy * scaleY);
    g_dx = g_dy = 0;
}

bool Mouse_IsLeftDown() {
#if defined(_DEBUG) && defined(GAME_RUNTIME)
    if (g_mouseInjectionActive) return (g_injectedButtons & MOUSE_BUTTON_LEFT) != 0;
#endif
    return g_lDown;
}

bool Mouse_IsRightDown() {
#if defined(_DEBUG) && defined(GAME_RUNTIME)
    if (g_mouseInjectionActive) return (g_injectedButtons & MOUSE_BUTTON_RIGHT) != 0;
#endif
    return g_rDown;
}

bool Mouse_IsMiddleDown() {
#if defined(_DEBUG) && defined(GAME_RUNTIME)
    if (g_mouseInjectionActive) return (g_injectedButtons & MOUSE_BUTTON_MIDDLE) != 0;
#endif
    return g_mDown;
}

int Mouse_GetWheelDelta() {
#if defined(_DEBUG) && defined(GAME_RUNTIME)
    if (g_mouseInjectionActive) {
        int delta = g_injectedWheel;
        g_injectedWheel = 0;
        return delta;
    }
#endif
    int delta = g_wheelDelta;
    g_wheelDelta = 0;
    return delta;
}

void Mouse_ClearWheel() {
#if defined(_DEBUG) && defined(GAME_RUNTIME)
    if (g_mouseInjectionActive) {
        g_injectedWheel = 0;
        return;
    }
#endif
    g_wheelDelta = 0;
}

int Mouse_GetX() {
    int x = 0;
    int y = 0;
    Mouse_GetPosition(x, y);
    return x;
}

int Mouse_GetY() {
    int x = 0;
    int y = 0;
    Mouse_GetPosition(x, y);
    return y;
}

void Mouse_GetPosition(int& x, int& y) {
#if defined(_DEBUG) && defined(GAME_RUNTIME)
    if (g_mouseInjectionActive && g_injectedHasAbsolute) {
        x = g_injectedAbsX;
        y = g_injectedAbsY;
        return;
    }
#endif
    RefreshClientPositionFromCursor();
    ClientToBackbufferInt(g_lastClientX, g_lastClientY, x, y);
}

// Raw Win32 client coordinates (used for cursor warping).
void Mouse_GetClientPosition(int& x, int& y) {
    RefreshClientPositionFromCursor();
    x = g_lastClientX;
    y = g_lastClientY;
}
