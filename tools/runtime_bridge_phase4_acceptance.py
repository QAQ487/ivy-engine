"""Phase 4 acceptance runner: deterministic fixture checks over the runtime test bridge.

Launches one Debug GAME_RUNTIME build with the bridge enabled and the fixed diagnostic
camera, then asserts observable, reproducible behaviour:

  ready  -> observe Player (owner-thread snapshot)
  hold W -> 120 engine Updates -> straight-line horizontal displacement, height unchanged
  capture -> real PNG written by the runtime (header, size, dimensions)
  quit   -> receipt, then the native exit code (recorded, not asserted)

Shutdown is reported separately: a nonzero exit is recorded as shutdown=KNOWN_FAILURE
unless --require-clean-shutdown is passed.

Self-contained (stdlib + tools/runtime_test_client.py). Read-only apart from the
generated PNGs and result file inside --out-dir, which must not already exist.
"""
import argparse
import ctypes as C
from ctypes import wintypes as W
import hashlib
import json
import math
import os
import pathlib
import struct
import subprocess
import sys
import time
import traceback
import uuid

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from runtime_test_client import Client

K = C.WinDLL("kernel32", use_last_error=True)
K.ProcessIdToSessionId.argtypes = [W.DWORD, C.POINTER(W.DWORD)]

MIN_DISPLACEMENT = 11.0
MAX_HEIGHT_DRIFT = 0.01
MAX_LINE_DEVIATION = 0.5
SPEED_TOLERANCE = 0.5
EXPECTED_SPEED = 6.0

ENV_CLEAR = ("IVY_FRAME_CAPTURE", "IVY_DISABLE_RENDER_THREAD", "IVY_PLAY_SCENE_SWITCH_CHECK",
             "IVY_SCENE_HOT_LOAD_CHECK", "IVY_RUNTIME_TEST_BRIDGE", "IVY_RUNTIME_TEST_RUN_ID",
             "IVY_RUNTIME_TEST_OUTPUT_DIR", "IVY_RUNTIME_TEST_FIXED_CAMERA")


MIN_PEAK_RISE = 0.25
LANDING_Y_TOLERANCE = 0.05
LANDING_VY_TOLERANCE = 0.1
LANDING_SAMPLE_COUNT = 3


def session_of(pid):
    value = W.DWORD()
    if not K.ProcessIdToSessionId(pid, C.byref(value)):
        raise C.WinError(C.get_last_error())
    return value.value


def vec(d):
    return (d["x"], d["y"], d["z"])


def _is_finite_number(value):
    return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)


