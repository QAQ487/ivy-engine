#!/usr/bin/env python3
"""Linux-runnable check: Python KEYBOARD_KEYS must match C++ key_names.h kKeyNames exactly."""
from __future__ import annotations

import pathlib
import re
import sys
import unittest

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

from runtime_bridge_keyboard_keys import (  # noqa: E402
    KEYBOARD_KEYS,
    is_forbidden_keyboard_combo,
    validate_keyboard_keys,
)

KEY_NAMES_H = ROOT / "engine" / "input" / "key_names.h"
KEYBOARD_H = ROOT / "engine" / "input" / "keyboard.h"
# Matches: {"NAME", 0x20},  // KK_SOMETHING   inside kKeyNames.
_ENTRY_RE = re.compile(
    r'\{\s*"([A-Z0-9]+)"\s*,\s*(0x[0-9A-Fa-f]+)\s*\}\s*,\s*//\s*(KK_[A-Z0-9]+)'
)
_TABLE_RE = re.compile(
    r"kKeyNames\s*\[\s*\]\s*=\s*\{(.*?)\}\s*;",
    re.S,
)
_KK_RE = re.compile(r"\b(KK_[A-Z0-9]+)\s*=\s*(0x[0-9A-Fa-f]+)")


def parse_cpp_key_entries(text: str) -> list[tuple[str, int, str]]:
    match = _TABLE_RE.search(text)
    if not match:
        raise AssertionError("kKeyNames not found in key_names.h")
    entries = []
    seen = set()
    for name, value, kk in _ENTRY_RE.findall(match.group(1)):
        if name in seen:
            raise AssertionError("duplicate C++ key name: %s" % name)
        seen.add(name)
        entries.append((name, int(value, 16), kk))
    if not entries:
        raise AssertionError("kKeyNames is empty")
    return entries


def parse_cpp_bridge_key_names(text: str) -> list[str]:
    return [name for name, _value, _kk in parse_cpp_key_entries(text)]


class KeyTableSyncTests(unittest.TestCase):
    def test_python_matches_cpp_table(self):
        cpp_names = parse_cpp_bridge_key_names(KEY_NAMES_H.read_text(encoding="utf-8"))
        self.assertEqual(set(cpp_names), KEYBOARD_KEYS)
        self.assertEqual(len(cpp_names), len(KEYBOARD_KEYS))
        self.assertNotIn("LEFTWINDOWS", KEYBOARD_KEYS)
        self.assertNotIn("RIGHTWINDOWS", KEYBOARD_KEYS)
        self.assertNotIn("NONE", KEYBOARD_KEYS)

    def test_values_match_keyboard_enum(self):
        enum = {kk: int(v, 16) for kk, v in _KK_RE.findall(KEYBOARD_H.read_text(encoding="utf-8"))}
        entries = parse_cpp_key_entries(KEY_NAMES_H.read_text(encoding="utf-8"))
        self.assertEqual(len(entries), len(_ENTRY_RE.findall(_TABLE_RE.search(
            KEY_NAMES_H.read_text(encoding="utf-8")).group(1))))
        vks = set()
        for name, value, kk in entries:
            with self.subTest(name=name):
                self.assertIn(kk, enum)
                self.assertEqual(value, enum[kk])
                self.assertNotIn(value, vks, "two names share one key code")
                vks.add(value)

    def test_cpp_excludes_win_keys_and_none(self):
        text = KEY_NAMES_H.read_text(encoding="utf-8")
        body = _TABLE_RE.search(text).group(1)
        self.assertNotIn("LEFTWINDOWS", body)
        self.assertNotIn("RIGHTWINDOWS", body)
        self.assertNotIn("KK_NONE", body)


class KeyValidationTests(unittest.TestCase):
    def test_accepts_expanded_and_modifier_combos(self):
        for keys in (
            ["ESCAPE"],
            ["ENTER", "TAB"],
            ["E", "T", "1", "4"],
            ["LCTRL", "E"],
            ["LSHIFT", "TAB"],
            ["LALT", "TAB"],
            ["W", "A", "S", "D", "SPACE", "UP", "DOWN", "LEFT", "RIGHT"],
        ):
            with self.subTest(keys=keys):
                self.assertEqual(validate_keyboard_keys(keys), list(keys))

    def test_rejects_unknown_and_win_names(self):
        for keys in (
            ["NOTAKEY"],
            ["LEFTWINDOWS"],
            ["RIGHTWINDOWS"],
            ["WIN"],
            ["ESCAPE", "NOTAKEY"],
        ):
            with self.subTest(keys=keys):
                with self.assertRaises(ValueError):
                    validate_keyboard_keys(keys)

    def test_rejects_alt_f4_combo(self):
        self.assertTrue(is_forbidden_keyboard_combo(["LALT", "F4"]))
        self.assertTrue(is_forbidden_keyboard_combo(["RALT", "F4", "E"]))
        self.assertFalse(is_forbidden_keyboard_combo(["F4"]))
        self.assertFalse(is_forbidden_keyboard_combo(["LALT"]))
        for keys in (["LALT", "F4"], ["RALT", "F4"], ["F4", "LALT", "E"]):
            with self.subTest(keys=keys):
                with self.assertRaises(ValueError):
                    validate_keyboard_keys(keys)

    def test_rejects_duplicates_empty_and_oversize(self):
        with self.assertRaises(ValueError):
            validate_keyboard_keys([])
        with self.assertRaises(ValueError):
            validate_keyboard_keys(["W", "W"])
        with self.assertRaises(ValueError):
            validate_keyboard_keys(["A", "B", "C", "D", "E", "F", "G", "H", "I", "J"])


if __name__ == "__main__":
    unittest.main()
