#!/usr/bin/env python3
"""Copy docs/agent + docs/schemas into a game repo for AI agents (stdlib only)."""
from __future__ import annotations

import argparse
import pathlib
import shutil
import subprocess
import sys
from dataclasses import dataclass

BEGIN_MARKER = "<!-- ivy-engine-docs:begin -->"
END_MARKER = "<!-- ivy-engine-docs:end -->"
MANAGED_SECTION = f"""{BEGIN_MARKER}
## Ivy Engine (exported docs)

Game agents: read engine APIs and layout from `docs/ivy-engine/README.md` (docs only; do not require engine source in this repo).
Re-export from the engine with `tools/export_agent_docs.py --game-repo <this-repo>`.
{END_MARKER}
"""

ENGINE_MARKERS = ("IvyEngine.sln", "engine/scripting/Script.h")


class ExportError(ValueError):
    """Invalid export inputs or failed export steps."""


@dataclass
class ExportResult:
    dry_run: bool
    actions: list[str]


def git_commit_hash(engine_root: pathlib.Path) -> str:
    proc = subprocess.run(
        ["git", "-C", str(engine_root), "rev-parse", "HEAD"],
        capture_output=True,
        text=True,
        check=False,
    )
    if proc.returncode != 0:
        raise ExportError(f"Unable to read engine git commit: {proc.stderr.strip()}")
    return proc.stdout.strip()


def _looks_like_engine_repo(path: pathlib.Path) -> bool:
    return (path / "IvyEngine.sln").is_file() and (path / "engine" / "scripting" / "Script.h").is_file()


def validate_game_repo(game_repo: pathlib.Path, engine_root: pathlib.Path) -> pathlib.Path:
    raw = pathlib.Path(game_repo)
    # Refuse relative path segments that attempt traversal before resolve.
    if ".." in raw.parts:
        raise ExportError("Refusing path traversal ('..') in --game-repo")
    if not raw.exists():
        raise ExportError(f"Path does not exist: {raw}")
    if not raw.is_dir():
        raise ExportError(f"Path is not a directory: {raw}")
    resolved = raw.resolve()
    engine_resolved = engine_root.resolve()
    if resolved == engine_resolved:
        raise ExportError("Refusing to export into the engine repo itself")
    if _looks_like_engine_repo(resolved):
        raise ExportError("Refusing --game-repo that looks like an Ivy Engine checkout")
    return resolved


def _managed_block() -> str:
    return MANAGED_SECTION.strip() + "\n"


def upsert_managed_section(path: pathlib.Path, dry_run: bool, actions: list[str]) -> None:
    block = _managed_block()
    original = path.read_text(encoding="utf-8") if path.is_file() else ""
    if BEGIN_MARKER in original and END_MARKER in original:
        before, rest = original.split(BEGIN_MARKER, 1)
        _mid, after = rest.split(END_MARKER, 1)
        if after.startswith("\n"):
            after = after[1:]
        new_text = before + block + after
    elif BEGIN_MARKER in original or END_MARKER in original:
        raise ExportError(f"Broken managed markers in {path}")
    else:
        text = original
        if text and not text.endswith("\n"):
            text += "\n"
        if text:
            text += "\n"
        new_text = text + block
    if new_text == original:
        actions.append(f"unchanged {path}")
        return
    actions.append(f"upsert managed section {path}")
    if not dry_run:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(new_text, encoding="utf-8")


def _rewrite_exported_markdown(text: str) -> str:
    """Point schema links at the exported sibling schemas/ folder."""
    return text.replace("](../schemas/", "](schemas/")


def _copy_tree(src: pathlib.Path, dst: pathlib.Path, dry_run: bool, actions: list[str]) -> None:
    for path in sorted(src.rglob("*")):
        if path.is_dir():
            continue
        rel = path.relative_to(src)
        target = dst / rel
        actions.append(f"copy {path} -> {target}")
        if not dry_run:
            target.parent.mkdir(parents=True, exist_ok=True)
            if path.suffix.lower() == ".md":
                target.write_text(_rewrite_exported_markdown(path.read_text(encoding="utf-8")), encoding="utf-8")
            else:
                shutil.copy2(path, target)


