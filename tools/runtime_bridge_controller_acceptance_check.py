"""Unit tests for left-stick controller runtime acceptance (false-PASS prevention)."""
import argparse
import pathlib
import subprocess
import sys
import tempfile
import unittest

HERE = pathlib.Path(__file__).resolve().parent
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

import runtime_bridge_controller_acceptance as runner
from runtime_bridge_mouse_acceptance_check import make_dummy_png

OP_ID = 42


def _pos(x, y, z):
    return {"x": x, "y": y, "z": z}


def _vel(x, y, z):
    return {"x": x, "y": y, "z": z}


def _sample(status, remaining, x, y, z, vx=0.0, vy=0.0, vz=6.0, request_id=OP_ID, found=True):
    return {
        "requestId": request_id,
        "status": status,
        "remaining": remaining,
        "found": found,
        "position": _pos(x, y, z),
        "velocity": _vel(vx, vy, vz),
    }


def _valid_walk_samples():
    samples = [
        _sample("active", 100, 0.0, 0.5, 1.0),
        _sample("active", 80, 0.0, 0.5, 3.0),
        _sample("active", 60, 0.0, 0.5, 5.0),
        _sample("active", 40, 0.0, 0.5, 7.0),
        _sample("active", 20, 0.0, 0.5, 9.0),
        _sample("active", 5, 0.0, 0.5, 10.5),
        _sample("active", 0, 0.0, 0.5, 11.6, vx=0.0, vz=0.0),
        _sample("releasing", 0, 0.0, 0.5, 11.6, vx=0.0, vz=0.0),
        _sample("completed", 0, 0.0, 0.5, 11.612895, vx=0.0, vz=0.0),
    ]
    return samples, _pos(0.0, 0.5, 0.0), _pos(0.0, 0.5, 11.612895)


def _release_samples(anchor=None):
    anchor = anchor or _pos(0.0, 0.5, 11.612895)
    return [
        _sample("completed", 0, anchor["x"], anchor["y"], anchor["z"], vx=0.0, vz=0.0),
        _sample("completed", 0, anchor["x"] + 0.01, anchor["y"], anchor["z"], vx=0.0, vz=0.0),
        _sample("completed", 0, anchor["x"], anchor["y"], anchor["z"] - 0.01, vx=0.02, vz=0.0),
    ]


def _args(**overrides):
    ns = argparse.Namespace(
        exe="C:\\dummy\\IvyEngineRuntime.exe",
        out_dir="C:\\dummy\\out-new",
        expected_scene="scene_runtime_bridge_player.json",
        settle=0.6,
        release_settle=0.3,
        ready_timeout=25.0,
        shutdown_timeout=15.0,
        timeout=10.0,
        poll_interval=0.05,
    )
    for key, value in overrides.items():
        setattr(ns, key, value)
    return ns


