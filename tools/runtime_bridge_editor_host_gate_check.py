#!/usr/bin/env python3
"""Fail closed if Editor Debug bridge gates drift apart.

Catches the PR #29 class of bug: call sites / .cpp bodies under
IVY_RUNTIME_TEST_BRIDGE_AVAILABLE while declarations or required includes
remain gated on _DEBUG && GAME_RUNTIME.
"""
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]

# (path, must_contain_pattern, must_not_contain_pattern, label)
RULES = [
    (
        "src/app/runtime_test_world_apply.h",
        r"#if\s+defined\(IVY_RUNTIME_TEST_BRIDGE_AVAILABLE\)",
        r"#if\s+defined\(_DEBUG\)\s*&&\s*defined\(GAME_RUNTIME\)",
        "world_apply.h must declare under IVY_RUNTIME_TEST_BRIDGE_AVAILABLE (not GAME_RUNTIME-only)",
    ),
    (
        "src/app/runtime_test_world_apply.cpp",
        r"#if\s+defined\(IVY_RUNTIME_TEST_BRIDGE_AVAILABLE\)",
        None,
        "world_apply.cpp body must stay under IVY_RUNTIME_TEST_BRIDGE_AVAILABLE",
    ),
    (
        "src/app/runtime_test_editor_apply.h",
        r"#if\s+defined\(IVY_RUNTIME_TEST_BRIDGE_AVAILABLE\)",
        r"#if\s+defined\(_DEBUG\)\s*&&\s*defined\(GAME_RUNTIME\)",
        "editor_apply.h must declare under IVY_RUNTIME_TEST_BRIDGE_AVAILABLE",
    ),
    (
        "src/app/main.cpp",
        r"#include\s*<shellapi\.h>",
        None,
        "main.cpp must include <shellapi.h> for CommandLineToArgvW",
    ),
    (
        "tools/runtime_bridge_editor_host_compile_check.cpp",
        r"#include\s*<windows\.h>[\s\S]*?#include\s*<shellapi\.h>",
        None,
        "editor host compile check must include <windows.h> before <shellapi.h> (MSVC)",
    ),
]


def main() -> int:
    failures = []
    main_cpp = (ROOT / "src/app/main.cpp").read_text(encoding="utf-8", errors="replace")
    # CommandLineToArgvW must only appear when shellapi is included under the bridge gate.
    if "CommandLineToArgvW" in main_cpp and "#include <shellapi.h>" not in main_cpp:
        failures.append("main.cpp uses CommandLineToArgvW without #include <shellapi.h>")

    # SetWorldOpHandler(&RuntimeTestWorld_Apply) must see a matching declaration gate.
    if "RuntimeTestWorld_Apply" in main_cpp:
        world_h = (ROOT / "src/app/runtime_test_world_apply.h").read_text(
            encoding="utf-8", errors="replace")
        if "IVY_RUNTIME_TEST_BRIDGE_AVAILABLE" not in world_h:
            failures.append(
                "main.cpp references RuntimeTestWorld_Apply but world_apply.h "
                "does not gate on IVY_RUNTIME_TEST_BRIDGE_AVAILABLE")
        if re.search(r"#if\s+defined\(_DEBUG\)\s*&&\s*defined\(GAME_RUNTIME\)", world_h):
            failures.append(
                "world_apply.h still uses _DEBUG && GAME_RUNTIME gate "
                "(Editor Debug would not see RuntimeTestWorld_Apply)")

    for rel, must, must_not, label in RULES:
        text = (ROOT / rel).read_text(encoding="utf-8", errors="replace")
        if must and not re.search(must, text):
            failures.append(f"{label}: missing /{must}/ in {rel}")
        if must_not and re.search(must_not, text):
            failures.append(f"{label}: forbidden /{must_not}/ still in {rel}")

    # shellapi include should sit inside the bridge-available region of main.cpp
    # (Release Editor must compile the include out with the bridge).
    bridge_blocks = re.findall(
        r"#if\s+defined\(IVY_RUNTIME_TEST_BRIDGE_AVAILABLE\)(.*?)#endif",
        main_cpp,
        flags=re.S,
    )
    if not any("#include <shellapi.h>" in block for block in bridge_blocks):
        # Also allow include immediately after the gate open on nearby lines.
        idx = main_cpp.find("#include <shellapi.h>")
        if idx < 0:
            failures.append("main.cpp missing shellapi include")
        else:
            window = main_cpp[max(0, idx - 400) : idx]
            if "IVY_RUNTIME_TEST_BRIDGE_AVAILABLE" not in window:
                failures.append(
                    "main.cpp <shellapi.h> should be near IVY_RUNTIME_TEST_BRIDGE_AVAILABLE "
                    "so Release Editor compiles it out")

    # Editor bridge quit must DestroyWindow, not RequestExitGame (ReturnToStart).
    if "EngineGUI_RequestBridgeHostQuit" not in main_cpp:
        failures.append(
            "main.cpp must call EngineGUI_RequestBridgeHostQuit for Editor bridge quit")
    if not re.search(
            r"#ifndef\s+GAME_RUNTIME[\s\S]*?EngineGUI_RequestBridgeHostQuit",
            main_cpp):
        failures.append(
            "Editor bridge quit (RequestBridgeHostQuit) must be under #ifndef GAME_RUNTIME")
    if not re.search(r"case\s+WM_APP\s*\+\s*48\s*:", main_cpp):
        failures.append("main.cpp must handle WM_APP+48 DestroyWindow for bridge quit")
    # The WndProc case must not be GAME_RUNTIME-only (Editor bridge quit needs it).
    if re.search(
            r"#ifdef\s+GAME_RUNTIME\s*\n\s*//[^\n]*\n\s*case\s+WM_APP\s*\+\s*48\s*:",
            main_cpp) or re.search(
            r"#ifdef\s+GAME_RUNTIME\s*\n\s*case\s+WM_APP\s*\+\s*48\s*:",
            main_cpp):
        failures.append(
            "WM_APP+48 DestroyWindow case must not be inside #ifdef GAME_RUNTIME "
            "(Editor bridge quit needs it)")

    if failures:
        print("FAIL Editor host bridge gate check:")
        for f in failures:
            print(" -", f)
        return 1
    print("PASS Editor host bridge gate check "
          "(world/editor apply + shellapi + Editor quit DestroyWindow)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
