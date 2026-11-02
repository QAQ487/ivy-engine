#!/usr/bin/env python3
"""Unit tests for runtime-bridge scenario schema, asserts, and fake-client runner."""
from __future__ import annotations

import json
import pathlib
import sys
import tempfile
import unittest

HERE = pathlib.Path(__file__).resolve().parent
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

from runtime_bridge_scenario_schema import (  # noqa: E402
    ScenarioValidationError,
    eval_assert,
    resolve_path,
    validate_scenario,
    validate_scene_path,
)
from runtime_bridge_scenario_runner import (  # noqa: E402
    FakeBridge,
    run_scenario_with_bridge,
)


def _minimal(**overrides):
    doc = {
        "version": 1,
        "name": "demo",
        "scene": "rom/scene/scene_runtime_bridge_world_manip.json",
        "steps": [{"type": "wait_ready"}],
    }
    doc.update(overrides)
    return doc


class ScenePathTests(unittest.TestCase):
    def test_accepts_rom_json(self):
        self.assertEqual(
            validate_scene_path("rom/scene/scene_runtime_bridge_world_manip.json"),
            "rom/scene/scene_runtime_bridge_world_manip.json",
        )

    def test_rejects_parent_and_absolute(self):
        with self.assertRaises(ScenarioValidationError) as ctx:
            validate_scene_path("rom/../secrets.json")
        self.assertIn("..", str(ctx.exception))
        with self.assertRaises(ScenarioValidationError):
            validate_scene_path("/rom/scene/x.json")
        with self.assertRaises(ScenarioValidationError):
            validate_scene_path("scene/foo.json")
        with self.assertRaises(ScenarioValidationError):
            validate_scene_path("rom/scene/foo.txt")


class SchemaValidationTests(unittest.TestCase):
    def test_minimal_ok(self):
        out = validate_scenario(_minimal())
        self.assertEqual(out["version"], 1)
        self.assertEqual(out["timeoutSec"], 120.0)
        self.assertEqual(out["steps"][0]["type"], "wait_ready")

    def test_unknown_top_level_key(self):
        with self.assertRaises(ScenarioValidationError) as ctx:
            validate_scenario(_minimal(extra=True))
        self.assertTrue(str(ctx.exception).startswith("$:"))
        self.assertIn("unknown key", str(ctx.exception))

    def test_unknown_step_type(self):
        with self.assertRaises(ScenarioValidationError) as ctx:
            validate_scenario(_minimal(steps=[{"type": "fly"}]))
        self.assertIn("$.steps[0].type", str(ctx.exception))

    def test_unknown_step_key(self):
        with self.assertRaises(ScenarioValidationError) as ctx:
            validate_scenario(_minimal(steps=[{"type": "pause", "extra": 1}]))
        self.assertIn("unknown key", str(ctx.exception))

    def test_nonfinite_rejected(self):
        with self.assertRaises(ScenarioValidationError):
            validate_scenario(_minimal(steps=[{
                "type": "apply_impulse",
                "object": "DynamicBox",
                "impulse": [float("nan"), 0, 0],
            }]))

    def test_frames_bounded(self):
        with self.assertRaises(ScenarioValidationError):
            validate_scenario(_minimal(steps=[{"type": "step", "frames": 0}]))
        with self.assertRaises(ScenarioValidationError):
            validate_scenario(_minimal(steps=[{"type": "step", "frames": 601}]))

    def test_assert_ops_validated(self):
        ok = validate_scenario(_minimal(steps=[{
            "type": "assert",
            "target": "DynamicBox.position.x",
            "op": "approx",
            "value": 0,
            "tol": 0.05,
        }]))
        self.assertEqual(ok["steps"][0]["op"], "approx")
        with self.assertRaises(ScenarioValidationError):
            validate_scenario(_minimal(steps=[{
                "type": "assert",
                "target": "DynamicBox.position.x",
                "op": "nope",
                "value": 0,
            }]))

    def test_set_transform_requires_component(self):
        with self.assertRaises(ScenarioValidationError):
            validate_scenario(_minimal(steps=[{
                "type": "set_transform",
                "object": "DynamicBox",
            }]))

    def test_sample_fields(self):
        out = validate_scenario(_minimal(steps=[{
            "type": "sample",
            "object": "FreeSpinBox",
            "fields": ["angularVelocity", "position"],
            "everyFrames": 1,
            "count": 30,
        }]))
        self.assertEqual(out["steps"][0]["count"], 30)
        with self.assertRaises(ScenarioValidationError):
            validate_scenario(_minimal(steps=[{
                "type": "sample",
                "object": "FreeSpinBox",
                "fields": ["nope"],
                "everyFrames": 1,
                "count": 1,
            }]))


