#!/usr/bin/env python3
"""Unit tests for tools/agent_docs_check.py (temp dirs; Linux-runnable)."""
from __future__ import annotations

import pathlib
import sys
import tempfile
import unittest

HERE = pathlib.Path(__file__).resolve().parent
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

import agent_docs_check as check  # noqa: E402


def _write(path: pathlib.Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


class AgentDocsCheckTests(unittest.TestCase):
    def setUp(self) -> None:
        self._tmpdir = tempfile.TemporaryDirectory()
        self.root = pathlib.Path(self._tmpdir.name)
        self.docs = self.root / "docs" / "agent"
        self.headers = self.root / "engine"
        _write(
            self.headers / "scripting" / "script_runtime_api.h",
            "void ScriptApi_FindTransform();\nvoid ScriptApi_PrefabInstantiate();\n",
        )
        _write(
            self.root / "src" / "app" / "camera" / "third_person_camera.h",
            "void ThirdPersonCamera_SetTarget();\n",
        )

    def tearDown(self) -> None:
        self._tmpdir.cleanup()

    def test_extract_api_markers(self) -> None:
        text = "Use `api:ScriptApi_FindTransform` and `api:OnCreate`.\nAlso `not-a-marker` and `api:`.\n"
        names = check.extract_api_markers(text)
        self.assertEqual(names, {"ScriptApi_FindTransform", "OnCreate"})

    def test_header_tokens_include_symbols(self) -> None:
        tokens = check.collect_header_identifiers(self.root)
        self.assertIn("ScriptApi_FindTransform", tokens)
        self.assertIn("ThirdPersonCamera_SetTarget", tokens)

    def test_missing_name_fails(self) -> None:
        _write(
            self.docs / "scripting.md",
            "Call `api:ScriptApi_DoesNotExist` and `api:ScriptApi_FindTransform`.\n",
        )
        missing = check.find_missing_apis(self.docs, self.root)
        self.assertEqual(missing, ["ScriptApi_DoesNotExist"])

    def test_all_present_passes(self) -> None:
        _write(
            self.docs / "scripting.md",
            "Use `api:ScriptApi_FindTransform`.\n",
        )
        missing = check.find_missing_apis(self.docs, self.root)
        self.assertEqual(missing, [])

    def test_run_check_exit_codes(self) -> None:
        _write(self.docs / "ok.md", "See `api:ScriptApi_FindTransform`.\n")
        self.assertEqual(check.run_check(self.docs, self.root), 0)
        _write(self.docs / "bad.md", "Missing `api:NoSuchThing`.\n")
        self.assertEqual(check.run_check(self.docs, self.root), 1)


if __name__ == "__main__":
    unittest.main()
