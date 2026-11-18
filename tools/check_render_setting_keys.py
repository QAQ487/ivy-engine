#!/usr/bin/env python3
"""Check that the runtime render-setting name tables agree.

Sources:
  engine/scene/render_setting_access.h      bool macro + numeric strcmp keys (+ lodForceLevel,
                                            handled in render_settings.cpp)
  engine/testing/runtime_test_bridge.cpp    IsRuntimeTestBridge{,Numeric}SettingKey
  tools/runtime_test_client.py              SETTING_KEYS / NUMERIC_SETTING_KEYS
  engine/scene/render_settings.h            documented key list comment
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def read(rel):
    return (ROOT / rel).read_text(encoding="utf-8")


def access_keys():
    text = read("engine/scene/render_setting_access.h")
    bools = set(re.findall(r"^\s*X\((\w+)\)", text, re.M))
    numbers = set(re.findall(r'strcmp\(key, "(\w+)"\) == 0', text.split("bool SetNumber")[1].split("bool GetNumber")[0]))
    numbers.add("lodForceLevel")  # special-cased in render_settings.cpp (Lod_SetForceLevel)
    return bools, numbers


def bridge_keys():
    text = read("engine/testing/runtime_test_bridge.cpp")
    def body(fn):
        m = re.search(fn + r"\(const std::string& key\) \{(.*?)\n\}", text, re.S)
        return set(re.findall(r'key == "(\w+)"', m.group(1)))
    return body("bool IsRuntimeTestBridgeSettingKey"), body("bool IsRuntimeTestBridgeNumericSettingKey")


def client_keys():
    text = read("tools/runtime_test_client.py")
    def tup(name):
        m = re.search(name + r" = \((.*?)\n\)", text, re.S)
        return set(re.findall(r'"(\w+)"', m.group(1)))
    return tup("SETTING_KEYS"), tup("NUMERIC_SETTING_KEYS")


def main():
    a_b, a_n = access_keys()
    b_b, b_n = bridge_keys()
    c_b, c_n = client_keys()
    ok = True
    for label, ref, other in (("bridge bool", a_b, b_b), ("client bool", a_b, c_b),
                              ("bridge numeric", a_n, b_n), ("client numeric", a_n, c_n)):
        if ref != other:
            ok = False
            print(f"FAIL {label}: only in access header {sorted(ref - other)}, only in {label} {sorted(other - ref)}")
    header = read("engine/scene/render_settings.h")
    for key in sorted(a_b | a_n):
        if key not in header:
            ok = False
            print(f"FAIL render_settings.h comment does not mention {key}")
    if not ok:
        return 1
    print(f"PASS render setting keys agree ({len(a_b)} toggles, {len(a_n)} numbers)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
