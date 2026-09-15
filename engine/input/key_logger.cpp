/**===========================================================================
 File   : key_logger.cpp
 Title  :
 Author : Terry Yeung
 Date   : 2025/6/27
 Brief  :
 ===========================================================================**/
#include "key_logger.h"
#include "../core/system_timer.h"
#include <map>

static Keyboard_State g_prev_state = {};
static Keyboard_State g_trigger_state = {};
static Keyboard_State g_release_state = {};
static Keyboard_State g_double_tap_state = {};
static Keyboard_State g_repeat_state = {};
static Keyboard_State g_longpress_state = {};
static double g_hold_time[256] = {};
static double g_last_press_time[256] = {};
static double g_last_repeat_time[256] = {};
static double g_current_time = 0.0;
static double g_last_update_time = 0.0;
static bool g_has_last_update_time = false;

void KeyLogger_Initialize() {
	Keyboard_Initialize();
	for (int i = 0; i < 256; ++i) {
		g_hold_time[i] = 0.0;
		g_last_press_time[i] = 0.0;
		g_last_repeat_time[i] = 0.0;
	}
}

void KeyLogger_Update(double now_time) {
	const Keyboard_State* state = Keyboard_GetState();
	g_current_time = now_time;  // 渡された時間を使う！

	// Real frame delta instead of an assumed 60FPS, so hold-time accumulates
	// correctly at any frame rate (falls back to 1/60s on the very first call).
	double dt = g_has_last_update_time ? (now_time - g_last_update_time) : (1.0 / 60.0);
	if (dt < 0.0) dt = 0.0;
	g_last_update_time = now_time;
	g_has_last_update_time = true;

	// LPBYTE : unsigned char*
	LPBYTE pt = (LPBYTE)&g_trigger_state;
	LPBYTE pn = (LPBYTE)state;
	LPBYTE po = (LPBYTE)&g_prev_state;
	LPBYTE pr = (LPBYTE)&g_release_state;
	LPBYTE pd = (LPBYTE)&g_double_tap_state;
	LPBYTE prp = (LPBYTE)&g_repeat_state;
	LPBYTE plp = (LPBYTE)&g_longpress_state;

	// トリガーなどのフラグ配列初期化
	for (int i = 0; i < sizeof(Keyboard_State); i++) {
		pt[i] = (po[i] ^ pn[i]) & pn[i];
		pr[i] = po[i] & ~pn[i];
		pd[i] = 0;
		prp[i] = 0;
		plp[i] = 0;
	}

	// 各キーに対する時間判定
	for (int k = 0; k < 256; ++k) {
		bool is_down = Keyboard_IsKeyDown((Keyboard_Keys)k, state);
		bool is_trigger = Keyboard_IsKeyDown((Keyboard_Keys)k, &g_trigger_state);

		if (is_down) {
			g_hold_time[k] += dt;
		}
		else {
			g_hold_time[k] = 0.0;
		}

		if (is_trigger) {
			double delta = g_current_time - g_last_press_time[k];

			if (delta > 0.0 && delta < 0.3) {
				// 0.3秒以内ならダブルタップ
				Keyboard_SetKeyDown(&g_double_tap_state, (Keyboard_Keys)k, true);
				// 成功したらタイムスタンプをリセット
				g_last_press_time[k] = 0.0;
			}
			else {
				// 0.3秒以上経っていたら、
				// 1回目のタイムスタンプを今回に更新（2回目としては認めない）
				g_last_press_time[k] = g_current_time;
			}
		}
		else {
			// 押されてないときに、タイムアウトで1回目を無効にする
			double delta = g_current_time - g_last_press_time[k];
			if (delta >= 0.3) {
				g_last_press_time[k] = 0.0; // 有効期限切れ
			}
		}

		if (is_down) {
			if (g_current_time - g_last_repeat_time[k] >= 0.25) {
				Keyboard_SetKeyDown(&g_repeat_state, (Keyboard_Keys)k, true);
				g_last_repeat_time[k] = g_current_time;
			}
		}
		else {
			g_last_repeat_time[k] = 0.0;
		}

		if (g_hold_time[k] >= 1.0) {
			Keyboard_SetKeyDown(&g_longpress_state, (Keyboard_Keys)k, true);
		}
	}

	g_prev_state = *state;
}

static void Keyboard_SetKeyDown(Keyboard_State* state, Keyboard_Keys key, bool down) {
	unsigned int* p = (unsigned int*)state;
	unsigned int bit = 1u << (key & 0x1F);
	unsigned int index = key >> 5;

	if (down) p[index] |= bit;
	else      p[index] &= ~bit;
}

bool KeyLogger_IsPressed(Keyboard_Keys key) {
	return Keyboard_IsKeyDown(key);
}

bool KeyLogger_IsTrigger(Keyboard_Keys key) {
	return Keyboard_IsKeyDown(key, &g_trigger_state);
}

bool KeyLogger_IsRelease(Keyboard_Keys key) {
	return Keyboard_IsKeyDown(key, &g_release_state);
}

bool KeyLogger_IsDoubleTap(Keyboard_Keys key) {
	return Keyboard_IsKeyDown(key, &g_double_tap_state);
}

bool KeyLogger_IsRepeat(Keyboard_Keys key) {
	return Keyboard_IsKeyDown(key, &g_repeat_state);
}

bool KeyLogger_IsLongPress(Keyboard_Keys key) {
	return Keyboard_IsKeyDown(key, &g_longpress_state);
}

double KeyLogger_GetHoldTime(Keyboard_Keys key) {
	return g_hold_time[(int)key];
}