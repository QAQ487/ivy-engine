"""Pure unit tests for Phase-6 explore host (invalid command / observe / poll / CLI)."""
import json
import pathlib
import subprocess
import sys
import tempfile
import time
import unittest

HERE = pathlib.Path(__file__).resolve().parent
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

import runtime_bridge_explore as host


def _args(**overrides):
    import argparse
    ns = argparse.Namespace(
        exe="C:/dummy/IvyEngineRuntime.exe",
        out_dir="C:/dummy/explore-out",
        expected_scene="scene_runtime_bridge_player.json",
        lifetime=180.0,
        idle_deadline=60.0,
        max_actions=12,
        ready_timeout=25.0,
        shutdown_timeout=15.0,
        poll_interval=0.05,
    )
    for k, v in overrides.items():
        setattr(ns, k, v)
    return ns


def _obs(found=True, x=0.0, y=0.5, z=0.0, vx=0.0, vy=0.0, vz=0.0):
    return {
        "found": found,
        "position": {"x": x, "y": y, "z": z},
        "velocity": {"x": vx, "y": vy, "z": vz},
    }


def _all_obs():
    return {name: _obs() for name in host.TARGETS}


class FakeLink:
    def __init__(self, statuses):
        self.statuses = list(statuses)
        self.i = 0

    def req(self, verb, payload=None):
        if verb != "status":
            raise AssertionError("unexpected verb %s" % verb)
        if self.i >= len(self.statuses):
            return self.statuses[-1]
        item = self.statuses[self.i]
        self.i += 1
        return item


class FakeChild:
    def __init__(self, alive=True):
        self._alive = alive
        self.returncode = None if alive else 1

    def poll(self):
        return None if self._alive else self.returncode


class KeepaliveLink:
    def __init__(self, fail_after=None, clock=None):
        self.calls = []
        self.fail_after = fail_after
        self.clock = clock

    def req(self, verb, payload=None):
        stamp = self.clock() if self.clock else time.time()
        self.calls.append((verb, stamp))
        if self.fail_after is not None and len(self.calls) > self.fail_after:
            raise OSError(233, "pipe broken")
        if verb != "status":
            raise AssertionError("keepalive must use status only")
        return {"ok": True, "ready": True, "playing": True}


