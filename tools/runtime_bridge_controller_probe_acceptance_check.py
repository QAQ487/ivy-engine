#!/usr/bin/env python3
"""Pure unit tests for controller probe acceptance helpers (no Windows runtime)."""
import json
import pathlib
import sys
import tempfile
import unittest
from types import ModuleType, SimpleNamespace

HERE = pathlib.Path(__file__).resolve().parent
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

# Import-safe on Linux: probe acceptance pulls session_of via mouse runner, which
# imports the WinDLL named-pipe client at module load.
if "runtime_test_client" not in sys.modules:
    _stub = ModuleType("runtime_test_client")

    class Client:  # noqa: D401 - test stub
        pass

    _stub.Client = Client
    sys.modules["runtime_test_client"] = _stub

import runtime_bridge_controller_probe_acceptance as runner
import runtime_bridge_mouse_acceptance as mouse
from runtime_bridge_controller_probe import (
    RESPONSE_BYTE_LIMIT,
    find_trigger_window,
    frames_after_sequence,
    max_controller_probe_response_bytes,
)


def _frame(lt=0.0, rt=0.0, sequence=1, pressed=None, pressed_edge=None, released_edge=None):
    return {
        "sequence": sequence,
        "lx": 0.0, "ly": 0.0, "rx": 0.0, "ry": 0.0, "lt": lt, "rt": rt,
        "pressed": list(pressed or []),
        "pressedEdge": list(pressed_edge or []),
        "releasedEdge": list(released_edge or []),
        "connected": True,
    }


class ProbeAcceptanceCheck(unittest.TestCase):
    def test_validate_runner_args(self):
        good = SimpleNamespace(
            exe="x", out_dir="y", fixture_scene="z.json",
            settle=0.5, ready_timeout=10.0, shutdown_timeout=10.0, completion_timeout=5.0)
        self.assertIsNone(runner.validate_runner_args(good))
        bad = SimpleNamespace(**{**good.__dict__, "exe": ""})
        self.assertEqual(runner.validate_runner_args(bad), "exe path required")
        bad = SimpleNamespace(**{**good.__dict__, "settle": 0.0})
        self.assertIn("settle", runner.validate_runner_args(bad))

    def test_response_bound_documented(self):
        self.assertGreaterEqual(RESPONSE_BYTE_LIMIT, max_controller_probe_response_bytes())
        self.assertGreater(RESPONSE_BYTE_LIMIT, 8192)

    def test_sequence_window_not_trailing_slice(self):
        # Regression: do not assert frames[-4:] after an arbitrary sleep.
        frames = [
            _frame(sequence=1),
            _frame(sequence=2),
            _frame(lt=1.0, sequence=3),
            _frame(lt=1.0, sequence=4),
            _frame(sequence=5),
            _frame(sequence=6),
            _frame(sequence=7),
        ]
        window = frames_after_sequence(frames, 1)
        found, err = find_trigger_window(window, True, 1.0, 2)
        self.assertIsNone(err)
        self.assertEqual([f["sequence"] for f in found], [2, 3, 4, 5])
        # Trailing idle would make [-4:] wrong if the window grew.
        wrong = window[-4:]
        self.assertNotEqual([f["sequence"] for f in wrong], [2, 3, 4, 5])

    def test_default_fixture_is_testbed_not_samples(self):
        path = runner.DEFAULT_FIXTURE
        self.assertTrue(path.is_file(), "missing probe fixture: %s" % path)
        parts = path.parts
        self.assertIn("testbed", parts)
        self.assertNotIn("samples", parts)
        self.assertEqual(path.name, "scene_runtime_bridge_controller_probe.json")

    def test_stage_fixture_copies_referenced_assets(self):
        with tempfile.TemporaryDirectory() as td:
            temp_dir = pathlib.Path(td)
            out_dir = temp_dir / "out"
            out_dir.mkdir()
            exe_dir = temp_dir / "Debug"
            exe_dir.mkdir()
            exe = exe_dir / "IvyEngineRuntime.exe"
            exe.write_bytes(b"MZ\x00\x00")
            (exe_dir / "resources" / "skybox").mkdir(parents=True)
            (exe_dir / "resources" / "skybox" / "MySkybox.dds").write_bytes(b"DDS")
            (exe_dir / "resources" / "font").mkdir(parents=True)
            (exe_dir / "resources" / "font" / "LemonSlice.ttf").write_bytes(b"TTF")

            staged_exe, package = runner.stage_fixture_package(
                exe, out_dir, runner.DEFAULT_FIXTURE)
            self.assertEqual(staged_exe, package / exe.name)
            self.assertTrue((package / "rom" / "scene" / runner.EXPECTED_SCENE).is_file())
            self.assertTrue((package / "rom" / "model" / "object" / "Box_Brown.fbx").is_file())
            self.assertTrue(mouse.fixture_asset_readable(package, "rom/skybox/MySkybox.dds"))
            self.assertTrue(
                mouse.fixture_asset_readable(package, "rom/font/LemonSlice.ttf"))

            scene = json.loads(runner.DEFAULT_FIXTURE.read_text(encoding="utf-8"))
            for rom_path in mouse.collect_fixture_rom_paths(scene):
                self.assertTrue(
                    mouse.fixture_asset_readable(package, rom_path),
                    "unresolved after staging: %s" % rom_path)


if __name__ == "__main__":
    unittest.main()
