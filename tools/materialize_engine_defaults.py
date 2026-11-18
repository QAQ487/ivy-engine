#!/usr/bin/env python3
"""Copy referenced engine defaults into a package rom/.

Project files already in the destination win. Shaders and branding are
overlaid by the caller. This script adds:

- host files the runtime opens even when no scene names them
  (the default UI font; the fade fill and debug text font are built into the exe)
- every safe rom/ path named in the destination's text files, when that
  file exists under resources/ and is not already in the package

rom/skybox/MySkybox.dds is included only when a content file names it.
resources/skybox/source is authoring input and is never copied.
"""

import sys
from pathlib import Path

HOST_ROM_PATHS = (
    "rom/font/LemonSlice.ttf",
)
TEXT_SUFFIXES = {".json", ".txt", ".prefab", ".physmat", ".mtl", ".obj", ".md"}


def is_safe_rom(path):
    if not path.startswith("rom/") or len(path) > 1024:
        return False
    parts = path.split("/")
    if any(part in ("", ".", "..") or part[0] == "." or part[-1] in ". " for part in parts):
        return False
    if any(any(ch in part for ch in '<>:"|?*') for part in parts):
        return False
    return True


def collect_references(root):
    found = set(HOST_ROM_PATHS)
    if not root.is_dir():
        return found
    for path in root.rglob("*"):
        if not path.is_file() or path.suffix.lower() not in TEXT_SUFFIXES:
            continue
        text = path.read_text(encoding="utf-8", errors="ignore").replace("\\", "/")
        start = 0
        while True:
            index = text.find("rom/", start)
            if index < 0:
                break
            if index > 0 and (text[index - 1].isalnum() or text[index - 1] in "_/"):
                start = index + 4
                continue
            end = index
            while end < len(text) and (
                text[end].isalnum() or text[end] in "/_.-+ "
            ):
                end += 1
            token = text[index:end].rstrip(" .")
            if is_safe_rom(token):
                found.add(token)
            start = end
    return found


def copy_if_missing(resources, dest_rom, rom_path):
    rest = rom_path[4:]
    if rest == "skybox/source" or rest.startswith("skybox/source/"):
        return False
    source = resources / rest
    dest = dest_rom / rest
    if dest.is_file() or not source.is_file():
        return False
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_bytes(source.read_bytes())
    return True


def main():
    if len(sys.argv) != 4:
        print("usage: materialize_engine_defaults.py <scan-root> <resources> <dest-rom>",
              file=sys.stderr)
        return 1
    scan_root = Path(sys.argv[1])
    resources = Path(sys.argv[2])
    dest_rom = Path(sys.argv[3])
    if not resources.is_dir():
        print("missing resources:", resources, file=sys.stderr)
        return 1
    copied = 0
    for rom_path in sorted(collect_references(scan_root)):
        if copy_if_missing(resources, dest_rom, rom_path):
            copied += 1
            print("added", rom_path)
    print("materialized", copied, "engine default(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
