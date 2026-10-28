/**===========================================================================
 File   : key_logger.h
 Title  : 入力の記録
 Author : Terry Yeung
 Date   : 2025/6/27
 Brief  :
 ===========================================================================**/
#ifndef KEY_LOGGER_H
#define KEY_LOGGER_H
#include "keyboard.h"

void KeyLogger_Initialize();
void KeyLogger_Update(double now_time);

static void Keyboard_SetKeyDown(Keyboard_State* state, Keyboard_Keys key, bool down);
bool KeyLogger_IsPressed(Keyboard_Keys key);     // キーが現在押されているかどうか（押し続けている間 true）
bool KeyLogger_IsTrigger(Keyboard_Keys key);     // キーが今フレームで押された瞬間か（1フレームだけ true）
bool KeyLogger_IsRelease(Keyboard_Keys key);     // キーが今フレームで離された瞬間か（1フレームだけ true）
bool KeyLogger_IsDoubleTap(Keyboard_Keys key);   // キーが素早く2回押されたか（ダブルタップ、例：0.3秒以内）
bool KeyLogger_IsRepeat(Keyboard_Keys key);      // キーが押し続けられていて一定時間ごとに true を返す（例：0.25秒ごと）
bool KeyLogger_IsLongPress(Keyboard_Keys key);   // キーが一定時間以上（例：1秒以上）押されているかどうか
double KeyLogger_GetHoldTime(Keyboard_Keys key); // キーが現在どれくらいの時間押され続けているか（秒数）

#endif // !KEY_LOGGER_H