class ControllerAcceptancePureValidationTests(unittest.TestCase):
    def test_valid_walk_passes_and_excludes_exhausted_budget_from_speed_only(self):
        samples, start, end = _valid_walk_samples()
        result = runner.validate_walking_samples(samples, start, end, OP_ID)
        self.assertTrue(result["pass"], result)
        self.assertEqual(result["activeSamples"], 6)
        self.assertLessEqual(result["horizontalDisplacement"], runner.MAX_DISPLACEMENT)
        self.assertFalse(runner.is_active_hold_sample(samples[6]))
        self.assertTrue(runner.is_active_hold_sample(samples[0]))

    def test_empty_samples_fail(self):
        self.assertFalse(runner.validate_walking_samples([], _pos(0, 0.5, 0), _pos(0, 0.5, 12), OP_ID)["pass"])
        self.assertFalse(runner.validate_walking_samples(None, _pos(0, 0.5, 0), _pos(0, 0.5, 12), OP_ID)["pass"])

    def test_request_id_mismatch_fails(self):
        samples, start, end = _valid_walk_samples()
        samples[2]["requestId"] = OP_ID + 1
        self.assertFalse(runner.validate_walking_samples(samples, start, end, OP_ID)["pass"])
        life = runner.validate_lifecycle_sample(samples[2], OP_ID, start)
        self.assertFalse(life["pass"])

    def test_missing_or_negative_remaining_fails(self):
        samples, start, end = _valid_walk_samples()
        samples[1]["remaining"] = None
        self.assertFalse(runner.validate_walking_samples(samples, start, end, OP_ID)["pass"])
        samples, start, end = _valid_walk_samples()
        samples[1]["remaining"] = -1
        self.assertFalse(runner.validate_walking_samples(samples, start, end, OP_ID)["pass"])
        samples, start, end = _valid_walk_samples()
        samples[1]["remaining"] = True
        self.assertFalse(runner.validate_walking_samples(samples, start, end, OP_ID)["pass"])

    def test_observe_found_false_fails(self):
        samples, start, end = _valid_walk_samples()
        samples[0]["found"] = False
        self.assertFalse(runner.validate_walking_samples(samples, start, end, OP_ID)["pass"])

    def test_malformed_status_fails(self):
        samples, start, end = _valid_walk_samples()
        samples[0]["status"] = "weird"
        self.assertFalse(runner.validate_walking_samples(samples, start, end, OP_ID)["pass"])

    def test_huge_displacement_fails(self):
        samples, start, end = _valid_walk_samples()
        end = _pos(0.0, 0.5, 50.0)
        samples[-1] = _sample("completed", 0, 0.0, 0.5, 50.0, vx=0.0, vz=0.0)
        result = runner.validate_walking_samples(samples, start, end, OP_ID)
        self.assertFalse(result["pass"])
        self.assertIn(">", result["error"])

    def test_sample_height_and_vertical_velocity_fail(self):
        samples, start, end = _valid_walk_samples()
        samples[2]["position"]["y"] = 1.5
        self.assertFalse(runner.validate_walking_samples(samples, start, end, OP_ID)["pass"])
        samples, start, end = _valid_walk_samples()
        samples[3]["velocity"]["y"] = 2.0
        self.assertFalse(runner.validate_walking_samples(samples, start, end, OP_ID)["pass"])
        samples, start, end = _valid_walk_samples()
        end = _pos(0.0, 1.0, 11.612895)
        self.assertFalse(runner.validate_walking_samples(samples, start, end, OP_ID)["pass"])

    def test_boundary_and_completed_nonfinite_still_fail(self):
        samples, start, end = _valid_walk_samples()
        samples[6]["position"]["x"] = float("nan")
        self.assertFalse(runner.validate_walking_samples(samples, start, end, OP_ID)["pass"])
        samples, start, end = _valid_walk_samples()
        samples[-1]["velocity"]["y"] = float("inf")
        self.assertFalse(runner.validate_walking_samples(samples, start, end, OP_ID)["pass"])

    def test_small_displacement_fails(self):
        samples = [
            _sample("active", 10, 0.0, 0.5, 0.05),
            _sample("active", 5, 0.0, 0.5, 0.08),
            _sample("active", 2, 0.0, 0.5, 0.09),
            _sample("active", 1, 0.0, 0.5, 0.095),
            _sample("active", 1, 0.0, 0.5, 0.096),
            _sample("completed", 0, 0.0, 0.5, 0.096, vz=0.0),
        ]
        result = runner.validate_walking_samples(samples, _pos(0, 0.5, 0), _pos(0, 0.5, 0.096), OP_ID)
        self.assertFalse(result["pass"])

    def test_line_deviation_fails(self):
        samples = [
            _sample("active", 100, 2.0, 0.5, 1.0),
            _sample("active", 80, 2.0, 0.5, 3.0),
            _sample("active", 60, 2.0, 0.5, 5.0),
            _sample("active", 40, 2.0, 0.5, 7.0),
            _sample("active", 20, 2.0, 0.5, 9.0),
            _sample("active", 5, 2.0, 0.5, 11.0),
            _sample("completed", 0, 0.0, 0.5, 11.6, vz=0.0),
        ]
        result = runner.validate_walking_samples(samples, _pos(0, 0.5, 0), _pos(0, 0.5, 11.6), OP_ID)
        self.assertFalse(result["pass"])

    def test_only_exhausted_budget_active_samples_fail(self):
        samples = [
            _sample("active", 0, 0.0, 0.5, 11.6, vz=0.0),
            _sample("completed", 0, 0.0, 0.5, 11.6, vz=0.0),
        ]
        result = runner.validate_walking_samples(samples, _pos(0, 0.5, 0), _pos(0, 0.5, 11.6), OP_ID)
        self.assertFalse(result["pass"])
        self.assertEqual(result["activeSamples"], 0)

    def test_bad_speed_on_exhausted_boundary_ignored_but_finite_required(self):
        samples, start, end = _valid_walk_samples()
        samples[6]["velocity"] = _vel(99.0, 0.0, 99.0)
        self.assertTrue(runner.validate_walking_samples(samples, start, end, OP_ID)["pass"])
        samples[6]["velocity"] = _vel(float("nan"), 0.0, 0.0)
        self.assertFalse(runner.validate_walking_samples(samples, start, end, OP_ID)["pass"])

    def test_incomplete_operation_fails(self):
        samples, start, end = _valid_walk_samples()
        samples[-1] = _sample("active", 1, 0.0, 0.5, 11.6)
        self.assertFalse(runner.validate_walking_samples(samples, start, end, OP_ID)["pass"])

    def test_release_requires_several_stable_samples(self):
        anchor = _pos(0.0, 0.5, 11.612895)
        status = {"status": "completed", "remainingUpdates": 0, "requestId": OP_ID}
        ok = runner.validate_release_stable(_release_samples(anchor), status, OP_ID, anchor)
        self.assertTrue(ok["pass"], ok)
        # Single sample must fail
        self.assertFalse(runner.validate_release_stable(_release_samples(anchor)[:1], status, OP_ID, anchor)["pass"])
        # Drifting position fails
        drifting = _release_samples(anchor)
        drifting[2]["position"] = _pos(1.0, 0.5, 11.612895)
        self.assertFalse(runner.validate_release_stable(drifting, status, OP_ID, anchor)["pass"])
        # Non-neutral speed fails
        fast = _release_samples(anchor)
        fast[1]["velocity"] = _vel(1.0, 0.0, 0.0)
        self.assertFalse(runner.validate_release_stable(fast, status, OP_ID, anchor)["pass"])
        # Wrong operation id fails
        self.assertFalse(runner.validate_release_stable(_release_samples(anchor), status, OP_ID + 1, anchor)["pass"])

    def test_timeout_and_disconnect_not_accepted_as_rejection(self):
        self.assertFalse(runner.validate_rejection_response(None))
        self.assertFalse(runner.validate_rejection_response(TimeoutError("deadline")))
        self.assertFalse(runner.validate_rejection_response({"ok": True, "status": "queued"}))
        self.assertTrue(runner.validate_rejection_response({"ok": False, "error": "invalid_request"}))

    def test_crash_exit_and_shutdown_validation(self):
        self.assertFalse(runner.validate_shutdown(None))
        self.assertFalse(runner.validate_shutdown(3221225477))
        self.assertTrue(runner.validate_shutdown(0))

    def test_cli_args_reject_nonfinite_and_unbounded_before_spawn(self):
        self.assertIsNone(runner.validate_runner_args(_args()))
        self.assertIsNotNone(runner.validate_runner_args(_args(timeout=float("nan"))))
        self.assertIsNotNone(runner.validate_runner_args(_args(timeout=-1)))
        self.assertIsNotNone(runner.validate_runner_args(_args(timeout=0)))
        self.assertIsNotNone(runner.validate_runner_args(_args(timeout=31)))
        self.assertIsNotNone(runner.validate_runner_args(_args(poll_interval=0)))
        self.assertIsNotNone(runner.validate_runner_args(_args(poll_interval=1.5)))
        self.assertIsNotNone(runner.validate_runner_args(_args(settle=float("inf"))))
        self.assertIsNotNone(runner.validate_runner_args(_args(release_settle=True)))
        self.assertIsNotNone(runner.validate_runner_args(_args(ready_timeout=-5)))
        self.assertIsNotNone(runner.validate_runner_args(_args(shutdown_timeout=0)))

    def test_cli_failure_before_spawn_no_outdir(self):
        runner_py = str(HERE / "runtime_bridge_controller_acceptance.py")
        with tempfile.TemporaryDirectory() as td:
            temp_dir = pathlib.Path(td)
            dummy_exe = temp_dir / "dummy.exe"
            dummy_exe.write_bytes(b"MZ\x00\x00")
            target_out = temp_dir / "out"
            res = subprocess.run(
                [sys.executable, runner_py, "--exe", str(dummy_exe), "--out-dir", str(target_out),
                 "--timeout", "nan"],
                capture_output=True,
                text=True,
            )
            self.assertNotEqual(res.returncode, 0)
            self.assertFalse(target_out.exists())

            res = subprocess.run(
                [sys.executable, runner_py, "--exe", str(dummy_exe), "--out-dir", str(target_out),
                 "--poll-interval", "-0.1"],
                capture_output=True,
                text=True,
            )
            self.assertNotEqual(res.returncode, 0)
            self.assertFalse(target_out.exists())

            nonexistent_exe = temp_dir / "missing.exe"
            res = subprocess.run(
                [sys.executable, runner_py, "--exe", str(nonexistent_exe), "--out-dir", str(target_out)],
                capture_output=True,
                text=True,
            )
            self.assertNotEqual(res.returncode, 0)
            self.assertFalse(target_out.exists())

            existing_out = temp_dir / "existing_out"
            existing_out.mkdir()
            res = subprocess.run(
                [sys.executable, runner_py, "--exe", str(dummy_exe), "--out-dir", str(existing_out)],
                capture_output=True,
                text=True,
            )
            self.assertNotEqual(res.returncode, 0)

    def test_png_validation_negative(self):
        valid = make_dummy_png(1920, 1080, 5000)
        self.assertTrue(runner.validate_png_bytes(valid)["pass"])
        self.assertFalse(runner.validate_png_bytes(valid[:16])["pass"])

    def test_import_runner_safety(self):
        script = "import runtime_bridge_controller_acceptance; print('IMPORT_SAFE')"
        res = subprocess.run([sys.executable, "-c", script], capture_output=True, text=True, cwd=str(HERE))
        self.assertEqual(res.returncode, 0)
        self.assertIn("IMPORT_SAFE", res.stdout)

    def test_helpers_wired_into_public_api(self):
        # Gates the runner calls must remain importable and callable.
        for name in (
            "validate_runner_args",
            "validate_lifecycle_sample",
            "validate_walking_samples",
            "validate_release_stable",
            "is_active_hold_sample",
        ):
            self.assertTrue(callable(getattr(runner, name)))


if __name__ == "__main__":
    unittest.main()
