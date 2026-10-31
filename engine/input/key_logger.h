/**===========================================================================
 File   : key_logger.h
 Title  : Input logging
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
bool KeyLogger_IsPressed(Keyboard_Keys key);     // True while the key is held down
bool KeyLogger_IsTrigger(Keyboard_Keys key);     // True only on the frame the key is pressed
bool KeyLogger_IsRelease(Keyboard_Keys key);     // True only on the frame the key is released
bool KeyLogger_IsDoubleTap(Keyboard_Keys key);   // True on a quick double-tap (e.g. within 0.3s)
bool KeyLogger_IsRepeat(Keyboard_Keys key);      // True periodically while held (e.g. every 0.25s)
bool KeyLogger_IsLongPress(Keyboard_Keys key);   // True if held for a threshold (e.g. 1s+)
double KeyLogger_GetHoldTime(Keyboard_Keys key); // How long the key has been held (seconds)

#endif // !KEY_LOGGER_H
