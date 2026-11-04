"""Bridge keyboard key-name table shared by the Python client and scenario schema.

Names map 1:1 to engine Keyboard_Keys (KK_*) via the C++ kBridgeKeyTable in
engine/testing/runtime_test_bridge.cpp. Digits use "0".."9"; modifiers use
LCTRL/RCTRL/LSHIFT/RSHIFT/LALT/RALT. Win keys are omitted; ALT+F4 is rejected.
"""
from __future__ import annotations

# Keep in sync with kBridgeKeyTable in engine/testing/runtime_test_bridge.cpp.
# tools/runtime_bridge_keyboard_keys_check.py parses both and fails on mismatch.
KEYBOARD_KEYS = frozenset({
    "BACK", "TAB", "ENTER", "PAUSE", "CAPSLOCK", "KANA", "KANJI", "ESCAPE",
    "IMECONVERT", "IMENOCONVERT", "SPACE", "PAGEUP", "PAGEDOWN", "END", "HOME", "LEFT",
    "UP", "RIGHT", "DOWN", "SELECT", "PRINT", "EXECUTE", "PRINTSCREEN", "INSERT",
    "DELETE", "HELP", "0", "1", "2", "3", "4", "5",
    "6", "7", "8", "9", "A", "B", "C", "D",
    "E", "F", "G", "H", "I", "J", "K", "L",
    "M", "N", "O", "P", "Q", "R", "S", "T",
    "U", "V", "W", "X", "Y", "Z", "APPS", "SLEEP",
    "NUMPAD0", "NUMPAD1", "NUMPAD2", "NUMPAD3", "NUMPAD4", "NUMPAD5", "NUMPAD6", "NUMPAD7",
    "NUMPAD8", "NUMPAD9", "MULTIPLY", "ADD", "SEPARATOR", "SUBTRACT", "DECIMAL", "DIVIDE",
    "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8",
    "F9", "F10", "F11", "F12", "F13", "F14", "F15", "F16",
    "F17", "F18", "F19", "F20", "F21", "F22", "F23", "F24",
    "NUMLOCK", "SCROLL", "LSHIFT", "RSHIFT", "LCTRL", "RCTRL", "LALT", "RALT",
    "BROWSERBACK", "BROWSERFORWARD", "BROWSERREFRESH", "BROWSERSTOP", "BROWSERSEARCH",
    "BROWSERFAVORITES", "BROWSERHOME", "VOLUMEMUTE", "VOLUMEDOWN", "VOLUMEUP",
    "MEDIANEXTTRACK", "MEDIAPREVIOUSTRACK", "MEDIASTOP", "MEDIAPLAYPAUSE",
    "LAUNCHMAIL", "SELECTMEDIA", "LAUNCHAPPLICATION1", "LAUNCHAPPLICATION2",
    "OEMSEMICOLON", "OEMPLUS", "OEMCOMMA", "OEMMINUS", "OEMPERIOD", "OEMQUESTION",
    "OEMTILDE", "OEMOPENBRACKETS", "OEMPIPE", "OEMCLOSEBRACKETS", "OEMQUOTES", "OEM8",
    "OEMBACKSLASH", "PROCESSKEY", "OEMCOPY", "OEMAUTO", "OEMENLW",
    "ATTN", "CRSEL", "EXSEL", "ERASEEOF", "PLAY", "ZOOM", "PA1", "OEMCLEAR",
})

_ALT_NAMES = frozenset({"LALT", "RALT"})
MAX_KEYBOARD_KEYS = 9


def is_forbidden_keyboard_combo(keys) -> bool:
    """True when a request would inject ALT together with F4."""
    names = set(keys)
    return "F4" in names and bool(names & _ALT_NAMES)


def validate_keyboard_keys(keys):
    """Validate bridge keyboard `keys` payload; raise ValueError on rejection."""
    if not isinstance(keys, (list, tuple)) or not 1 <= len(keys) <= MAX_KEYBOARD_KEYS:
        raise ValueError("keys must be unique whitelist names")
    if any(not isinstance(k, str) or k not in KEYBOARD_KEYS for k in keys):
        raise ValueError("keys must be unique whitelist names")
    if len(set(keys)) != len(keys):
        raise ValueError("keys must be unique whitelist names")
    if is_forbidden_keyboard_combo(keys):
        raise ValueError("keys must be unique whitelist names")
    return list(keys)