def _remove_stale(dest_root: pathlib.Path, keep: set[pathlib.Path], dry_run: bool, actions: list[str]) -> None:
    if not dest_root.exists():
        return
    for path in sorted(dest_root.rglob("*"), reverse=True):
        if path.is_dir():
            if dry_run:
                continue
            try:
                path.rmdir()
            except OSError:
                pass
            continue
        if path.resolve() not in keep:
            actions.append(f"remove stale {path}")
            if not dry_run:
                path.unlink()


def export_docs(
    engine_root: pathlib.Path,
    game_repo: pathlib.Path,
    *,
    dry_run: bool = False,
) -> ExportResult:
    engine_root = engine_root.resolve()
    game = validate_game_repo(game_repo, engine_root)
    agent_src = engine_root / "docs" / "agent"
    schemas_src = engine_root / "docs" / "schemas"
    if not agent_src.is_dir():
        raise ExportError(f"Missing docs/agent at {agent_src}")
    if not schemas_src.is_dir():
        raise ExportError(f"Missing docs/schemas at {schemas_src}")

    dest_root = game / "docs" / "ivy-engine"
    actions: list[str] = []
    commit = git_commit_hash(engine_root)

    # Plan destination files for stale cleanup.
    planned: list[pathlib.Path] = []
    for path in agent_src.rglob("*"):
        if path.is_file():
            planned.append(dest_root / path.relative_to(agent_src))
    for path in schemas_src.rglob("*"):
        if path.is_file():
            planned.append(dest_root / "schemas" / path.relative_to(schemas_src))
    version_path = dest_root / "ENGINE_COMMIT"
    planned.append(version_path)
    planned_rels = {p.relative_to(dest_root) for p in planned}

    if dry_run:
        actions.append(f"dry-run: would sync into {dest_root}")
        for p in planned:
            actions.append(f"dry-run copy/write {p}")
        actions.append(f"dry-run ENGINE_COMMIT={commit}")
        if dest_root.is_dir():
            for path in sorted(dest_root.rglob("*")):
                if not path.is_file():
                    continue
                rel = path.relative_to(dest_root)
                if rel not in planned_rels:
                    actions.append(f"dry-run remove stale {path}")
        for name in ("AGENTS.md", "CLAUDE.md"):
            actions.append(f"dry-run upsert managed section {game / name}")
        print("\n".join(actions))
        return ExportResult(dry_run=True, actions=actions)

    dest_root.mkdir(parents=True, exist_ok=True)
    _copy_tree(agent_src, dest_root, dry_run=False, actions=actions)
    _copy_tree(schemas_src, dest_root / "schemas", dry_run=False, actions=actions)
    actions.append(f"write {version_path} = {commit}")
    version_path.write_text(commit + "\n", encoding="utf-8")

    keep = {p.resolve() for p in planned}
    _remove_stale(dest_root, keep, dry_run=False, actions=actions)

    for name in ("AGENTS.md", "CLAUDE.md"):
        upsert_managed_section(game / name, dry_run=False, actions=actions)

    return ExportResult(dry_run=False, actions=actions)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Export docs/agent and docs/schemas into a game repo as docs/ivy-engine/."
    )
    parser.add_argument(
        "--game-repo",
        required=True,
        type=pathlib.Path,
        help="Path to the external game/content repository",
    )
    parser.add_argument(
        "--engine-root",
        type=pathlib.Path,
        default=None,
        help="Engine repo root (default: parent of tools/)",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print actions without writing files",
    )
    args = parser.parse_args(argv)
    engine_root = args.engine_root or pathlib.Path(__file__).resolve().parent.parent
    try:
        export_docs(engine_root, args.game_repo, dry_run=args.dry_run)
    except ExportError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
