#!/usr/bin/env python3
"""Linux-runnable check: Python KEYBOARD_KEYS must match C++ kBridgeKeyTable exactly."""
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

BRIDGE_CPP = ROOT / "engine" / "testing" / "runtime_test_bridge.cpp"
# Matches: {"NAME", KK_SOMETHING}, inside kBridgeKeyTable.
_ENTRY_RE = re.compile(
    r'\{\s*"([A-Z0-9]+)"\s*,\s*(KK_[A-Z0-9]+)\s*\}'
)
_TABLE_RE = re.compile(
    r"kBridgeKeyTable\s*\[\s*\]\s*=\s*\{(.*?)\}\s*;",
    re.S,
)


def parse_cpp_bridge_key_names(text: str) -> list[str]:
    match = _TABLE_RE.search(text)
    if not match:
        raise AssertionError("kBridgeKeyTable not found in runtime_test_bridge.cpp")
    names = []
    seen = set()
    for name, _kk in _ENTRY_RE.findall(match.group(1)):
        if name in seen:
            raise AssertionError("duplicate C++ bridge key name: %s" % name)
        seen.add(name)
        names.append(name)
    if not names:
        raise AssertionError("kBridgeKeyTable is empty")
    return names


class KeyTableSyncTests(unittest.TestCase):
    def test_python_matches_cpp_table(self):
        cpp_names = parse_cpp_bridge_key_names(BRIDGE_CPP.read_text(encoding="utf-8"))
        self.assertEqual(set(cpp_names), KEYBOARD_KEYS)
        self.assertEqual(len(cpp_names), len(KEYBOARD_KEYS))
        # Stable documented order in C++ should be unique; set equality is the contract.
        self.assertNotIn("LEFTWINDOWS", KEYBOARD_KEYS)
        self.assertNotIn("RIGHTWINDOWS", KEYBOARD_KEYS)
        self.assertNotIn("NONE", KEYBOARD_KEYS)

    def test_cpp_excludes_win_keys_and_none(self):
        text = BRIDGE_CPP.read_text(encoding="utf-8")
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
