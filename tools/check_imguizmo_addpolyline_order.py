#!/usr/bin/env python3
"""Fail if ImGuizmo AddPolyline calls disagree with vendored ImGui's signature.

Vendored ImGui (1.92.5) declares:
  AddPolyline(const ImVec2* points, int num_points, ImU32 col, ImDrawFlags flags, float thickness)

ImGui >= 1.92.8 swapped the last two parameters to (thickness, flags). ImGuizmo
upstream targets that newer order; against 1.92.5 that makes rotation rings use
thickness 1 and drops closed fan borders.

Usage (repo root):
  python3 tools/check_imguizmo_addpolyline_order.py
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
IMGUI_H = ROOT / "libraries" / "imgui" / "imgui.h"
IMGUIZMO_CPP = ROOT / "libraries" / "imguizmo" / "ImGuizmo.cpp"

# Last two parameters must be flags then thickness for ImGui 1.92.5.
EXPECTED_TAIL = ("ImDrawFlags flags", "float thickness")


def parse_addpolyline_signature(imgui_h: str) -> list[str]:
    match = re.search(
        r"IMGUI_API\s+void\s+AddPolyline\s*\(([^)]*)\)\s*;",
        imgui_h,
    )
    if not match:
        raise SystemExit("FAIL: AddPolyline declaration not found in imgui.h")
    params = [p.strip() for p in match.group(1).split(",") if p.strip()]
    if len(params) < 5:
        raise SystemExit(f"FAIL: unexpected AddPolyline arity: {params!r}")
    return params


def split_args(arg_text: str) -> list[str]:
    """Split a C++ argument list on top-level commas."""
    args: list[str] = []
    depth = 0
    start = 0
    for i, ch in enumerate(arg_text):
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth -= 1
        elif ch == "," and depth == 0:
            args.append(arg_text[start:i].strip())
            start = i + 1
    args.append(arg_text[start:].strip())
    return [a for a in args if a]


def looks_like_thickness(expr: str) -> bool:
    e = expr.strip()
    if re.search(r"Thickness|thickness", e):
        return True
    if re.fullmatch(r"\d+\.\d+f?", e):
        return True
    if re.fullmatch(r"\d+f", e):
        return True
    return False


def looks_like_flags(expr: str) -> bool:
    e = expr.strip()
    if e == "0" or e == "ImDrawFlags_None":
        return True
    if e.startswith("ImDrawFlags_"):
        return True
    return False


def strip_cpp_comments(source: str) -> str:
    """Replace // and /* */ comments with spaces so they cannot fake calls."""
    out: list[str] = []
    i = 0
    n = len(source)
    while i < n:
        if source.startswith("//", i):
            while i < n and source[i] != "\n":
                out.append(" ")
                i += 1
            continue
        if source.startswith("/*", i):
            while i < n and not source.startswith("*/", i):
                out.append("\n" if source[i] == "\n" else " ")
                i += 1
            if i < n:
                out.extend([" ", " "])
                i += 2
            continue
        out.append(source[i])
        i += 1
    return "".join(out)


def find_addpolyline_calls(source: str) -> list[tuple[int, list[str]]]:
    cleaned = strip_cpp_comments(source)
    calls: list[tuple[int, list[str]]] = []
    for match in re.finditer(r"(?<![A-Za-z0-9_])AddPolyline\s*\(", cleaned):
        open_paren = match.end() - 1
        depth = 0
        i = open_paren
        while i < len(cleaned):
            ch = cleaned[i]
            if ch == "(":
                depth += 1
            elif ch == ")":
                depth -= 1
                if depth == 0:
                    arg_text = cleaned[open_paren + 1 : i]
                    line = cleaned.count("\n", 0, match.start()) + 1
                    calls.append((line, split_args(arg_text)))
                    break
            i += 1
    return calls


def main() -> int:
    params = parse_addpolyline_signature(IMGUI_H.read_text(encoding="utf-8", errors="replace"))
    tail = (params[-2], params[-1])
    print(f"imgui.h AddPolyline last params: {tail[0]}, {tail[1]}")
    if (tail[0].replace("  ", " "), tail[1].replace("  ", " ")) != EXPECTED_TAIL:
        # Allow extra qualifiers / spacing differences by token check.
        if "flags" not in params[-2] or "thickness" not in params[-1]:
            print(
                "FAIL: vendored imgui.h AddPolyline is not (flags, thickness); "
                f"got ({params[-2]}, {params[-1]})"
            )
            return 1

    source = IMGUIZMO_CPP.read_text(encoding="utf-8", errors="replace")
    calls = find_addpolyline_calls(source)
    if not calls:
        print("FAIL: no AddPolyline calls found in ImGuizmo.cpp")
        return 1

    failures = 0
    for line, args in calls:
        if len(args) != 5:
            print(f"FAIL: line {line}: expected 5 args, got {len(args)}: {args}")
            failures += 1
            continue
        flags_arg, thickness_arg = args[3], args[4]
        # Correct 1.92.5 order: flags then thickness.
        if looks_like_thickness(flags_arg) and looks_like_flags(thickness_arg):
            print(
                f"FAIL: line {line}: AddPolyline uses (thickness, flags) order "
                f"incompatible with ImGui 1.92.5: flags={flags_arg!r}, "
                f"thickness={thickness_arg!r}"
            )
            failures += 1
            continue
        if not looks_like_flags(flags_arg):
            print(
                f"FAIL: line {line}: 4th arg should be ImDrawFlags, got {flags_arg!r}"
            )
            failures += 1
            continue
        if not looks_like_thickness(thickness_arg) and thickness_arg not in {"1", "1.f", "1.0", "2"}:
            # Thickness may be a float literal without 'f' or a style field.
            if not re.search(r"Thickness|thickness|\d", thickness_arg):
                print(
                    f"FAIL: line {line}: 5th arg should be thickness, got {thickness_arg!r}"
                )
                failures += 1
                continue
        print(f"OK: line {line}: flags={flags_arg}, thickness={thickness_arg}")

    if failures:
        print(f"{failures} AddPolyline call(s) disagree with imgui.h")
        return 1
    print(f"IMGUIZMO_ADDPOLYLINE_ORDER PASS ({len(calls)} call(s))")
    return 0


if __name__ == "__main__":
    sys.exit(main())
