#!/usr/bin/env python3
"""Verify docs/agent markers against engine headers (Linux-runnable).

Marker kinds:
  `api:Identifier` — C++ symbol / macro / type token in engine/**/*.h or src/app/**/*.h
  `mod:Name` — module/file/namespace: requires a header basename Name.h under those trees
"""
from __future__ import annotations

import argparse
import pathlib
import re
import sys

API_MARKER_RE = re.compile(r"`api:([A-Za-z_][A-Za-z0-9_]*)`")
MOD_MARKER_RE = re.compile(r"`mod:([A-Za-z_][A-Za-z0-9_]*)`")
IDENT_RE = re.compile(r"\b([A-Za-z_][A-Za-z0-9_]*)\b")
HEADER_GLOBS = ("engine/**/*.h", "src/app/**/*.h")


def extract_api_markers(text: str) -> set[str]:
    return set(API_MARKER_RE.findall(text))


def extract_mod_markers(text: str) -> set[str]:
    return set(MOD_MARKER_RE.findall(text))


def collect_header_identifiers(repo_root: pathlib.Path) -> set[str]:
    tokens: set[str] = set()
    for pattern in HEADER_GLOBS:
        for path in repo_root.glob(pattern):
            if not path.is_file():
                continue
            try:
                text = path.read_text(encoding="utf-8", errors="ignore")
            except OSError:
                continue
            tokens.update(IDENT_RE.findall(text))
    return tokens


def collect_header_basenames(repo_root: pathlib.Path) -> set[str]:
    names: set[str] = set()
    for pattern in HEADER_GLOBS:
        for path in repo_root.glob(pattern):
            if path.is_file():
                names.add(path.stem)
    return names


def iter_agent_markdown(docs_agent: pathlib.Path):
    if not docs_agent.is_dir():
        return
    for path in sorted(docs_agent.rglob("*.md")):
        if path.is_file():
            yield path


def find_missing_apis(docs_agent: pathlib.Path, repo_root: pathlib.Path) -> list[str]:
    headers = collect_header_identifiers(repo_root)
    referenced: set[str] = set()
    for path in iter_agent_markdown(docs_agent):
        referenced |= extract_api_markers(path.read_text(encoding="utf-8"))
    return sorted(name for name in referenced if name not in headers)


def find_missing_mods(docs_agent: pathlib.Path, repo_root: pathlib.Path) -> list[str]:
    basenames = collect_header_basenames(repo_root)
    referenced: set[str] = set()
    for path in iter_agent_markdown(docs_agent):
        referenced |= extract_mod_markers(path.read_text(encoding="utf-8"))
    return sorted(name for name in referenced if name not in basenames)


def run_check(docs_agent: pathlib.Path, repo_root: pathlib.Path) -> int:
    if not docs_agent.is_dir():
        print(f"error: docs/agent not found at {docs_agent}", file=sys.stderr)
        return 2
    missing_apis = find_missing_apis(docs_agent, repo_root)
    missing_mods = find_missing_mods(docs_agent, repo_root)
    failed = False
    if missing_apis:
        failed = True
        print("Missing API identifiers (not found in engine/ or src/app/ headers):")
        for name in missing_apis:
            print(f"  - {name}")
    if missing_mods:
        failed = True
        print("Missing module markers (no matching *.h basename under engine/ or src/app/):")
        for name in missing_mods:
            print(f"  - {name}")
    if failed:
        return 1
    print("OK: all docs/agent `api:` / `mod:` markers verified")
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Check docs/agent api:/mod: markers against engine headers."
    )
    parser.add_argument(
        "--repo-root",
        type=pathlib.Path,
        default=None,
        help="Engine repo root (default: parent of tools/)",
    )
    parser.add_argument(
        "--docs-agent",
        type=pathlib.Path,
        default=None,
        help="Path to docs/agent (default: <repo>/docs/agent)",
    )
    args = parser.parse_args(argv)
    repo_root = (args.repo_root or pathlib.Path(__file__).resolve().parent.parent).resolve()
    docs_agent = (args.docs_agent or (repo_root / "docs" / "agent")).resolve()
    return run_check(docs_agent, repo_root)


if __name__ == "__main__":
    raise SystemExit(main())
