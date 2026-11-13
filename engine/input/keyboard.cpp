/**===========================================================================

 File   :  [keyboard.cpp]

 Author : Terry Yeung
 Date   : 2025/9/29
 -----------------------------------------------------------------------------
 http://go.microsoft.com/fwlink/?LinkId=248929
 http://go.microsoft.com/fwlink/?LinkID=615561
 ===========================================================================**/
#include "keyboard.h"

#include <assert.h>
 //#include "debugPrintf.h"

static_assert(sizeof(Keyboard_State) == 256 / 8, "Keyboard_State size mismatch");

static Keyboard_State gState = {};
static Keyboard_State gStateOld = {};
#if defined(_DEBUG)
static Keyboard_State gPhysicalState = {};
static bool gInjectionActive = false;

// Main/input owner thread only. Never sends OS keyboard input.
void Keyboard_TestInject(const Keyboard_Keys* keys, int count) {
    gInjectionActive = true;
    ZeroMemory(&gState, sizeof(gState));
    for(int i=0; i<count; ++i) {
        const int key = static_cast<int>(keys[i]);
        if(key > 0 && key <= 0xfe) {
            reinterpret_cast<unsigned int*>(&gState)[key >> 5] |= 1u << (key & 31);
        }
    }
}
void Keyboard_ClearInjection() {
    if(gInjectionActive) { gInjectionActive=false; gState=gPhysicalState; }
}
bool Keyboard_IsInjectionActive() { return gInjectionActive; }
#endif


void keycopy() {
	gStateOld = gState; //Save previous key state
}

static void keyDown(int key) {
	if (key < 0 || key > 0xfe) { return; }

	#if defined(_DEBUG)
    unsigned int* p = reinterpret_cast<unsigned int*>(&gPhysicalState);
#else
    unsigned int* p = reinterpret_cast<unsigned int*>(&gState);
#endif
	unsigned int bf = 1u << (key & 0x1f);

	p[(key >> 5)] |= bf;
#if defined(_DEBUG)
    if(!gInjectionActive) gState=gPhysicalState;
#endif
}

static void keyUp(int key) {
	if (key < 0 || key > 0xfe) { return; }

	#if defined(_DEBUG)
    unsigned int* p = reinterpret_cast<unsigned int*>(&gPhysicalState);
#else
    unsigned int* p = reinterpret_cast<unsigned int*>(&gState);
#endif
	unsigned int bf = 1u << (key & 0x1f);
	p[(key >> 5)] &= ~bf;
#if defined(_DEBUG)
    if(!gInjectionActive) gState=gPhysicalState;
#endif
}

void Keyboard_Initialize(void) {
#if defined(_DEBUG)
    gInjectionActive=false;
#endif
	Keyboard_Reset();
}

bool Keyboard_IsKeyDown(Keyboard_Keys key, const Keyboard_State* pState) {
	if (key <= 0xfe) {
		unsigned int* p = (unsigned int*)pState;
		unsigned int bf = 1u << (key & 0x1f);
		return (p[(key >> 5)] & bf) != 0;
	}
	return false;
}

//Edge-trigger check
bool Keyboard_IsKeyDownTrigger(Keyboard_Keys key) {
	if (key <= 0xfe) {
		unsigned int* p = (unsigned int*)&gState;
		unsigned int* p2 = (unsigned int*)&gStateOld;

		unsigned int bf = 1u << (key & 0x1f);

		return ((p[(key >> 5)] & bf) ^ (p2[(key >> 5)] & bf)) & (p[(key >> 5)] & bf);
	}
	return false;
}

bool Keyboard_IsKeyUp(Keyboard_Keys key, const Keyboard_State* pState) {
	if (key <= 0xfe) {
		unsigned int* p = (unsigned int*)pState;
		unsigned int bf = 1u << (key & 0x1f);
		return (p[(key >> 5)] & bf) == 0;
	}
	return false;
}

bool Keyboard_IsKeyDown(Keyboard_Keys key) {
	return Keyboard_IsKeyDown(key, &gState);
}

bool Keyboard_IsKeyUp(Keyboard_Keys key) {
	return Keyboard_IsKeyUp(key, &gState);
}

// Get the current keyboard state
const Keyboard_State* Keyboard_GetState(void) {
	return &gState;
}
const Keyboard_State* Keyboard_GetStateOld(void) {
	return &gStateOld;
}

void Keyboard_Reset(void) {
#if defined(_DEBUG)
    ZeroMemory(&gPhysicalState, sizeof(gPhysicalState));
    if(gInjectionActive) return; // Focus messages cannot erase synthetic input.
#endif
	ZeroMemory(&gState, sizeof(Keyboard_State));
	ZeroMemory(&gStateOld, sizeof(Keyboard_State));
}

// Window-message procedure hook for keyboard input
void Keyboard_ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam) {
	bool down = false;

	switch (message) {
	case WM_ACTIVATEAPP:
		Keyboard_Reset();
		return;

	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		down = true;
		break;

	case WM_KEYUP:
	case WM_SYSKEYUP:
		break;

	default:
		return;
	}

	int vk = (int)wParam;
	switch (vk) {
	case VK_SHIFT:
		vk = (int)MapVirtualKey(((unsigned int)lParam & 0x00ff0000) >> 16u, MAPVK_VSC_TO_VK_EX);
		if (!down) {
			// Workaround so left and right Shift both clear when released together
			keyUp(VK_LSHIFT);
			keyUp(VK_RSHIFT);
		}
		break;

	case VK_CONTROL:
		vk = ((UINT)lParam & 0x01000000) ? VK_RCONTROL : VK_LCONTROL;
		break;

	case VK_MENU:
		vk = ((UINT)lParam & 0x01000000) ? VK_RMENU : VK_LMENU;
		break;
	}

	if (down) {
		keyDown(vk);
	}
	else {
		keyUp(vk);
	}
}