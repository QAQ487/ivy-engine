#!/usr/bin/env python3
"""Unit tests for tools/export_agent_docs.py (temp dirs; Linux-runnable)."""
from __future__ import annotations

import io
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
import unittest
from contextlib import redirect_stdout
from unittest import mock

HERE = pathlib.Path(__file__).resolve().parent
REPO_ROOT = HERE.parent
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

import export_agent_docs as export  # noqa: E402


def _write(path: pathlib.Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


class ExportAgentDocsTests(unittest.TestCase):
    def setUp(self) -> None:
        self._tmpdir = tempfile.TemporaryDirectory()
        self.root = pathlib.Path(self._tmpdir.name)
        self.engine = self.root / "engine-repo"
        self.game = self.root / "game-repo"
        self.engine.mkdir()
        self.game.mkdir()
        # Minimal engine doc pack + marker that this tree is "the engine".
        _write(self.engine / "IvyEngine.sln", "Microsoft Visual Studio Solution File\n")
        _write(self.engine / "docs" / "agent" / "README.md", "# Agent README\n")
        _write(self.engine / "docs" / "agent" / "scripting.md", "# Scripting\n")
        _write(
            self.engine / "docs" / "schemas" / "runtime_bridge_scenario.v1.schema.json",
            '{"$schema":"https://json-schema.org/draft/2020-12/schema"}\n',
        )
        (self.engine / ".git").mkdir()

    def tearDown(self) -> None:
        self._tmpdir.cleanup()

    def test_refuse_missing_path(self) -> None:
        missing = self.root / "nope"
        with self.assertRaises(export.ExportError) as ctx:
            export.validate_game_repo(missing, self.engine)
        self.assertIn("exist", str(ctx.exception).lower())

    def test_refuse_file_not_directory(self) -> None:
        f = self.root / "file.txt"
        f.write_text("x", encoding="utf-8")
        with self.assertRaises(export.ExportError) as ctx:
            export.validate_game_repo(f, self.engine)
        self.assertIn("directory", str(ctx.exception).lower())

    def test_refuse_engine_repo_itself(self) -> None:
        with self.assertRaises(export.ExportError) as ctx:
            export.validate_game_repo(self.engine, self.engine)
        self.assertIn("engine", str(ctx.exception).lower())

    def test_refuse_path_traversal_outside(self) -> None:
        # Relative path with .. that escapes after resolve still must be a real dir;
        # craft a path string the validator rejects for traversal before resolve abuse.
        with self.assertRaises(export.ExportError) as ctx:
            export.validate_game_repo(pathlib.Path("../outside"), self.engine)
        self.assertRegex(str(ctx.exception).lower(), r"traversal|\.\.")

    def test_copy_docs_and_version_marker(self) -> None:
        fake_hash = "abc123def456"
        _write(
            self.engine / "docs" / "agent" / "README.md",
            "# Agent\nSee [schema](../schemas/runtime_bridge_scenario.v1.schema.json).\n",
        )
        with mock.patch.object(export, "git_commit_hash", return_value=fake_hash):
            result = export.export_docs(self.engine, self.game, dry_run=False)
        dest = self.game / "docs" / "ivy-engine"
        self.assertTrue((dest / "README.md").is_file())
        self.assertTrue((dest / "scripting.md").is_file())
        self.assertTrue(
            (dest / "schemas" / "runtime_bridge_scenario.v1.schema.json").is_file()
        )
        readme = (dest / "README.md").read_text(encoding="utf-8")
        self.assertIn("](schemas/runtime_bridge_scenario.v1.schema.json)", readme)
        self.assertNotIn("](../schemas/", readme)
        version = (dest / "ENGINE_COMMIT").read_text(encoding="utf-8").strip()
        self.assertEqual(version, fake_hash)
        self.assertFalse(result.dry_run)

    def test_removes_stale_files_only_under_managed_folder(self) -> None:
        dest = self.game / "docs" / "ivy-engine"
        _write(dest / "stale.md", "old\n")
        _write(self.game / "docs" / "keep-me.md", "untouched\n")
        with mock.patch.object(export, "git_commit_hash", return_value="deadbeef"):
            export.export_docs(self.engine, self.game, dry_run=False)
        self.assertFalse((dest / "stale.md").exists())
        self.assertTrue((self.game / "docs" / "keep-me.md").is_file())
        self.assertTrue((dest / "README.md").is_file())

    def test_managed_section_idempotent_and_preserves_outside(self) -> None:
        agents = self.game / "AGENTS.md"
        claude = self.game / "CLAUDE.md"
        _write(agents, "# Game agents\n\nKeep this intro.\n")
        _write(claude, "Human notes above.\n")
        with mock.patch.object(export, "git_commit_hash", return_value="hash1"):
            export.export_docs(self.engine, self.game, dry_run=False)
        first_agents = agents.read_text(encoding="utf-8")
        first_claude = claude.read_text(encoding="utf-8")
        self.assertIn("Keep this intro.", first_agents)
        self.assertIn("Human notes above.", first_claude)
        self.assertIn(export.BEGIN_MARKER, first_agents)
        self.assertIn(export.END_MARKER, first_agents)
        self.assertIn("docs/ivy-engine/README.md", first_agents)
        self.assertIn(export.BEGIN_MARKER, first_claude)
        self.assertEqual(first_agents.count(export.BEGIN_MARKER), 1)

        with mock.patch.object(export, "git_commit_hash", return_value="hash2"):
            export.export_docs(self.engine, self.game, dry_run=False)
        second_agents = agents.read_text(encoding="utf-8")
        self.assertEqual(second_agents.count(export.BEGIN_MARKER), 1)
        self.assertEqual(second_agents.count(export.END_MARKER), 1)
        self.assertIn("Keep this intro.", second_agents)
        # Outside content unchanged; managed block replaced in place.
        before = second_agents.split(export.BEGIN_MARKER)[0]
        self.assertEqual(before, first_agents.split(export.BEGIN_MARKER)[0])

    def test_creates_agents_and_claude_when_missing(self) -> None:
        with mock.patch.object(export, "git_commit_hash", return_value="h"):
            export.export_docs(self.engine, self.game, dry_run=False)
        self.assertTrue((self.game / "AGENTS.md").is_file())
        self.assertTrue((self.game / "CLAUDE.md").is_file())
        self.assertIn(export.BEGIN_MARKER, (self.game / "AGENTS.md").read_text(encoding="utf-8"))

    def test_dry_run_writes_nothing(self) -> None:
        with mock.patch.object(export, "git_commit_hash", return_value="h"):
            buf = io.StringIO()
            with redirect_stdout(buf):
                export.export_docs(self.engine, self.game, dry_run=True)
        self.assertFalse((self.game / "docs" / "ivy-engine").exists())
        self.assertFalse((self.game / "AGENTS.md").exists())
        self.assertIn("dry-run", buf.getvalue().lower())


class ExportCliSmoke(unittest.TestCase):
    def test_module_main_help(self) -> None:
        proc = subprocess.run(
            [sys.executable, str(HERE / "export_agent_docs.py"), "--help"],
            capture_output=True,
            text=True,
            cwd=str(REPO_ROOT),
            check=False,
        )
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertIn("--game-repo", proc.stdout)


if __name__ == "__main__":
    unittest.main()
