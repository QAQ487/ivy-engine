// key_names.h
// Keyboard key names shared by the runtime test bridge and input action
// bindings ("Key.SPACE"). Values are the Windows virtual-key codes, equal to
// the Keyboard_Keys (KK_*) enum in keyboard.h; the trailing comment names the
// enum value. Win keys are left out on purpose.
// Keep in sync with tools/runtime_bridge_keyboard_keys.py KEYBOARD_KEYS
// (checked by tools/runtime_bridge_keyboard_keys_check.py, which also checks
// each value against keyboard.h). No Windows dependency.

#pragma once

#include <cstddef>
#include <string_view>

namespace key_names {

struct Entry {
    const char* name;
    unsigned char vk;
};

inline constexpr Entry kKeyNames[] = {
    {"BACK", 0x08},                  // KK_BACK
    {"TAB", 0x09},                   // KK_TAB
    {"ENTER", 0x0D},                 // KK_ENTER
    {"PAUSE", 0x13},                 // KK_PAUSE
    {"CAPSLOCK", 0x14},              // KK_CAPSLOCK
    {"KANA", 0x15},                  // KK_KANA
    {"KANJI", 0x19},                 // KK_KANJI
    {"ESCAPE", 0x1B},                // KK_ESCAPE
    {"IMECONVERT", 0x1C},            // KK_IMECONVERT
    {"IMENOCONVERT", 0x1D},          // KK_IMENOCONVERT
    {"SPACE", 0x20},                 // KK_SPACE
    {"PAGEUP", 0x21},                // KK_PAGEUP
    {"PAGEDOWN", 0x22},              // KK_PAGEDOWN
    {"END", 0x23},                   // KK_END
    {"HOME", 0x24},                  // KK_HOME
    {"LEFT", 0x25},                  // KK_LEFT
    {"UP", 0x26},                    // KK_UP
    {"RIGHT", 0x27},                 // KK_RIGHT
    {"DOWN", 0x28},                  // KK_DOWN
    {"SELECT", 0x29},                // KK_SELECT
    {"PRINT", 0x2A},                 // KK_PRINT
    {"EXECUTE", 0x2B},               // KK_EXECUTE
    {"PRINTSCREEN", 0x2C},           // KK_PRINTSCREEN
    {"INSERT", 0x2D},                // KK_INSERT
    {"DELETE", 0x2E},                // KK_DELETE
    {"HELP", 0x2F},                  // KK_HELP
    {"0", 0x30},                     // KK_D0
    {"1", 0x31},                     // KK_D1
    {"2", 0x32},                     // KK_D2
    {"3", 0x33},                     // KK_D3
    {"4", 0x34},                     // KK_D4
    {"5", 0x35},                     // KK_D5
    {"6", 0x36},                     // KK_D6
    {"7", 0x37},                     // KK_D7
    {"8", 0x38},                     // KK_D8
    {"9", 0x39},                     // KK_D9
    {"A", 0x41},                     // KK_A
    {"B", 0x42},                     // KK_B
    {"C", 0x43},                     // KK_C
    {"D", 0x44},                     // KK_D
    {"E", 0x45},                     // KK_E
    {"F", 0x46},                     // KK_F
    {"G", 0x47},                     // KK_G
    {"H", 0x48},                     // KK_H
    {"I", 0x49},                     // KK_I
    {"J", 0x4A},                     // KK_J
    {"K", 0x4B},                     // KK_K
    {"L", 0x4C},                     // KK_L
    {"M", 0x4D},                     // KK_M
    {"N", 0x4E},                     // KK_N
    {"O", 0x4F},                     // KK_O
    {"P", 0x50},                     // KK_P
    {"Q", 0x51},                     // KK_Q
    {"R", 0x52},                     // KK_R
    {"S", 0x53},                     // KK_S
    {"T", 0x54},                     // KK_T
    {"U", 0x55},                     // KK_U
    {"V", 0x56},                     // KK_V
    {"W", 0x57},                     // KK_W
    {"X", 0x58},                     // KK_X
    {"Y", 0x59},                     // KK_Y
    {"Z", 0x5A},                     // KK_Z
    {"APPS", 0x5D},                  // KK_APPS
    {"SLEEP", 0x5F},                 // KK_SLEEP
    {"NUMPAD0", 0x60},               // KK_NUMPAD0
    {"NUMPAD1", 0x61},               // KK_NUMPAD1
    {"NUMPAD2", 0x62},               // KK_NUMPAD2
    {"NUMPAD3", 0x63},               // KK_NUMPAD3
    {"NUMPAD4", 0x64},               // KK_NUMPAD4
    {"NUMPAD5", 0x65},               // KK_NUMPAD5
    {"NUMPAD6", 0x66},               // KK_NUMPAD6
    {"NUMPAD7", 0x67},               // KK_NUMPAD7
    {"NUMPAD8", 0x68},               // KK_NUMPAD8
    {"NUMPAD9", 0x69},               // KK_NUMPAD9
    {"MULTIPLY", 0x6A},              // KK_MULTIPLY
    {"ADD", 0x6B},                   // KK_ADD
    {"SEPARATOR", 0x6C},             // KK_SEPARATOR
    {"SUBTRACT", 0x6D},              // KK_SUBTRACT
    {"DECIMAL", 0x6E},               // KK_DECIMAL
    {"DIVIDE", 0x6F},                // KK_DIVIDE
    {"F1", 0x70},                    // KK_F1
    {"F2", 0x71},                    // KK_F2
    {"F3", 0x72},                    // KK_F3
    {"F4", 0x73},                    // KK_F4
    {"F5", 0x74},                    // KK_F5
    {"F6", 0x75},                    // KK_F6
    {"F7", 0x76},                    // KK_F7
    {"F8", 0x77},                    // KK_F8
    {"F9", 0x78},                    // KK_F9
    {"F10", 0x79},                   // KK_F10
    {"F11", 0x7A},                   // KK_F11
    {"F12", 0x7B},                   // KK_F12
    {"F13", 0x7C},                   // KK_F13
    {"F14", 0x7D},                   // KK_F14
    {"F15", 0x7E},                   // KK_F15
    {"F16", 0x7F},                   // KK_F16
    {"F17", 0x80},                   // KK_F17
    {"F18", 0x81},                   // KK_F18
    {"F19", 0x82},                   // KK_F19
    {"F20", 0x83},                   // KK_F20
    {"F21", 0x84},                   // KK_F21
    {"F22", 0x85},                   // KK_F22
    {"F23", 0x86},                   // KK_F23
    {"F24", 0x87},                   // KK_F24
    {"NUMLOCK", 0x90},               // KK_NUMLOCK
    {"SCROLL", 0x91},                // KK_SCROLL
    {"LSHIFT", 0xA0},                // KK_LEFTSHIFT
    {"RSHIFT", 0xA1},                // KK_RIGHTSHIFT
    {"LCTRL", 0xA2},                 // KK_LEFTCONTROL
    {"RCTRL", 0xA3},                 // KK_RIGHTCONTROL
    {"LALT", 0xA4},                  // KK_LEFTALT
    {"RALT", 0xA5},                  // KK_RIGHTALT
    {"BROWSERBACK", 0xA6},           // KK_BROWSERBACK
    {"BROWSERFORWARD", 0xA7},        // KK_BROWSERFORWARD
    {"BROWSERREFRESH", 0xA8},        // KK_BROWSERREFRESH
    {"BROWSERSTOP", 0xA9},           // KK_BROWSERSTOP
    {"BROWSERSEARCH", 0xAA},         // KK_BROWSERSEARCH
    {"BROWSERFAVORITES", 0xAB},      // KK_BROWSERFAVORITES
    {"BROWSERHOME", 0xAC},           // KK_BROWSERHOME
    {"VOLUMEMUTE", 0xAD},            // KK_VOLUMEMUTE
    {"VOLUMEDOWN", 0xAE},            // KK_VOLUMEDOWN
    {"VOLUMEUP", 0xAF},              // KK_VOLUMEUP
    {"MEDIANEXTTRACK", 0xB0},        // KK_MEDIANEXTTRACK
    {"MEDIAPREVIOUSTRACK", 0xB1},    // KK_MEDIAPREVIOUSTRACK
    {"MEDIASTOP", 0xB2},             // KK_MEDIASTOP
    {"MEDIAPLAYPAUSE", 0xB3},        // KK_MEDIAPLAYPAUSE
    {"LAUNCHMAIL", 0xB4},            // KK_LAUNCHMAIL
    {"SELECTMEDIA", 0xB5},           // KK_SELECTMEDIA
    {"LAUNCHAPPLICATION1", 0xB6},    // KK_LAUNCHAPPLICATION1
    {"LAUNCHAPPLICATION2", 0xB7},    // KK_LAUNCHAPPLICATION2
    {"OEMSEMICOLON", 0xBA},          // KK_OEMSEMICOLON
    {"OEMPLUS", 0xBB},               // KK_OEMPLUS
    {"OEMCOMMA", 0xBC},              // KK_OEMCOMMA
    {"OEMMINUS", 0xBD},              // KK_OEMMINUS
    {"OEMPERIOD", 0xBE},             // KK_OEMPERIOD
    {"OEMQUESTION", 0xBF},           // KK_OEMQUESTION
    {"OEMTILDE", 0xC0},              // KK_OEMTILDE
    {"OEMOPENBRACKETS", 0xDB},       // KK_OEMOPENBRACKETS
    {"OEMPIPE", 0xDC},               // KK_OEMPIPE
    {"OEMCLOSEBRACKETS", 0xDD},      // KK_OEMCLOSEBRACKETS
    {"OEMQUOTES", 0xDE},             // KK_OEMQUOTES
    {"OEM8", 0xDF},                  // KK_OEM8
    {"OEMBACKSLASH", 0xE2},          // KK_OEMBACKSLASH
    {"PROCESSKEY", 0xE5},            // KK_PROCESSKEY
    {"OEMCOPY", 0xF2},               // KK_OEMCOPY
    {"OEMAUTO", 0xF3},               // KK_OEMAUTO
    {"OEMENLW", 0xF4},               // KK_OEMENLW
    {"ATTN", 0xF6},                  // KK_ATTN
    {"CRSEL", 0xF7},                 // KK_CRSEL
    {"EXSEL", 0xF8},                 // KK_EXSEL
    {"ERASEEOF", 0xF9},              // KK_ERASEEOF
    {"PLAY", 0xFA},                  // KK_PLAY
    {"ZOOM", 0xFB},                  // KK_ZOOM
    {"PA1", 0xFD},                   // KK_PA1
    {"OEMCLEAR", 0xFE},              // KK_OEMCLEAR
};

// Virtual-key code for a name ("SPACE" -> 0x20); false for an unknown name.
inline bool FromName(std::string_view name, unsigned char* outVk) {
    for (const Entry& e : kKeyNames) {
        if (name == e.name) {
            if (outVk) *outVk = e.vk;
            return true;
        }
    }
    return false;
}

// Name for a virtual-key code; nullptr when the key has no name in the table.
inline const char* ToName(unsigned char vk) {
    for (const Entry& e : kKeyNames) {
        if (e.vk == vk)
            return e.name;
    }
    return nullptr;
}

}  // namespace key_names
