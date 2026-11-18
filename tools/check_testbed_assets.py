#!/usr/bin/env python3
"""Resolve every asset path referenced by testbed scenes.

A path is valid only when it is a safe ``rom/`` relative path and the file
exists under ``testbed/`` or, failing that, under repo-root ``resources/``
(``rom/<rest>`` maps to ``resources/<rest>``). The testbed must not keep a
second copy of a file that already lives in ``resources/``.

Scene-to-scene names (``scene``, ``sceneFile``, ``nextScene``) must be plain
filenames in ``testbed/rom/scene``.
"""

import json
import re
import struct
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
TESTBED = REPO / "testbed"
RESOURCES = REPO / "resources"
SCENE_DIR = TESTBED / "rom" / "scene"
SCENE_KEYS = {"scene", "sceneFile", "nextScene"}
PLACEHOLDERS = {
    "rom/texture/sprite_demo/adventurer_walk.png": (256, 128),
    "rom/texture/sprite_demo/test_sheet.png": (256, 128),
    "rom/texture/sprite_demo/hp_fill.png": (128, 32),
    "rom/texture/sprite_demo/hp_frame.png": (128, 32),
}
MAX_PLACEHOLDER_BYTES = 4096


def fail(message):
    print("FAIL:", message, file=sys.stderr)
    raise SystemExit(1)


def is_safe_relative(path):
    if not path or len(path) > 1024:
        return False
    if any(ord(ch) < 0x20 for ch in path):
        return False
    normalized = path.replace("\\", "/")
    if normalized.startswith("/") or (len(normalized) >= 2 and normalized[1] == ":"):
        return False
    parts = normalized.split("/")
    if any(part == "" or part in (".", "..") or part[0] == "." or part[-1] in ". " for part in parts):
        return False
    if any(any(ch in part for ch in '<>:"|?*') for part in parts):
        return False
    return True


def is_inside(root, candidate):
    try:
        candidate.resolve().relative_to(root.resolve())
    except ValueError:
        return False
    return candidate.resolve() != root.resolve()


def resolve_rom(ref):
    if not is_safe_relative(ref) or not ref.replace("\\", "/").startswith("rom/"):
        fail("unsafe rom path: %s" % ref)
    normalized = ref.replace("\\", "/")
    local = TESTBED / normalized
    if local.is_file() and is_inside(TESTBED, local):
        return local.resolve()
    shared = RESOURCES / normalized[4:]
    if shared.is_file() and is_inside(RESOURCES, shared):
        return shared.resolve()
    fail("missing asset: %s" % normalized)


def _builtin_paths():
    header = (REPO / "engine" / "core" / "builtin_paths.h").read_text(encoding="utf-8")
    return set(re.findall(r'"(builtin:[^"]+)"', header))


BUILTIN_PATHS = _builtin_paths()


def walk(value, scene_name, inherited_key=None):
    if isinstance(value, dict):
        for key, item in value.items():
            walk(item, scene_name, key)
        return
    if isinstance(value, list):
        for item in value:
            walk(item, scene_name, inherited_key)
        return
    if not isinstance(value, str) or not value:
        return

    if inherited_key in SCENE_KEYS:
        if "/" in value or "\\" in value or not is_safe_relative(value):
            fail("%s: scene name escapes the project: %s" % (scene_name, value))
        if not value.endswith(".json"):
            fail("%s: scene name is not a json file: %s" % (scene_name, value))
        scene_path = SCENE_DIR / value
        if not scene_path.is_file() or not is_inside(SCENE_DIR, scene_path):
            fail("%s: missing scene %s" % (scene_name, value))
        return

    # "builtin:..." names an engine asset compiled into the exe (engine/core/builtin_paths.h),
    # not a project file. Only the known built-in paths are accepted.
    if value.lower().startswith("builtin:"):
        if value not in BUILTIN_PATHS:
            fail("%s: unknown built-in asset %s" % (scene_name, value))
        return

    normalized = value.replace("\\", "/")
    parts = normalized.split("/")
    asset_suffix = normalized.lower().endswith((
        ".fbx", ".png", ".dds", ".ttf", ".tga", ".wav", ".efk", ".physmat", ".cso"))
    if ".." in parts or normalized.startswith("/") or (len(normalized) > 1 and normalized[1] == ":"):
        fail("%s: path escapes the project: %s" % (scene_name, value))
    if normalized.startswith("rom/") or asset_suffix:
        resolve_rom(normalized)