class ResolveAndAssertTests(unittest.TestCase):
    def test_resolve_observe_path(self):
        ctx = {
            "observes": {
                "DynamicBox": {
                    "found": True,
                    "position": {"x": 3.0, "y": 2.0, "z": 0.0},
                    "velocity": {"x": 1.5, "y": 0.0, "z": 0.0},
                }
            }
        }
        self.assertEqual(resolve_path("DynamicBox.position.x", ctx), 3.0)
        self.assertEqual(resolve_path("DynamicBox.velocity.x", ctx), 1.5)

    def test_resolve_sample_index(self):
        ctx = {
            "samples": [
                {"angularVelocity": {"x": 0.0, "y": 0.0, "z": 2.0}},
                {"angularVelocity": {"x": 0.0, "y": 0.0, "z": 1.9}},
            ]
        }
        self.assertEqual(resolve_path("sample[-1].angularVelocity.z", ctx), 1.9)
        self.assertEqual(resolve_path("sample[0].angularVelocity.z", ctx), 2.0)

    def test_assert_ops(self):
        self.assertTrue(eval_assert("eq", 1, 1)["pass"])
        self.assertTrue(eval_assert("approx", 1.001, 1.0, tol=0.01)["pass"])
        self.assertFalse(eval_assert("approx", 1.2, 1.0, tol=0.01)["pass"])
        self.assertTrue(eval_assert("gt", 2, 1)["pass"])
        self.assertTrue(eval_assert("lt", 1, 2)["pass"])
        self.assertTrue(eval_assert("between", 0.5, [0.0, 1.0])["pass"])
        self.assertFalse(eval_assert("between", 1.5, [0.0, 1.0])["pass"])
        self.assertTrue(eval_assert("changed", 2, previous=1)["pass"])
        self.assertFalse(eval_assert("changed", 1, previous=1)["pass"])
        self.assertTrue(eval_assert(
            "axis_dot",
            {"x": 0.0, "y": 0.0, "z": 2.0},
            [0.0, 0.0, 1.0],
            tol=0.98,
        )["pass"])
        self.assertFalse(eval_assert(
            "axis_dot",
            {"x": 1.0, "y": 0.0, "z": 0.0},
            [0.0, 0.0, 1.0],
            tol=0.98,
        )["pass"])

    def test_approx_vec3(self):
        self.assertTrue(eval_assert(
            "approx",
            {"x": 3.01, "y": 2.0, "z": 0.0},
            [3.0, 2.0, 0.0],
            tol=0.05,
        )["pass"])


