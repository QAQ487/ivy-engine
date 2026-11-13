#!/usr/bin/env python3
"""Fail if tools/*.cmd echo lines would redirect via an unescaped '>' arrow.

cmd.exe treats `echo ==> foo` as redirecting stdout to a file named `=`.
Require `echo ==^> ...` (or no bare `>` after echo) instead.
"""
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
TOOLS = ROOT / "tools"

# Match echo lines that contain `>` not preceded by `^` (cmd escape).
# Ignores @echo off and lines that only use already-escaped ^>.
UNSAFE = re.compile(
    r"(?i)^\s*echo\s+.*(?<!\^)>",
)


def main() -> int:
    failures: list[str] = []
    for path in sorted(TOOLS.glob("*.cmd")):
        text = path.read_text(encoding="utf-8", errors="replace")
        for i, line in enumerate(text.splitlines(), 1):
            stripped = line.lstrip()
            if stripped.lower().startswith("@echo"):
                continue
            if not stripped.lower().startswith("echo"):
                continue
            if UNSAFE.search(line):
                failures.append(f"{path.relative_to(ROOT)}:{i}: {line.rstrip()}")
    if failures:
        print("FAIL tools/*.cmd has unescaped echo redirect (`>` must be `^>`):")
        for f in failures:
            print(" -", f)
        return 1
    print("PASS tools/*.cmd echo lines have no unescaped `>` redirects")
    return 0


if __name__ == "__main__":
    sys.exit(main())