class ExploreHostValidationTests(unittest.TestCase):
    def test_valid_commands(self):
        ok, err, cmd = host.validate_command(
            {"sequence": 1, "action": "controller", "lx": 0.0, "ly": 1.0, "updates": 30}, 1)
        self.assertTrue(ok, err)
        ok, err, cmd = host.validate_command({"sequence": 2, "action": "capture"}, 2)
        self.assertTrue(ok, err)
        ok, err, cmd = host.validate_command({"sequence": 3, "action": "quit"}, 3)
        self.assertTrue(ok, err)

    def test_events_action_schema_and_reply_validation(self):
        ok, err, cmd = host.validate_command({"sequence": 1, "action": "events"}, 1)
        self.assertTrue(ok, err)
        self.assertEqual(cmd["action"], "events")
        self.assertFalse(host.validate_command(
            {"sequence": 1, "action": "events", "events": [{"sequence": 1}]}, 1)[0])
        empty = {"ok": True, "protocolVersion": 1, "events": [], "latestSequence": 0, "dropped": 0}
        self.assertTrue(host.validate_events_reply(empty)[0])
        good = {
            "ok": True, "protocolVersion": 1, "runId": "x", "requestId": 3,
            "events": [{"sequence": 1, "type": "checkpoint_reached",
                        "checkpoint": "Checkpoint_North", "actor": "Player", "score": 1}],
            "latestSequence": 1, "dropped": 0,
        }
        self.assertTrue(host.validate_events_reply(good)[0])
        # Parent-observed RED payload must stay rejected.
        parent_red = {
            "ok": True, "protocolVersion": 1,
            "events": [{"sequence": 999, "type": "checkpoint_reached",
                        "checkpoint": "Checkpoint_North", "actor": "Player", "score": -1}],
            "latestSequence": 0, "dropped": 0,
        }
        self.assertFalse(host.validate_events_reply(parent_red)[0])
        bad_cases = [
            {"ok": True, "protocolVersion": 1, "events": "x", "latestSequence": 0, "dropped": 0},
            {"ok": True, "protocolVersion": 1, "events": [], "latestSequence": True, "dropped": 0},
            {"ok": True, "protocolVersion": 1, "events": [], "latestSequence": 1, "dropped": 0},
            {"ok": True, "protocolVersion": 1, "events": [{"sequence": 0, "type": "checkpoint_reached",
                "checkpoint": "Checkpoint_North", "actor": "Player", "score": 1}],
             "latestSequence": 0, "dropped": 0},
            {"ok": True, "protocolVersion": 1, "events": [
                {"sequence": 1, "type": "checkpoint_reached", "checkpoint": "Checkpoint_North",
                 "actor": "Player", "score": 1},
                {"sequence": 1, "type": "checkpoint_reached", "checkpoint": "Checkpoint_North",
                 "actor": "Player", "score": 1}], "latestSequence": 1, "dropped": 0},
            {"ok": True, "protocolVersion": 1, "events": [
                {"sequence": 1, "type": "checkpoint_reached", "checkpoint": "Checkpoint_North",
                 "actor": "Player", "score": 1},
                {"sequence": 3, "type": "checkpoint_reached", "checkpoint": "Checkpoint_North",
                 "actor": "Player", "score": 1}], "latestSequence": 3, "dropped": 0},
            {"ok": True, "protocolVersion": 1, "events": [{"sequence": 1, "type": "other",
                "checkpoint": "Checkpoint_North", "actor": "Player", "score": 1}],
             "latestSequence": 1, "dropped": 0},
            {"ok": True, "protocolVersion": 1, "events": [{"sequence": 1, "type": "checkpoint_reached",
                "checkpoint": "Checkpoint_North", "actor": "Enemy", "score": 1}],
             "latestSequence": 1, "dropped": 0},
            {"ok": True, "protocolVersion": 1, "events": [{"sequence": 1, "type": "checkpoint_reached",
                "checkpoint": "bad name!", "actor": "Player", "score": 1}],
             "latestSequence": 1, "dropped": 0},
            {"ok": True, "protocolVersion": 1, "events": [{"sequence": 1, "type": "checkpoint_reached",
                "checkpoint": "Checkpoint_North", "actor": "Player", "score": True}],
             "latestSequence": 1, "dropped": 0},
            {"ok": True, "protocolVersion": 1, "events": [{"sequence": 1, "type": "checkpoint_reached",
                "checkpoint": "Checkpoint_North", "actor": "Player", "score": 1}],
             "latestSequence": 2, "dropped": 0},
            {"ok": True, "protocolVersion": 1,
             "events": [{"sequence": i + 1, "type": "checkpoint_reached",
                         "checkpoint": "Checkpoint_North", "actor": "Player", "score": 1}
                        for i in range(65)],
             "latestSequence": 65, "dropped": 0},
            {"ok": False, "events": [], "latestSequence": 0, "dropped": 0},
        ]
        for bad in bad_cases:
            self.assertFalse(host.validate_events_reply(bad)[0], bad)

    def test_invalid_payloads_rejected_no_raise(self):
        cases = [
            ({"sequence": 2, "action": "controller", "lx": 0, "ly": 1, "updates": 1}, 1),
            ({"sequence": 1, "action": "keyboard", "keys": ["W"]}, 1),
            ({"sequence": 1, "action": "controller", "lx": float("nan"), "ly": 1, "updates": 1}, 1),
            ({"sequence": 1, "action": "controller", "lx": True, "ly": 1, "updates": 1}, 1),
            ({"sequence": 1, "action": "controller", "lx": 0, "ly": False, "updates": 1}, 1),
            ({"sequence": 1, "action": "controller", "lx": 0, "ly": 1, "updates": 0}, 1),
            ({"sequence": 1, "action": "controller", "lx": 0, "ly": 1, "updates": 61}, 1),
            ({"sequence": 1, "action": "controller", "lx": 0, "ly": 1, "updates": 1, "extra": 1}, 1),
            ({"sequence": 1, "action": "capture", "target": "Player"}, 1),
            ({"sequence": 1, "action": ["quit"]}, 1),
            ({"sequence": 1, "action": {"name": "quit"}}, 1),
            ({"sequence": 1, "action": "exec", "code": "x"}, 1),
            ("not-an-object", 1),
            (None, 1),
            (["sequence", 1], 1),
        ]
        for payload, seq in cases:
            with self.subTest(payload=payload):
                ok, err, cmd = host.validate_command(payload, seq)
                self.assertFalse(ok)
                self.assertIsNone(cmd)
                self.assertTrue(err)

    def test_observations_require_found_and_finite(self):
        good = _all_obs()
        self.assertTrue(host.validate_observations(good)[0])
        missing = dict(good)
        del missing["Landmark_East"]
        self.assertFalse(host.validate_observations(missing)[0])
        bad_found = _all_obs()
        bad_found["Player"]["found"] = False
        self.assertFalse(host.validate_observations(bad_found)[0])
        nan = _all_obs()
        nan["Landmark_North"]["position"]["x"] = float("nan")
        self.assertFalse(host.validate_observations(nan)[0])
        bool_v = _all_obs()
        bool_v["Landmark_South"]["velocity"]["z"] = True
        self.assertFalse(host.validate_observations(bool_v)[0])
        incomplete = _all_obs()
        incomplete["Landmark_West"]["velocity"] = {"x": 0.0, "y": 0.0}
        self.assertFalse(host.validate_observations(incomplete)[0])

    def test_read_command_file_oversize(self):
        with tempfile.TemporaryDirectory() as td:
            path = pathlib.Path(td) / "command-1.json"
            path.write_bytes(b"x" * (host.MAX_COMMAND_BYTES + 1))
            ok, err, data = host.read_command_file(path)
            self.assertFalse(ok)
            self.assertIsNone(data)
            self.assertIn("exceeds", err)
            path.write_bytes(b'{"sequence":1,"action":"quit"}')
            ok, err, data = host.read_command_file(path)
            self.assertTrue(ok, err)
            self.assertTrue(data.startswith(b"{"))

    def test_poll_controller_fake_link(self):
        op = 7
        done = [
            {"ok": True, "controller": {"requestId": op, "status": "active", "remainingUpdates": 2}},
            {"ok": True, "controller": {"requestId": op, "status": "completed", "remainingUpdates": 0}},
        ]
        ok, err, info = host.poll_controller(FakeLink(done), op, time.monotonic() + 2, 0.0)
        self.assertTrue(ok, err)
        self.assertEqual(info["status"], "completed")

        mismatch = [{"ok": True, "controller": {"requestId": 99, "status": "active", "remainingUpdates": 1}}]
        ok, err, info = host.poll_controller(FakeLink(mismatch), op, time.monotonic() + 1, 0.0)
        self.assertFalse(ok)
        self.assertIn("requestId", err)

        cancelled = [{"ok": True, "controller": {"requestId": op, "status": "cancelled", "remainingUpdates": 0}}]
        ok, err, info = host.poll_controller(FakeLink(cancelled), op, time.monotonic() + 1, 0.0)
        self.assertFalse(ok)
        self.assertIn("cancelled", err)

        bad_rem = [{"ok": True, "controller": {"requestId": op, "status": "active", "remainingUpdates": -1}}]
        ok, err, info = host.poll_controller(FakeLink(bad_rem), op, time.monotonic() + 1, 0.0)
        self.assertFalse(ok)
        self.assertIn("remaining", err)

        bad_st = [{"ok": True, "controller": {"requestId": op, "status": "weird", "remainingUpdates": 1}}]
        ok, err, info = host.poll_controller(FakeLink(bad_st), op, time.monotonic() + 1, 0.0)
        self.assertFalse(ok)

        not_ok = [{"ok": False, "controller": {"requestId": op, "status": "active", "remainingUpdates": 1}}]
        ok, err, info = host.poll_controller(FakeLink(not_ok), op, time.monotonic() + 1, 0.0)
        self.assertFalse(ok)

        stall = [{"ok": True, "controller": {"requestId": op, "status": "active", "remainingUpdates": 1}}]
        ok, err, info = host.poll_controller(FakeLink(stall), op, time.monotonic() + 0.05, 0.02)
        self.assertFalse(ok)
        self.assertIn("deadline", err)

    def test_wait_keepalive_schedule_no_idle_reset(self):
        with tempfile.TemporaryDirectory() as td:
            cmd_path = pathlib.Path(td) / "command-1.json"
            child = FakeChild(alive=True)
            clock = {"t": 100.0}

            def mono():
                return clock["t"]

            def sleep(dt):
                clock["t"] += max(dt, 0.0)

            link = KeepaliveLink(clock=mono)
            last_cmd_at = 100.0
            # >2s parent latency, no command file: keepalives every <=0.5s; idle marker untouched.
            event, detail = host.wait_for_command(
                cmd_path, child, link, last_cmd_at, idle_deadline=60.0, lifetime_deadline=100.0 + 5.0,
                poll_interval=0.05, monotonic=mono, sleep=sleep)
            self.assertEqual(event, "lifetime")
            self.assertGreaterEqual(len(link.calls), 5)
            gaps = [link.calls[i][1] - link.calls[i - 1][1] for i in range(1, len(link.calls))]
            self.assertTrue(gaps)
            self.assertTrue(all(g <= host.KEEPALIVE_INTERVAL + 1e-9 for g in gaps))
            self.assertLessEqual(host.KEEPALIVE_INTERVAL, 0.5)
            self.assertEqual(last_cmd_at, 100.0)

            link2 = KeepaliveLink(fail_after=0, clock=mono)
            clock["t"] = 200.0
            event, detail = host.wait_for_command(
                cmd_path, child, link2, 200.0, 60.0, 200.0 + 10.0, 0.05, mono, sleep)
            self.assertEqual(event, "keepalive-fail")
            self.assertIn("keepalive", detail)

            cmd_path.write_text('{"sequence":1,"action":"quit"}', encoding="utf-8")
            link3 = KeepaliveLink(clock=mono)
            clock["t"] = 300.0
            last = 300.0
            event, detail = host.wait_for_command(
                cmd_path, child, link3, last, 60.0, 300.0 + 10.0, 0.05, mono, sleep)
            self.assertEqual(event, "ready")
            self.assertEqual(last, 300.0)

    def test_finalize_quit_waits_before_close(self):
        events = []

        class TrackingChild:
            def __init__(self, exit_after_waits=1, code=0):
                self._waits = 0
                self._exit_after = exit_after_waits
                self.returncode = None
                self._code = code

            def poll(self):
                return self.returncode

            def wait(self, timeout=None):
                events.append(("wait", timeout, "link_open" if not link.closed else "link_closed"))
                if self.returncode is not None:
                    return self.returncode
                self._waits += 1
                if self._waits >= self._exit_after:
                    self.returncode = self._code
                    return self._code
                raise subprocess.TimeoutExpired(cmd="fake", timeout=timeout)

            def kill(self):
                events.append(("kill", None, "link_open" if not link.closed else "link_closed"))
                self.returncode = 1

        class TrackingLink:
            def __init__(self):
                self.closed = False

            def close(self):
                events.append(("close", None, None))
                self.closed = True

        link = TrackingLink()
        child = TrackingChild(exit_after_waits=1, code=0)
        summary = {"trace": [{"sequence": 1, "action": "quit", "ok": False, "error": None}], "handled": 1}
        reply = {"schema": "x", "sequence": 1, "ok": False, "error": None, "action": "quit", "observations": None}
        with tempfile.TemporaryDirectory() as td:
            out = pathlib.Path(td)
            code, child_out, link_out = host.finalize_quit(
                child, link, summary, time.monotonic(), 1.0, out, reply, 1,
                out / "command-1.json", True, None)
            self.assertEqual(code, 0)
            self.assertTrue(reply["ok"])
            self.assertEqual(events[0][0], "wait")
            self.assertEqual(events[0][2], "link_open")
            self.assertIn("close", [e[0] for e in events])
            self.assertGreater(events.index(("close", None, None)), 0)
            self.assertFalse(summary["shutdown"]["timedOut"])
            self.assertFalse(summary["shutdown"]["forcedKill"])
            self.assertEqual(summary["shutdown"]["rawExitCode"], 0)
            self.assertIsNone(link_out)

        # Timeout → forcedKill; still wait before close; do not call it a runtime crash.
        events.clear()
        link = TrackingLink()
        child = TrackingChild(exit_after_waits=99, code=0)
        summary = {"trace": [{"sequence": 2, "action": "quit", "ok": False, "error": None}], "handled": 2}
        reply = {"schema": "x", "sequence": 2, "ok": False, "error": None, "action": "quit", "observations": None}
        with tempfile.TemporaryDirectory() as td:
            out = pathlib.Path(td)
            code, child_out, link_out = host.finalize_quit(
                child, link, summary, time.monotonic(), 0.01, out, reply, 2,
                out / "command-2.json", True, None)
            self.assertNotEqual(code, 0)
            self.assertFalse(reply["ok"])
            self.assertTrue(summary["shutdown"]["timedOut"])
            self.assertTrue(summary["shutdown"]["forcedKill"])
            self.assertEqual(summary["shutdown"]["rawExitCode"], 1)
            self.assertIn("force-killed", reply["error"])
            self.assertNotIn("crash", reply["error"].lower())
            self.assertEqual(events[0][0], "wait")
            self.assertEqual(events[0][2], "link_open")
            self.assertIn(("kill", None, "link_open"), events)
            self.assertTrue(events.index(("close", None, None)) > events.index(("kill", None, "link_open")))

    def test_cli_bounds_and_pre_spawn(self):
        self.assertIsNone(host.validate_explore_args(_args()))
        self.assertIsNotNone(host.validate_explore_args(_args(lifetime=241)))
        self.assertIsNotNone(host.validate_explore_args(_args(idle_deadline=91)))
        self.assertIsNotNone(host.validate_explore_args(_args(max_actions=13)))
        runner_py = str(HERE / "runtime_bridge_explore.py")
        with tempfile.TemporaryDirectory() as td:
            td_path = pathlib.Path(td)
            missing = td_path / "missing.exe"
            out = td_path / "out"
            res = subprocess.run(
                [sys.executable, runner_py, "--exe", str(missing), "--out-dir", str(out), "--lifetime", "nan"],
                capture_output=True, text=True)
            self.assertNotEqual(res.returncode, 0)
            self.assertFalse(out.exists())

    def test_atomic_write_json(self):
        with tempfile.TemporaryDirectory() as td:
            path = pathlib.Path(td) / "ready.json"
            host.atomic_write_json(path, {"ok": True, "n": 1})
            self.assertEqual(json.loads(path.read_text(encoding="utf-8"))["n"], 1)

    def test_import_safe(self):
        res = subprocess.run(
            [sys.executable, "-c", "import runtime_bridge_explore as h; print('IMPORT_SAFE')"],
            capture_output=True, text=True, cwd=str(HERE))
        self.assertEqual(res.returncode, 0)
        self.assertIn("IMPORT_SAFE", res.stdout)


if __name__ == "__main__":
    unittest.main()