def is_json_number(value):
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def require_color_array(scene_name, key, value, count):
    if (not isinstance(value, list) or len(value) != count
            or any(not is_json_number(item) for item in value)):
        fail("%s: settings.render.%s must be an array of %d numbers (serializer format)"
             % (scene_name, key, count))


def check_render_settings(scene_name, data):
    """Match RenderSettings_ToJson / SceneJson_ValidateTypes for fields that are present.

    clearColor and skyboxTint are 4-float arrays. An {r,g,b,a} object is the
    format the loader rejects with 'Invalid type: settings.render.clearColor'.
    """
    settings = data.get("settings")
    if settings is None:
        return
    if not isinstance(settings, dict):
        fail("%s: settings must be an object" % scene_name)
    render = settings.get("render")
    if render is None:
        return
    if not isinstance(render, dict):
        fail("%s: settings.render must be an object" % scene_name)
    for key in ("clearColor", "skyboxTint"):
        if key in render:
            require_color_array(scene_name, key, render[key], 4)
    if "fogColor" in render:
        require_color_array(scene_name, "fogColor", render["fogColor"], 3)
    for key in ("skyboxEnabled", "iblEnabled", "fogEnabled"):
        if key in render and not isinstance(render[key], bool):
            fail("%s: settings.render.%s must be a bool" % (scene_name, key))
    for key in ("skyboxExposure", "skyboxRotationYDeg", "iblIntensity",
                "fogDensity", "fogStart", "fogHeight", "fogHeightFalloff"):
        if key in render and not is_json_number(render[key]):
            fail("%s: settings.render.%s must be a number" % (scene_name, key))
    for key in ("skyboxCubemap", "iblIrradiance", "iblPrefilter", "iblBrdfLut", "defaultShader"):
        if key in render and not isinstance(render[key], str):
            fail("%s: settings.render.%s must be a string" % (scene_name, key))
    if "shadowQuality" in render:
        quality = render["shadowQuality"]
        if not isinstance(quality, str) and not (isinstance(quality, int) and not isinstance(quality, bool)):
            fail("%s: settings.render.shadowQuality must be a string or integer" % scene_name)


def main():
    if not (TESTBED / "project.json").is_file():
        fail("testbed/project.json is missing")
    project = json.loads((TESTBED / "project.json").read_text(encoding="utf-8-sig"))
    default_scene = (SCENE_DIR / "default_scene.txt").read_text(encoding="utf-8-sig").strip()
    for name in (project.get("defaultScene"), project.get("lastOpenedScene"), default_scene):
        scene_path = SCENE_DIR / name
        if not name or "/" in name or "\\" in name or not scene_path.is_file():
            fail("startup scene is not inside testbed/rom/scene: %s" % name)
        if not is_inside(SCENE_DIR, scene_path):
            fail("startup scene escapes testbed/rom/scene: %s" % name)

    if (REPO / "samples" / "scenes").exists():
        fail("samples/scenes still exists; scenes belong in testbed/rom/scene")

    scenes = sorted(SCENE_DIR.glob("*.json"))
    if len(scenes) < 2:
        fail("testbed has no scenes")
    for scene_path in scenes:
        data = json.loads(scene_path.read_text(encoding="utf-8-sig"))
        check_render_settings(scene_path.name, data)
        walk(data, scene_path.name)

    for ref, (width, height) in PLACEHOLDERS.items():
        path = resolve_rom(ref)
        blob = path.read_bytes()
        if blob[:8] != b"\x89PNG\r\n\x1a\n" or blob[12:16] != b"IHDR":
            fail("%s is not a PNG" % ref)
        got = struct.unpack(">II", blob[16:24])
        if got != (width, height):
            fail("%s is %sx%s, expected %sx%s" % (ref, got[0], got[1], width, height))
        if path.stat().st_size > MAX_PLACEHOLDER_BYTES:
            fail("%s is %s bytes; placeholders must stay tiny" % (ref, path.stat().st_size))
        if not is_inside(TESTBED, path):
            fail("%s should live in testbed/rom, not resources/" % ref)

    rom_root = TESTBED / "rom"
    if rom_root.is_dir():
        for path in rom_root.rglob("*"):
            if not path.is_file() or path.is_symlink():
                continue
            rel = path.relative_to(rom_root)
            engine_copy = RESOURCES / rel
            if engine_copy.is_file():
                fail("testbed copied an engine default: %s" % path.relative_to(REPO))

    print("PASS: %d testbed scenes resolve inside the project or resources/" % len(scenes))


if __name__ == "__main__":
    main()