class FakeClientRunTests(unittest.TestCase):
    def test_pass_and_cleanup(self):
        scenario = validate_scenario({
            "version": 1,
            "name": "fake-pass",
            "scene": "rom/scene/scene_runtime_bridge_world_manip.json",
            "timeoutSec": 30,
            "steps": [
                {"type": "wait_ready"},
                {"type": "list_objects", "limit": 64},
                {"type": "observe", "object": "DynamicBox"},
                {"type": "assert", "target": "DynamicBox.position.x", "op": "approx",
                 "value": 0.0, "tol": 0.05},
                {"type": "pause"},
                {"type": "set_transform", "object": "DynamicBox",
                 "position": [3.0, 2.0, 0.0], "zeroVelocity": True},
                {"type": "observe", "object": "DynamicBox"},
                {"type": "assert", "target": "DynamicBox.position.x", "op": "approx",
                 "value": 3.0, "tol": 0.05},
                {"type": "label", "text": "done"},
            ],
        })
        bridge = FakeBridge(initial_positions={"DynamicBox": (0.0, 2.0, 0.0)})
        with tempfile.TemporaryDirectory() as tmp:
            out_dir = pathlib.Path(tmp) / "out"
            report = run_scenario_with_bridge(
                scenario, bridge, out_dir=out_dir, keep_open=False)
            self.assertEqual(report["status"], "PASS")
            self.assertTrue(bridge.quit_called)
            self.assertTrue(bridge.closed)
            result_path = out_dir / "result.json"
            self.assertTrue(result_path.is_file())
            saved = json.loads(result_path.read_text(encoding="utf-8"))
            self.assertEqual(saved["status"], "PASS")
            self.assertTrue(saved["steps"])

    def test_fail_assert_still_cleans_up(self):
        scenario = validate_scenario({
            "version": 1,
            "name": "fake-fail",
            "scene": "rom/scene/scene_runtime_bridge_world_manip.json",
            "steps": [
                {"type": "wait_ready"},
                {"type": "observe", "object": "DynamicBox"},
                {"type": "assert", "target": "DynamicBox.position.x", "op": "eq", "value": 99},
            ],
        })
        bridge = FakeBridge(initial_positions={"DynamicBox": (0.0, 2.0, 0.0)})
        with tempfile.TemporaryDirectory() as tmp:
            out_dir = pathlib.Path(tmp) / "out-fail"
            report = run_scenario_with_bridge(
                scenario, bridge, out_dir=out_dir, keep_open=False)
            self.assertEqual(report["status"], "FAIL")
            self.assertTrue(bridge.quit_called)
            self.assertTrue(bridge.closed)
            self.assertTrue((out_dir / "result.json").is_file())

    def test_keep_open_skips_quit_on_pass(self):
        scenario = validate_scenario(_minimal(steps=[
            {"type": "wait_ready"},
            {"type": "comment", "text": "noop"},
        ]))
        bridge = FakeBridge()
        with tempfile.TemporaryDirectory() as tmp:
            out_dir = pathlib.Path(tmp) / "keep"
            report = run_scenario_with_bridge(
                scenario, bridge, out_dir=out_dir, keep_open=True)
            self.assertEqual(report["status"], "PASS")
            self.assertFalse(bridge.quit_called)
            self.assertTrue(bridge.closed)

    def test_example_scenarios_validate(self):
        root = HERE.parent / "testbed" / "scenarios"
        if not root.is_dir():
            self.skipTest("example scenarios not present")
        files = sorted(root.glob("*.json"))
        self.assertGreaterEqual(len(files), 3)
        for path in files:
            data = json.loads(path.read_text(encoding="utf-8"))
            try:
                validate_scenario(data)
            except ScenarioValidationError as exc:
                self.fail("%s failed validation: %s" % (path.name, exc))


class CliGuardTests(unittest.TestCase):
    def test_import_runner_safe_on_linux(self):
        import runtime_bridge_scenario_runner as mod
        self.assertTrue(callable(mod.main))

    def test_validate_only_path(self):
        from runtime_bridge_scenario_runner import load_and_validate_scenario
        path = HERE.parent / "testbed" / "scenarios" / "world_manip.json"
        if not path.is_file():
            self.skipTest("world_manip scenario missing")
        scenario = load_and_validate_scenario(path)
        self.assertEqual(scenario["name"], "world-manip")


if __name__ == "__main__":
    unittest.main()