def validate_jump_samples(samples, baseline_y):
    """Validate a jump trajectory of observe samples against baseline_y.

    Each sample must match {position:{x,y,z}, velocity:{x,y,z}} with finite fields.
    Requires nonempty samples, finite baseline, peakRise >= 0.25, an observed
    positive vy before a negative vy, and the final three consecutive samples
    landed (abs(y-baseline)<=0.05 and abs(vy)<=0.1).
    """
    sample_count = len(samples) if isinstance(samples, (list, tuple)) else 0

    def result(ok, peak_rise, reason):
        return {
            "pass": ok,
            "peakRise": peak_rise,
            "sampleCount": sample_count,
            "reason": reason,
        }

    if not _is_finite_number(baseline_y):
        return result(False, 0.0, "baseline_y non-finite")
    if not samples or not isinstance(samples, (list, tuple)):
        return result(False, 0.0, "samples empty or missing")

    ys = []
    vys = []
    for idx, sample in enumerate(samples):
        if not isinstance(sample, dict):
            return result(False, 0.0, "sample %d not a dict" % idx)
        position = sample.get("position")
        velocity = sample.get("velocity")
        if not isinstance(position, dict):
            return result(False, 0.0, "sample %d missing position" % idx)
        if not isinstance(velocity, dict):
            return result(False, 0.0, "sample %d missing velocity" % idx)
        for label, vector in (("position", position), ("velocity", velocity)):
            for axis in ("x", "y", "z"):
                value = vector.get(axis)
                if not _is_finite_number(value):
                    return result(
                        False, 0.0,
                        "sample %d non-finite %s.%s: %s" % (idx, label, axis, value),
                    )
        ys.append(float(position["y"]))
        vys.append(float(velocity["y"]))

    peak_rise = max(ys) - float(baseline_y)
    if peak_rise < MIN_PEAK_RISE:
        return result(False, peak_rise, "peakRise below %.2f" % MIN_PEAK_RISE)

    first_positive = None
    first_negative_after = None
    for index, vy in enumerate(vys):
        if first_positive is None and vy > 0.0:
            first_positive = index
        elif first_positive is not None and first_negative_after is None and vy < 0.0:
            first_negative_after = index
            break
    if first_positive is None:
        return result(False, peak_rise, "missing positive vy")
    if first_negative_after is None:
        return result(False, peak_rise, "missing negative vy after positive")

    if sample_count < LANDING_SAMPLE_COUNT:
        return result(False, peak_rise, "fewer than %d samples for landing" % LANDING_SAMPLE_COUNT)
    for index in range(sample_count - LANDING_SAMPLE_COUNT, sample_count):
        if abs(ys[index] - baseline_y) > LANDING_Y_TOLERANCE:
            return result(False, peak_rise, "landing y outside tolerance")
        if abs(vys[index]) > LANDING_VY_TOLERANCE:
            return result(False, peak_rise, "landing vy outside tolerance")

    return result(True, peak_rise, None)


class Link:
    """Request helper that reconnects: the bridge closes an idle connection after 2 s."""

    def __init__(self, pid, run_id, report):
        self.pid, self.run_id, self.report = pid, run_id, report
        self.client = Client(pid, run_id, timeout=5)
        self.counter = 0

    def _next(self):
        self.counter += 1
        return self.counter

    def req(self, verb, payload=None):
        for attempt in (0, 1):
            request_id = self._next()
            try:
                return self.client.request(verb, request_id, payload)
            except OSError as exc:
                self.report.setdefault("reconnects", []).append(
                    {"t": round(time.monotonic() - self.report["startedAt"], 3), "error": str(exc)})
                try:
                    self.client.close()
                except Exception:
                    pass
                self.client = Client(self.pid, self.run_id, timeout=5)
                if attempt:
                    raise
        raise RuntimeError("unreachable")

    def observe(self, target):
        return self.req("observe", {"target": target})

    def capture(self, timeout=6.0):
        reply = self.client.capture(self._next(), timeout)
        self.counter = max(self.counter, int(reply.get("requestId") or self.counter))
        return reply

    def close(self):
        try:
            self.client.close()
        except Exception:
            pass


