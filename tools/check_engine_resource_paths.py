#!/usr/bin/env python3
"""Every hardcoded rom/ path in C++ must resolve under resources/ or be optional.

Optional paths are content, generated files, or build outputs. They are listed
below so a new engine default cannot land only as a string in C++.

Shader .cso files are gitignored build output. A rom/shader/Name.cso path is
accepted when shaders/Name.hlsl exists.
"""

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
RESOURCES = REPO / "resources"
SCAN_ROOTS = (REPO / "engine", REPO / "src", REPO / "tools")
TOKEN = re.compile(r"rom[/\\][^\s\"'<>|*?]{1,180}")

# Content, tests, or files the project writes. Not engine defaults.
OPTIONAL_PREFIXES = (
    "rom/scene/",
    "rom/prefabs/",
    "rom/effects/",
    "rom/editor/",
    "rom/model/object/",
    "rom/model/player/",
    "rom/texture/sprite_demo/",
    "rom/physics/",
    "rom/audio/",
    "rom/terrain/",  # heightmaps the editor writes for sculpted terrains (ADR 0011)
)
OPTIONAL_EXACT = {
    "rom/tags.json",  # project tag list; built-in names already load
    "rom/texture/white.png",  # generated only if branding is missing
    # Retired bitmap font atlas. Old content may still name it; legacy_asset_alias.h reads it as
    # the default UI font. No such file ships.
    "rom/texture/consolab_ascii_512.png",
    # Retired font. Same alias; the file was removed from resources/font.
    "rom/font/LemonSlice.ttf",
    "rom/skip_boot_preload",
    "rom/idle.png",
    "rom/walk.png",
    "rom/model/a.fbx",
    # Comment example in billboard_component.h. Nothing loads this file.
    "rom/texture/point_area_circle.png",
    # Comment example in equirect_cubemap.h. The real file lives in the testbed.
    "rom/skybox/panorama_test.png",
    "rom/skybox/cache/skybox__panorama_test.dds",
}


def normalize(token):
    token = token.replace("\\", "/").rstrip(".,);]}>\"'`")
    while token.endswith("/"):
        token = token[:-1]
    return token


def is_safe_rom(token):
    if not token.startswith("rom/") or len(token) > 1024:
        return False
    parts = token.split("/")
    if any(part in ("", ".", "..") or part[0] == "." or part[-1] in ". " for part in parts):
        return False
    return not any(any(ch in part for ch in '<>:"|?*') for part in parts)


def is_optional(token):
    if not is_safe_rom(token):
        return True
    if token in OPTIONAL_EXACT:
        return True
    if any(token.startswith(prefix) for prefix in OPTIONAL_PREFIXES):
        return True
    # Directory mentions such as rom/shader or rom/model/primitives.
    if "." not in token.split("/")[-1]:
        return True
    if token.startswith("rom/shader/") and token.endswith(".cso"):
        hlsl = REPO / "shaders" / (Path(token).stem + ".hlsl")
        return hlsl.is_file()
    return False


def resources_file(token):
    if not token.startswith("rom/"):
        return None
    direct = RESOURCES / token[4:]
    if direct.is_file():
        return direct
    # Windows paths are case-insensitive and builtin_assets.h keys are lower case.
    current = RESOURCES
    for part in token[4:].split("/"):
        match = None
        if current.is_dir():
            match = next((c for c in current.iterdir() if c.name.lower() == part.lower()), None)
        if match is None:
            return direct
        current = match
    return current


def main():
    missing = []
    seen = {}
    for root in SCAN_ROOTS:
        for path in root.rglob("*"):
            if path.suffix.lower() not in {".cpp", ".h", ".hpp", ".inl"}:
                continue
            rel = path.relative_to(REPO).as_posix()
            text = path.read_text(encoding="utf-8", errors="replace")
            for match in TOKEN.finditer(text):
                token = normalize(match.group(0))
                if not token.startswith("rom/"):
                    continue
                seen.setdefault(token, set()).add(rel)
    for token, where in sorted(seen.items()):
        if is_optional(token):
            continue
        # Paths that appear only in tools/ are fixtures, not engine defaults.
        if all(item.startswith("tools/") for item in where):
            continue
        dest = resources_file(token)
        if dest is None or not dest.is_file():
            missing.append((token, ", ".join(sorted(where))))
    if missing:
        print("FAIL: hardcoded rom paths with no resources file:", file=sys.stderr)
        for token, where in missing:
            print(f"  {token}  ({where})", file=sys.stderr)
        return 1
    print(f"PASS: {len(seen)} hardcoded rom paths are in resources/ or documented optional")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
