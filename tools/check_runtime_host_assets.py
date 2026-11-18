#!/usr/bin/env python3
"""Host runtime assets must exist under resources/ and ship via OutDir mirror.

The runtime opens these even when no scene names them (fade, debug atlas, UI
font). A clean build mirrors resources/ beside the exe; loaders resolve
rom/<rest> via project rom then <exe-dir>/resources/<rest>. DebugText must use
that fallback (CreateWICTextureFromFile does not on its own).
"""

import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
RESOURCES = REPO / "resources"

# Keep in sync with tools/materialize_engine_defaults.py HOST_ROM_PATHS.
HOST_ROM_PATHS = (
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

    # Built-in textures: the C++ table, the package builder and the Python mirror must agree,
    # and each file must still exist in resources/ (the editor reads it from there).
    header = (REPO / "engine" / "core" / "builtin_assets.h").read_text(encoding="utf-8", errors="replace")
    builder = (REPO / "engine" / "editor" / "playable_package_builder.cpp").read_text(
        encoding="utf-8", errors="replace")
    cpp_paths = set(re.findall(r'\{"(rom/[^"]+)",\s*k', header))
    builder_block = re.search(r"kBuiltinRomPaths\[\]\s*=\s*\{(.*?)\};", builder, re.S)
    builder_paths = set(re.findall(r'"(rom/[^"]+)"', builder_block.group(1))) if builder_block else set()
    py_block = re.search(r"BUILTIN_ROM_PATHS\s*=\s*\((.*?)\)", materialize, re.S)
    py_paths = set(re.findall(r'"(rom/[^"]+)"', py_block.group(1))) if py_block else set()
    if not cpp_paths:
        errors.append("engine/core/builtin_assets.h lists no built-in texture paths")
    if cpp_paths != builder_paths:
        errors.append(f"builtin_assets.h {sorted(cpp_paths)} != playable_package_builder.cpp {sorted(builder_paths)}")
    if cpp_paths != py_paths:
        errors.append(f"builtin_assets.h {sorted(cpp_paths)} != materialize_engine_defaults.py {sorted(py_paths)}")
    for rom_path in sorted(cpp_paths):
        if not (RESOURCES / rom_path[4:]).is_file():
            errors.append(f"built-in texture source missing in resources/: {rom_path}")

    # Debug text and the fade fill need no shipped files: the font is compiled in.
    debug_text = DEBUG_TEXT.read_text(encoding="utf-8", errors="replace")
    if "UIFont_GetBuiltinMono" not in debug_text:
        errors.append("engine/debug_draw/debug_text.cpp must use UIFont_GetBuiltinMono (built-in font)")
    if "consolab" in debug_text.lower():
        errors.append("engine/debug_draw/debug_text.cpp must not name the retired bitmap atlas")
    embed = subprocess.run(
        [sys.executable, str(REPO / "tools" / "embed_builtin_assets.py"), "--check"],
        capture_output=True, text=True)
    if embed.returncode != 0:
        errors.append(embed.stderr.strip() or "engine/core/builtin_assets_data.h is stale")

    if errors:
        print("FAIL: runtime host asset coverage:", file=sys.stderr)
        for item in errors:
            print(f"  {item}", file=sys.stderr)
        return 1

    print(
        f"PASS: {len(HOST_ROM_PATHS)} host assets in resources/, "
        f"vcxproj OutDir mirror, DebugText uses the built-in font"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