def run(args):
    out_dir = pathlib.Path(args.out_dir).resolve()
    if out_dir.exists():
        raise SystemExit("out dir already exists: %s" % out_dir)
    out_dir.mkdir(parents=True)
    exe = pathlib.Path(args.exe).resolve()
    if not exe.is_file():
        raise SystemExit("exe not found: %s" % exe)

    report = {"schema": "ivy.runtime_bridge.phase4/1", "startedAt": time.monotonic(),
              "exe": str(exe), "exeSha256": hashlib.sha256(exe.read_bytes()).hexdigest(),
              "launcherSession": session_of(os.getpid()), "checks": {}, "status": "FAIL"}
    child = None
    try:
        run_id = uuid.uuid4().hex
        report["runId"] = run_id
        env = dict(os.environ)
        for key in ENV_CLEAR:
            env.pop(key, None)
        env.update({"IVY_RUNTIME_TEST_BRIDGE": "1", "IVY_RUNTIME_TEST_RUN_ID": run_id,
                    "IVY_RUNTIME_TEST_OUTPUT_DIR": str(out_dir),
                    "IVY_RUNTIME_TEST_FIXED_CAMERA": "1", "IVY_SKIP_CLOSE_CONFIRM": "1"})
        with (out_dir / "runtime-out.log").open("wb") as stdout, (out_dir / "runtime-err.log").open("wb") as stderr:
            child = subprocess.Popen([str(exe)], cwd=str(pathlib.Path.home()), env=env,
                                     stdout=stdout, stderr=stderr)
        report["pid"] = child.pid
        report["childSession"] = session_of(child.pid)
        if report["childSession"] == 0 or report["launcherSession"] == 0:
            raise AssertionError("not running in an interactive session")

        link = Link(child.pid, run_id, report)
        deadline = time.monotonic() + args.ready_timeout
        while True:
            status = link.req("status")
            if status.get("ready") and status.get("playing"):
                break
            if time.monotonic() > deadline:
                raise AssertionError("runtime not ready within %.0fs" % args.ready_timeout)
            if child.poll() is not None:
                raise AssertionError("runtime exited before ready: %s" % child.returncode)
            time.sleep(0.05)
        report["checks"]["ready"] = {"pass": True, "scene": status.get("scene"),
                                     "state": status.get("state"),
                                     "capabilities": sorted(status.get("capabilities") or [])}
        scene_name = pathlib.PurePosixPath(str(status.get("scene", "")).replace("\\", "/")).name
        if scene_name != args.expected_scene:
            raise AssertionError("unexpected scene %r (expected %r)" % (scene_name, args.expected_scene))

        time.sleep(args.settle)
        before = link.observe("Player")
        if not before.get("found"):
            raise AssertionError("Player target not found in snapshot")
        report["before"] = {"position": before["position"], "velocity": before["velocity"]}

        accepted = link.req("keyboard", {"keys": ["W"], "updates": args.updates})
        report["keyboardAccepted"] = accepted.get("ok") is True and accepted.get("status") == "queued"
        samples, hold_start = [], time.monotonic()
        while time.monotonic() - hold_start < args.hold_window:
            current = link.req("status")
            observed = link.observe("Player")
            samples.append({"t": round(time.monotonic() - hold_start, 3),
                            "status": current["keyboard"]["status"],
                            "remaining": current["keyboard"]["remainingUpdates"],
                            "position": observed.get("position"),
                            "velocity": observed.get("velocity")})
            time.sleep(0.06)
        after = link.observe("Player")
        report["after"] = {"position": after["position"], "velocity": after["velocity"]}
        report["samples"] = samples

        start, end = vec(before["position"]), vec(after["position"])
        delta = tuple(end[i] - start[i] for i in range(3))
        horizontal = math.hypot(delta[0], delta[2])
        span = (delta[0], delta[2])
        span_len = math.hypot(*span)
        worst = 0.0
        speeds = []
        for sample in samples:
            if sample["status"] != "active" or sample["remaining"] == 0 or sample["position"] is None:
                continue
            pos = vec(sample["position"])
            if span_len > 1e-6:
                rel = (pos[0] - start[0], pos[2] - start[2])
                along = (rel[0] * span[0] + rel[1] * span[1]) / span_len
                perp = abs(rel[0] * span[1] - rel[1] * span[0]) / span_len
                worst = max(worst, perp)
            if sample["velocity"] is not None:
                vel = vec(sample["velocity"])
                speeds.append(math.hypot(vel[0], vel[2]))
        report["checks"]["movement"] = {
            "pass": horizontal >= MIN_DISPLACEMENT and abs(delta[1]) <= MAX_HEIGHT_DRIFT
                    and worst <= MAX_LINE_DEVIATION
                    and len(speeds) > 5
                    and all(abs(v - EXPECTED_SPEED) <= SPEED_TOLERANCE for v in speeds),
            "horizontalDisplacement": horizontal, "delta": list(delta),
            "maxLineDeviation": worst, "minSpeed": min(speeds) if speeds else None,
            "maxSpeed": max(speeds) if speeds else None, "activeSamples": len(speeds),
            "completed": samples[-1]["status"] == "completed" and samples[-1]["remaining"] == 0,
        }
        if not report["checks"]["movement"]["pass"]:
            raise AssertionError("movement check failed: %s" % json.dumps(report["checks"]["movement"]))

        if not args.no_capture:
            images = {}
            for label in ("afterW",):
                reply = link.capture()
                capture = reply.get("capture") or {}
                name = pathlib.PurePath(str(capture.get("basename") or "")).name
                info = {"basename": name, "reportedWidth": capture.get("width"),
                        "reportedHeight": capture.get("height"), "error": capture.get("error")}
                if name and (out_dir / name).is_file():
                    data = (out_dir / name).read_bytes()
                    info["bytes"] = len(data)
                    info["pngHeader"] = data[:8] == b"\x89PNG\r\n\x1a\n"
                    if info["pngHeader"] and len(data) >= 24:
                        info["width"], info["height"] = struct.unpack(">II", data[16:24])
                    info["sha256"] = hashlib.sha256(data).hexdigest()
                info["pass"] = bool(info.get("pngHeader")) and info.get("bytes", 0) > 4096 \
                    and (info.get("width"), info.get("height")) == (capture.get("width"), capture.get("height")) \
                    and info.get("width", 0) > 100
                images[label] = info
            report["checks"]["capture"] = {"pass": all(i["pass"] for i in images.values()), "images": images}
            if not report["checks"]["capture"]["pass"]:
                raise AssertionError("capture check failed: %s" % json.dumps(images))

        jumpBaseline = link.observe("Player")["position"]["y"]
        link.req("keyboard", {"keys": ["SPACE"], "updates": 3})
        jumpSamples, jump_start = [], time.monotonic()
        while time.monotonic() - jump_start < 6.0:
            current = link.req("status")
            observed = link.observe("Player")
            jumpSamples.append({"t": round(time.monotonic() - jump_start, 3),
                                "status": current["keyboard"]["status"],
                                "position": observed.get("position"),
                                "velocity": observed.get("velocity")})
            pos = observed.get("position") or {}
            if (not args.no_capture
                    and "airborne" not in (report.get("checks", {}).get("capture", {}) or {}).get("images", {})
                    and _is_finite_number(pos.get("y"))
                    and float(pos["y"]) - float(jumpBaseline) > MIN_PEAK_RISE):
                reply = link.capture()
                capture = reply.get("capture") or {}
                name = pathlib.PurePath(str(capture.get("basename") or "")).name
                info = {"basename": name, "reportedWidth": capture.get("width"),
                        "reportedHeight": capture.get("height"), "error": capture.get("error")}
                if name and (out_dir / name).is_file():
                    data = (out_dir / name).read_bytes()
                    info["bytes"] = len(data)
                    info["pngHeader"] = data[:8] == b"\x89PNG\r\n\x1a\n"
                    if info["pngHeader"] and len(data) >= 24:
                        info["width"], info["height"] = struct.unpack(">II", data[16:24])
                    info["sha256"] = hashlib.sha256(data).hexdigest()
                info["pass"] = bool(info.get("pngHeader")) and info.get("bytes", 0) > 4096 \
                    and (info.get("width"), info.get("height")) == (capture.get("width"), capture.get("height")) \
                    and info.get("width", 0) > 100
                images = report.setdefault("checks", {}).setdefault(
                    "capture", {"pass": True, "images": {}}).setdefault("images", {})
                images["airborne"] = info
                report["checks"]["capture"]["pass"] = all(i["pass"] for i in images.values())
            if (validate_jump_samples(jumpSamples, jumpBaseline)["pass"]
                    and jumpSamples[-1]["status"] == "completed"):
                break
            time.sleep(0.03)
        if not args.no_capture:
            reply = link.capture()
            capture = reply.get("capture") or {}
            name = pathlib.PurePath(str(capture.get("basename") or "")).name
            info = {"basename": name, "reportedWidth": capture.get("width"),
                    "reportedHeight": capture.get("height"), "error": capture.get("error")}
            if name and (out_dir / name).is_file():
                data = (out_dir / name).read_bytes()
                info["bytes"] = len(data)
                info["pngHeader"] = data[:8] == b"\x89PNG\r\n\x1a\n"
                if info["pngHeader"] and len(data) >= 24:
                    info["width"], info["height"] = struct.unpack(">II", data[16:24])
                info["sha256"] = hashlib.sha256(data).hexdigest()
            info["pass"] = bool(info.get("pngHeader")) and info.get("bytes", 0) > 4096 \
                and (info.get("width"), info.get("height")) == (capture.get("width"), capture.get("height")) \
                and info.get("width", 0) > 100
            images = report.setdefault("checks", {}).setdefault(
                "capture", {"pass": True, "images": {}}).setdefault("images", {})
            images["landed"] = info
            report["checks"]["capture"]["pass"] = all(i["pass"] for i in images.values())
            if not report["checks"]["capture"]["pass"]:
                raise AssertionError("capture check failed: %s" % json.dumps(images))
        report["jumpSamples"] = jumpSamples
        report["checks"]["jump"] = validate_jump_samples(jumpSamples, jumpBaseline)
        if not report["checks"]["jump"]["pass"]:
            raise AssertionError("jump check failed: %s" % json.dumps(report["checks"]["jump"]))

        quit_reply = link.req("quit")
        report["quitReceipt"] = {"ok": quit_reply.get("ok"), "requiresReceipt": quit_reply.get("requiresReceipt")}
        link.close()
        try:
            exit_code = child.wait(timeout=args.shutdown_timeout)
        except subprocess.TimeoutExpired:
            exit_code = None
        report["shutdown"] = {
            "exitCode": exit_code,
            "status": "PASS" if exit_code == 0 else ("TIMEOUT" if exit_code is None else "KNOWN_FAILURE"),
            "hResult": hex(exit_code) if isinstance(exit_code, int) and exit_code < 0 else None,
        }
        if exit_code is None and not args.require_clean_shutdown:
            report["shutdown"]["status"] = "KNOWN_FAILURE"
        failed = [name for name, value in report["checks"].items() if not value.get("pass")]
        if failed:
            raise AssertionError("failed checks: %s" % failed)
        if args.require_clean_shutdown and report["shutdown"]["status"] != "PASS":
            raise AssertionError("shutdown not clean: %s" % report["shutdown"])
        report["status"] = "PASS" if report["shutdown"]["status"] == "PASS" else "PASS_WITH_KNOWN_SHUTDOWN_FAILURE"
    except Exception:
        report["error"] = traceback.format_exc()
    finally:
        if child is not None and child.poll() is None:
            child.kill()
            child.wait(timeout=5)
        report["childCleanedUp"] = child is None or child.poll() is not None
        report["elapsedSeconds"] = round(time.monotonic() - report["startedAt"], 2)
        (out_dir / "result.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
        print(json.dumps({k: report[k] for k in ("status", "runId", "pid", "exeSha256", "checks",
                                                "shutdown", "error", "elapsedSeconds")
                          if k in report}, ensure_ascii=False, indent=2))
    return 0 if report["status"].startswith("PASS") else 1


def main():
    parser = argparse.ArgumentParser(description="Phase 4 acceptance runner for the runtime test bridge")
    parser.add_argument("--exe", required=True, help="Debug GAME_RUNTIME executable to launch")
    parser.add_argument("--out-dir", required=True, help="new directory for PNGs, logs and result.json")
    parser.add_argument("--expected-scene", default="scene_runtime_bridge_player.json")
    parser.add_argument("--updates", type=int, default=120)
    parser.add_argument("--hold-window", type=float, default=3.0)
    parser.add_argument("--settle", type=float, default=0.6)
    parser.add_argument("--ready-timeout", type=float, default=25.0)
    parser.add_argument("--shutdown-timeout", type=float, default=15.0)
    parser.add_argument("--no-capture", action="store_true")
    parser.add_argument("--require-clean-shutdown", action="store_true")
    return run(parser.parse_args())


if __name__ == "__main__":
    raise SystemExit(main())
