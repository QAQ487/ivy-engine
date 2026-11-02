"""Windows runtime acceptance for the controller consumer-probe path.

Stages the isolated probe fixture, launches Debug GAME_RUNTIME with the stock
runtime_test_client, and asserts per-Update getter observations from
controller_probe frames inside each operation's sequence window
(latestSequence before → frames after). Does not mock traces or rely on
client polling of single-Update edges.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import pathlib
import shutil
import subprocess
import sys
import time
import traceback
import uuid

HERE = pathlib.Path(__file__).resolve().parent
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

from runtime_bridge_controller_probe import (
    BUTTONS,
    RESPONSE_BYTE_LIMIT,
    find_button_window,
    find_combined_window,
    find_right_stick_window,
    find_trigger_window,
    frames_after_sequence,
    max_controller_probe_response_bytes,
    validate_eventual_neutral_suffix,
    validate_probe_reply,
)

ENV_CLEAR = (
    "IVY_FRAME_CAPTURE",
    "IVY_DISABLE_RENDER_THREAD",
    "IVY_PLAY_SCENE_SWITCH_CHECK",
    "IVY_SCENE_HOT_LOAD_CHECK",
    "IVY_RUNTIME_TEST_BRIDGE",
    "IVY_RUNTIME_TEST_RUN_ID",
    "IVY_RUNTIME_TEST_OUTPUT_DIR",
    "IVY_RUNTIME_TEST_FIXED_CAMERA",
)

DEFAULT_FIXTURE = HERE.parent / "testbed" / "rom" / "scene" / "scene_runtime_bridge_controller_probe.json"
EXPECTED_SCENE = "scene_runtime_bridge_controller_probe.json"

CLI_BOUNDS = {
    "settle": (0.0, 5.0),
    "ready_timeout": (0.0, 60.0),
    "shutdown_timeout": (0.0, 60.0),
    "completion_timeout": (0.0, 30.0),
}


def validate_runner_args(args):
    if not isinstance(getattr(args, "exe", None), str) or not args.exe:
        return "exe path required"
    if not isinstance(getattr(args, "out_dir", None), str) or not args.out_dir:
        return "out-dir required"
    scene = getattr(args, "fixture_scene", None)
    if not isinstance(scene, str) or not scene or len(scene) > 512:
        return "fixture-scene must be a non-empty string <= 512 chars"
    for name, (lo, hi) in CLI_BOUNDS.items():
        value = getattr(args, name, None)
        if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
            return "%s must be a finite number" % name
        if not (lo < float(value) <= hi):
            return "%s must be in (%g,%g]" % (name, lo, hi)
    return None


class ProbeLink:
    """Stock Client wrapper with a run-wide monotonic request id."""

    def __init__(self, pid, run_id, report, completion_timeout):
        from runtime_test_client import Client
        self.Client = Client
        self.pid = pid
        self.run_id = run_id
        self.report = report
        self.completion_timeout = completion_timeout
        self.client = Client(pid, run_id, timeout=5)
        self.counter = 0

    def _next(self):
        self.counter += 1
        return self.counter

    def _adopt(self, reply):
        rid = reply.get("requestId")
        if type(rid) is int:
            self.counter = max(self.counter, rid)
        return reply

    def req(self, verb, payload=None):
        for attempt in (0, 1):
            request_id = self._next()
            try:
                return self._adopt(self.client.request(verb, request_id, payload))
            except OSError as exc:
                self.report.setdefault("reconnects", []).append(
                    {"t": round(time.monotonic() - self.report["startedAt"], 3), "error": str(exc)})
                try:
                    self.client.close()
                except Exception:
                    pass
                self.client = self.Client(self.pid, self.run_id, timeout=5)
                if attempt:
                    raise
        raise RuntimeError("unreachable")

    def controller(self, lx, ly, updates, **axes):
        request_id = self._next()
        reply = self.client.controller(
            lx, ly, updates, request_id,
            completion_timeout=self.completion_timeout, **axes)
        return self._adopt(reply)

    def controller_probe(self):
        return self._adopt(self.client.controller_probe(self._next()))

    def close_for_cancel(self):
        try:
            self.client.close()
        except Exception:
            pass
        self.client = self.Client(self.pid, self.run_id, timeout=5)

    def close(self):
        try:
            self.client.close()
        except Exception:
            pass


def stage_fixture_package(exe, out_dir, fixture_scene):
    """Copy the Debug runtime tree and point default_scene at the probe fixture."""
    from runtime_bridge_mouse_acceptance import stage_fixture_scene_assets

    exe = pathlib.Path(exe).resolve()
    fixture_src = pathlib.Path(fixture_scene).resolve()
    if not fixture_src.is_file():
        raise SystemExit("fixture scene not found: %s" % fixture_src)
    package = out_dir / "fixture"
    if package.exists():
        raise SystemExit("fixture package already exists: %s" % package)
    shutil.copytree(exe.parent, package)
    staged_exe = package / exe.name
    if not staged_exe.is_file():
        raise SystemExit("staged exe missing: %s" % staged_exe)
    rom_scene = package / "rom" / "scene"
    rom_scene.mkdir(parents=True, exist_ok=True)
    shutil.copy2(fixture_src, rom_scene / EXPECTED_SCENE)
    (rom_scene / "default_scene.txt").write_text(EXPECTED_SCENE + "\n", encoding="ascii")
    stage_fixture_scene_assets(package, fixture_src)
    return staged_exe, package


def _assert_probe(reply, label):
    ok, err = validate_probe_reply(reply)
    if not ok:
        raise AssertionError("%s: %s" % (label, err))
    if reply.get("overflow") is True:
        raise AssertionError("%s: probe overflow" % label)


def run(args):
    arg_error = validate_runner_args(args)
    if arg_error:
        raise SystemExit(arg_error)

    from runtime_bridge_mouse_acceptance import session_of

    exe = pathlib.Path(args.exe).resolve()
    if not exe.is_file():
        raise SystemExit("exe not found: %s" % exe)

    out_dir = pathlib.Path(args.out_dir).resolve()
    if out_dir.exists():
        raise SystemExit("out dir already exists: %s" % out_dir)
    out_dir.mkdir(parents=True)

    staged_exe, package = stage_fixture_package(exe, out_dir, args.fixture_scene)
    launcher_session = session_of(os.getpid())
    report = {
        "schema": "ivy.runtime_bridge.controller_probe_acceptance/1",
        "startedAt": time.monotonic(),
        "exe": str(exe),
        "stagedExe": str(staged_exe),
        "exeSha256": hashlib.sha256(exe.read_bytes()).hexdigest(),
        "launcherSession": launcher_session,
        "responseByteLimit": RESPONSE_BYTE_LIMIT,
        "maxProbeResponseBytes": max_controller_probe_response_bytes(),
        "checks": {},
        "status": "FAIL",
    }
    child = None
    link = None
    try:
        run_id = uuid.uuid4().hex
        report["runId"] = run_id
        env = dict(os.environ)
        for key in ENV_CLEAR:
            env.pop(key, None)
        env.update({
            "IVY_RUNTIME_TEST_BRIDGE": "1",
            "IVY_RUNTIME_TEST_RUN_ID": run_id,
            "IVY_RUNTIME_TEST_OUTPUT_DIR": str(out_dir),
            "IVY_RUNTIME_TEST_FIXED_CAMERA": "1",
            "IVY_SKIP_CLOSE_CONFIRM": "1",
        })

        with (out_dir / "runtime-out.log").open("wb") as stdout, (out_dir / "runtime-err.log").open("wb") as stderr:
            child = subprocess.Popen(
                [str(staged_exe)], cwd=str(package), env=env, stdout=stdout, stderr=stderr)
        report["pid"] = child.pid
        report["childSession"] = session_of(child.pid)
        if report["childSession"] == 0 or launcher_session == 0:
            raise AssertionError("not running in an interactive session")

        link = ProbeLink(child.pid, run_id, report, args.completion_timeout)

        deadline = time.monotonic() + args.ready_timeout
        status = None
        while True:
            status = link.req("status")
            if status.get("ready") and status.get("playing"):
                break
            if time.monotonic() > deadline:
                raise AssertionError("runtime not ready within %.0fs" % args.ready_timeout)
            if child.poll() is not None:
                raise AssertionError("runtime exited before ready: %s" % child.returncode)
            time.sleep(0.05)

        capabilities = sorted(status.get("capabilities") or [])
        if "controller" not in capabilities or "controller_probe" not in capabilities:
            raise AssertionError("missing controller/controller_probe capability: %s" % capabilities)
        scene_name = pathlib.PurePosixPath(str(status.get("scene", "")).replace("\\", "/")).name
        if scene_name != EXPECTED_SCENE:
            raise AssertionError("unexpected scene %r (expected %r)" % (scene_name, EXPECTED_SCENE))
        report["checks"]["ready"] = {
            "pass": True,
            "scene": status.get("scene"),
            "capabilities": capabilities,
        }

        time.sleep(args.settle)
        executed = 0

        def record(name, detail):
            nonlocal executed
            executed += 1
            report["checks"][name] = detail

        # --- LT / RT exact inject values ---
        for value in (0.0, 0.25, 0.5, 1.0):
            before = link.controller_probe()
            _assert_probe(before, "before LT %.2f" % value)
            seq = before["latestSequence"]
            done = link.controller(0.0, 0.0, 2, lt=value)
            if done.get("controller", {}).get("status") != "completed":
                raise AssertionError("LT op not completed: %s" % done)
            after = link.controller_probe()
            _assert_probe(after, "after LT %.2f" % value)
            window = frames_after_sequence(after["frames"], seq)
            found, err = find_trigger_window(window, True, value, 2)
            if err:
                raise AssertionError("LT %.2f window: %s" % (value, err))
            record("lt_%.2f" % value, {"pass": True, "sequences": [f["sequence"] for f in found]})

            before = link.controller_probe()
            _assert_probe(before, "before RT %.2f" % value)
            seq = before["latestSequence"]
            done = link.controller(0.0, 0.0, 2, rt=value)
            if done.get("controller", {}).get("status") != "completed":
                raise AssertionError("RT op not completed: %s" % done)
            after = link.controller_probe()
            _assert_probe(after, "after RT %.2f" % value)
            window = frames_after_sequence(after["frames"], seq)
            found, err = find_trigger_window(window, False, value, 2)
            if err:
                raise AssertionError("RT %.2f window: %s" % (value, err))
            record("rt_%.2f" % value, {"pass": True, "sequences": [f["sequence"] for f in found]})

        # --- every supported button ---
        for button in BUTTONS:
            before = link.controller_probe()
            _assert_probe(before, "before button %s" % button)
            seq = before["latestSequence"]
            done = link.controller(0.0, 0.0, 2, buttons=[button])
            if done.get("controller", {}).get("status") != "completed":
                raise AssertionError("button %s not completed" % button)
            after = link.controller_probe()
            _assert_probe(after, "after button %s" % button)
            window = frames_after_sequence(after["frames"], seq)
            found, err = find_button_window(window, button, 2)
            if err:
                raise AssertionError("button %s window: %s" % (button, err))
            record("button_%s" % button, {"pass": True, "sequences": [f["sequence"] for f in found]})

        # --- combined triggers + buttons ---
        buttons = ["A", "LB", "DPAD_UP"]
        before = link.controller_probe()
        _assert_probe(before, "before combined")
        seq = before["latestSequence"]
        done = link.controller(0.0, 0.0, 2, lt=0.25, rt=1.0, buttons=buttons)
        if done.get("controller", {}).get("status") != "completed":
            raise AssertionError("combined op not completed")
        after = link.controller_probe()
        _assert_probe(after, "after combined")
        window = frames_after_sequence(after["frames"], seq)
        found, err = find_combined_window(window, 0.25, 1.0, buttons, 2)
        if err:
            raise AssertionError("combined window: %s" % err)
        record("combined", {"pass": True, "sequences": [f["sequence"] for f in found]})

        # --- right stick keeps left stick neutral ---
        before = link.controller_probe()
        _assert_probe(before, "before right-stick")
        seq = before["latestSequence"]
        done = link.controller(0.0, 0.0, 2, rx=0.5, ry=-1.0)
        if done.get("controller", {}).get("status") != "completed":
            raise AssertionError("right-stick op not completed")
        after = link.controller_probe()
        _assert_probe(after, "after right-stick")
        window = frames_after_sequence(after["frames"], seq)
        found, err = find_right_stick_window(window, 0.5, -1.0, 2)
        if err:
            raise AssertionError("right-stick window: %s" % err)
        record("right_stick", {"pass": True, "sequences": [f["sequence"] for f in found]})

        # --- disconnect cancel → eventual neutral ---
        before = link.controller_probe()
        _assert_probe(before, "before cancel")
        seq = before["latestSequence"]
        request_id = link._next()
        # Queue a long hold then drop the pipe (cancel), then reconnect.
        queued = link.client.request(
            "controller", request_id,
            {"lx": 0.0, "ly": 0.0, "lt": 1.0, "rt": 1.0, "buttons": ["B", "START"], "updates": 60})
        link._adopt(queued)
        if queued.get("ok") is not True:
            raise AssertionError("cancel op failed to queue: %s" % queued)
        time.sleep(0.15)  # allow at least one hold Update before disconnect
        link.close_for_cancel()
        settle_deadline = time.monotonic() + 3.0
        while time.monotonic() < settle_deadline:
            st = link.req("status")
            ctrl = st.get("controller") or {}
            if ctrl.get("status") in ("cancelled", "idle", "completed"):
                break
            time.sleep(0.05)
        else:
            raise AssertionError("controller did not settle after disconnect cancel")
        time.sleep(0.1)
        after = link.controller_probe()
        _assert_probe(after, "after cancel")
        window = frames_after_sequence(after["frames"], seq)
        ok, err = validate_eventual_neutral_suffix(window, 3)
        if not ok:
            raise AssertionError("cancel neutral suffix: %s" % err)
        record("disconnect_cancel", {"pass": True, "windowFrames": len(window)})

        # Full-capacity response must fit the stock client bound (smoke via probe).
        if RESPONSE_BYTE_LIMIT < max_controller_probe_response_bytes():
            raise AssertionError("RESPONSE_BYTE_LIMIT below worst-case probe size")
        record("response_bound", {
            "pass": True,
            "limit": RESPONSE_BYTE_LIMIT,
            "worstCase": max_controller_probe_response_bytes(),
        })

        report["checks"]["executedAssertions"] = executed
        if executed < 26:
            raise AssertionError("expected at least 26 consumer checks, got %d" % executed)

        quit_reply = link.req("quit")
        if quit_reply.get("ok") is not True:
            raise AssertionError("quit rejected: %s" % quit_reply)
        exit_code = child.wait(timeout=args.shutdown_timeout)
        report["shutdown"] = {"exitCode": exit_code, "pass": exit_code == 0}
        if exit_code != 0:
            raise AssertionError("shutdown not clean exit 0: %s" % exit_code)

        report["status"] = "PASS"
    except Exception:
        report["error"] = traceback.format_exc()
    finally:
        if link is not None:
            link.close()
        if child is not None and child.poll() is None:
            child.kill()
            child.wait(timeout=5)
        report["childCleanedUp"] = child is None or child.poll() is not None
        report["elapsedSeconds"] = round(time.monotonic() - report["startedAt"], 2)
        (out_dir / "result.json").write_text(
            json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
        summary_keys = (
            "status", "runId", "pid", "exeSha256", "checks", "shutdown",
            "error", "elapsedSeconds", "responseByteLimit", "maxProbeResponseBytes",
        )
        print(json.dumps({k: report[k] for k in summary_keys if k in report}, ensure_ascii=False, indent=2))

    return 0 if report["status"] == "PASS" else 1


def main():
    parser = argparse.ArgumentParser(
        description="Controller consumer-probe runtime acceptance for Ivy test bridge")
    parser.add_argument("--exe", required=True, help="Debug GAME_RUNTIME executable to stage and launch")
    parser.add_argument("--out-dir", required=True, help="new directory for fixture package, logs, result.json")
    parser.add_argument(
        "--fixture-scene",
        default=str(DEFAULT_FIXTURE),
        help="path to scene_runtime_bridge_controller_probe.json",
    )
    parser.add_argument("--settle", type=float, default=0.6)
    parser.add_argument("--ready-timeout", type=float, default=25.0)
    parser.add_argument("--shutdown-timeout", type=float, default=15.0)
    parser.add_argument("--completion-timeout", type=float, default=10.0)
    args = parser.parse_args()
    arg_error = validate_runner_args(args)
    if arg_error:
        raise SystemExit(arg_error)
    return run(args)


if __name__ == "__main__":
    raise SystemExit(main())
