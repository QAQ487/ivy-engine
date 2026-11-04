#!/usr/bin/env python3
"""Host runtime assets must exist under resources/ and ship via OutDir mirror.

The runtime opens these even when no scene names them (fade, debug atlas, UI
font). A clean build mirrors resources/ beside the exe; loaders resolve
rom/<rest> via project rom then <exe-dir>/resources/<rest>. DebugText must use
that fallback (CreateWICTextureFromFile does not on its own).
"""

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
RESOURCES = REPO / "resources"

# Keep in sync with tools/materialize_engine_defaults.py HOST_ROM_PATHS.
HOST_ROM_PATHS = (
    "rom/texture/fade.png",
    "rom/texture/consolab_ascii_512.png",
    "rom/font/LemonSlice.ttf",
)

VCXPROJS = (
    REPO / "IvyEngineRuntime.vcxproj",
    REPO / "IvyEngine.vcxproj",
)

# Post-build must mirror resources -> $(OutDir)resources (both configs).
ROBOCOPY_NEEDLE = re.compile(
    r'robocopy\s+"\$\(ProjectDir\)resources"\s+"\$\(OutDir\)resources"',
    re.IGNORECASE,
)

DEBUG_TEXT = REPO / "engine" / "debug_draw" / "debug_text.cpp"


def main():
    errors = []

    for rom_path in HOST_ROM_PATHS:
        dest = RESOURCES / rom_path[4:]
        if not dest.is_file():
            errors.append(f"missing tracked host asset: {rom_path} -> {dest.relative_to(REPO)}")

    materialize = (REPO / "tools" / "materialize_engine_defaults.py").read_text(
        encoding="utf-8", errors="replace"
    )
    for rom_path in HOST_ROM_PATHS:
        if rom_path not in materialize:
            errors.append(f"materialize_engine_defaults.py missing HOST path: {rom_path}")

    for vcx in VCXPROJS:
        text = vcx.read_text(encoding="utf-8", errors="replace")
        matches = ROBOCOPY_NEEDLE.findall(text)
        if len(matches) < 2:
            errors.append(
                f"{vcx.name}: expected robocopy resources->$(OutDir)resources "
                f"in Debug and Release (found {len(matches)})"
            )

    debug_text = DEBUG_TEXT.read_text(encoding="utf-8", errors="replace")
    if "ReadableWideCurrent" not in debug_text:
        errors.append(
            "engine/debug_draw/debug_text.cpp must resolve font texture via "
            "content_path::ReadableWideCurrent (resources fallback)"
        )
    if "CreateWICTextureFromFile" not in debug_text:
        errors.append("engine/debug_draw/debug_text.cpp missing CreateWICTextureFromFile")

    if errors:
        print("FAIL: runtime host asset coverage:", file=sys.stderr)
        for item in errors:
            print(f"  {item}", file=sys.stderr)
        return 1

    print(
        f"PASS: {len(HOST_ROM_PATHS)} host assets in resources/, "
        f"vcxproj OutDir mirror, DebugText uses content_path fallback"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
