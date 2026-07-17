/**===========================================================================

 File   :  [mouse.cpp]

 Author : Hayato Otsuki
 Date   : 2025/11/4
 -----------------------------------------------------------------------------

 ===========================================================================**/
#include "mouse.h"
#include "engine/rendering/d3d11/renderer.h"
#include <windowsx.h> // GET_X_LPARAM / GET_Y_LPARAM

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

bool Mouse_IsLeftDown() { return g_lDown; }
bool Mouse_IsRightDown() { return g_rDown; }
bool Mouse_IsMiddleDown() { return g_mDown; }

int Mouse_GetWheelDelta() {
    int delta = g_wheelDelta;
    g_wheelDelta = 0;
    return delta;
}

void Mouse_ClearWheel()
{
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
    RefreshClientPositionFromCursor();
    ClientToBackbufferInt(g_lastClientX, g_lastClientY, x, y);
}

// Raw Win32 client coordinates (used for cursor warping).
void Mouse_GetClientPosition(int& x, int& y) {
    RefreshClientPositionFromCursor();
    x = g_lastClientX;
    y = g_lastClientY;
}
