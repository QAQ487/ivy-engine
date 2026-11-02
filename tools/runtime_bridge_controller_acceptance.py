"""Import-safe stdlib left-stick runtime acceptance runner for the Ivy runtime test bridge.

Fixed forward profile only (lx=0, ly=1, updates=120). Asserts requestId-bound
lifecycle samples, walking envelope with max displacement, multi-sample release
stability, real PNG captures, explicit rejections, quit exit 0, and owned-child
cleanup. Does not claim camera angles from PNG hashes.
"""
import argparse
import hashlib
import json
import math
import os
import pathlib
import subprocess
import sys
import time
import traceback
import uuid

HERE = pathlib.Path(__file__).resolve().parent
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

from runtime_bridge_mouse_acceptance import (
    DEFAULT_FIXTURE,
    ENV_CLEAR,
    EXPECTED_SCENE,
    SCENE_ARG,
    Link,
    session_of,
    stage_fixture_package,
    validate_png_bytes,
    validate_rejection_response,
    validate_shutdown,
    vec,
)

FIXED_LX = 0.0
FIXED_LY = 1.0
FIXED_UPDATES = 120

MIN_DISPLACEMENT = 11.0
# 6 m/s ±0.5 over 120 updates at 60 Hz nominal → upper envelope 13.0
MAX_DISPLACEMENT = (6.0 + 0.5) * (FIXED_UPDATES / 60.0)
MAX_HEIGHT_DRIFT = 0.01
MAX_LINE_DEVIATION = 0.5
MAX_VERTICAL_SPEED = 0.1
SPEED_TOLERANCE = 0.5
EXPECTED_SPEED = 6.0
MIN_ACTIVE_SAMPLES = 5
RELEASE_SPEED_TOLERANCE = 0.15
RELEASE_POSITION_TOLERANCE = 0.05
MIN_RELEASE_SAMPLES = 3

ALLOWED_CONTROLLER_STATUS = frozenset({
    "queued", "preparing", "active", "releasing", "completed", "cancelled",
})

# CLI duration / poll bounds (exclusive zero, inclusive upper)
CLI_BOUNDS = {
    "settle": (0.0, 5.0),
    "release_settle": (0.0, 5.0),
    "ready_timeout": (0.0, 60.0),
    "shutdown_timeout": (0.0, 60.0),
    "timeout": (0.0, 30.0),
    "poll_interval": (0.0, 1.0),
}


def _is_finite_number(value):
    return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)


def _is_nonneg_int(value):
    return type(value) is int and value >= 0


def is_active_hold_sample(sample):
    """True for active hold samples; excludes exhausted-budget release-boundary samples."""
    if not isinstance(sample, dict):
        return False
    if sample.get("status") != "active":
        return False
    remaining = sample.get("remaining")
    return type(remaining) is int and remaining > 0


def validate_runner_args(args):
    """Reject invalid CLI before creating out-dir or spawning GUI. Returns error string or None."""
    if not isinstance(getattr(args, "exe", None), str) or not args.exe:
        return "exe path required"
    if not isinstance(getattr(args, "out_dir", None), str) or not args.out_dir:
        return "out-dir required"
    scene = getattr(args, "expected_scene", None)
    if not isinstance(scene, str) or not scene or len(scene) > 256:
        return "expected-scene must be a non-empty string <= 256 chars"
    for name, (lo, hi) in CLI_BOUNDS.items():
        value = getattr(args, name, None)
        if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
            return "%s must be a finite number" % name
        if not (lo < float(value) <= hi):
            return "%s must be in (%g,%g]" % (name, lo, hi)
    return None


def validate_lifecycle_sample(sample, operation_id, start_pos=None):
    """Validate one status/observe sample bound to the queued controller operation."""
    if not isinstance(sample, dict):
        return {"pass": False, "error": "sample not a dict"}
    if sample.get("found") is not True:
        return {"pass": False, "error": "observe found is not true"}
    if sample.get("requestId") != operation_id:
        return {"pass": False, "error": "controller requestId mismatch: %r != %r" % (sample.get("requestId"), operation_id)}
    status = sample.get("status")
    if status not in ALLOWED_CONTROLLER_STATUS:
        return {"pass": False, "error": "malformed controller status: %r" % status}
    remaining = sample.get("remaining")
    if not _is_nonneg_int(remaining):
        return {"pass": False, "error": "remainingUpdates missing/negative/non-int: %r" % remaining}
    if remaining > FIXED_UPDATES:
        return {"pass": False, "error": "remainingUpdates %r > %d" % (remaining, FIXED_UPDATES)}
    pos = sample.get("position")
    vel = sample.get("velocity")
    if not isinstance(pos, dict) or not isinstance(vel, dict):
        return {"pass": False, "error": "position/velocity missing"}
    for label, vector in (("position", pos), ("velocity", vel)):
        for axis in ("x", "y", "z"):
            if not _is_finite_number(vector.get(axis)):
                return {"pass": False, "error": "non-finite %s.%s" % (label, axis)}
    if start_pos is not None:
        if not isinstance(start_pos, dict) or not _is_finite_number(start_pos.get("y")):
            return {"pass": False, "error": "start_pos height invalid"}
        if abs(pos["y"] - start_pos["y"]) > MAX_HEIGHT_DRIFT:
            return {"pass": False, "error": "sample height drift %.6f" % abs(pos["y"] - start_pos["y"])}
    if abs(vel["y"]) > MAX_VERTICAL_SPEED:
        return {"pass": False, "error": "vertical velocity %.6f > %.2f" % (abs(vel["y"]), MAX_VERTICAL_SPEED)}
    return {"pass": True, "error": None}


def validate_walking_samples(samples, start_pos, end_pos, operation_id):
    """Validate forward-walk samples against the Phase 4 walking envelope with max bound."""
    if not samples or not isinstance(samples, (list, tuple)):
        return {"pass": False, "error": "samples empty or missing", "activeSamples": 0}
    if not isinstance(start_pos, dict) or not isinstance(end_pos, dict):
        return {"pass": False, "error": "start/end position missing", "activeSamples": 0}
    for label, pos in (("start", start_pos), ("end", end_pos)):
        for axis in ("x", "y", "z"):
            if not _is_finite_number(pos.get(axis)):
                return {"pass": False, "error": "%s non-finite %s" % (label, axis), "activeSamples": 0}
    if abs(end_pos["y"] - start_pos["y"]) > MAX_HEIGHT_DRIFT:
        return {"pass": False, "error": "endpoint height drift", "activeSamples": 0}

    start, end = vec(start_pos), vec(end_pos)
    delta = tuple(end[i] - start[i] for i in range(3))
    horizontal = math.hypot(delta[0], delta[2])
    span = (delta[0], delta[2])
    span_len = math.hypot(*span)
    worst = 0.0
    speeds = []

    for idx, sample in enumerate(samples):
        life = validate_lifecycle_sample(sample, operation_id, start_pos)
        if not life["pass"]:
            return {"pass": False, "error": "sample %d: %s" % (idx, life["error"]), "activeSamples": len(speeds)}
        # Exhausted-budget boundary: still fully lifecycle-validated; skip only speed envelope.
        if not is_active_hold_sample(sample):
            continue
        pos = sample["position"]
        vel = sample["velocity"]
        pos_v = (pos["x"], pos["y"], pos["z"])
        if span_len > 1e-6:
            rel = (pos_v[0] - start[0], pos_v[2] - start[2])
            perp = abs(rel[0] * span[1] - rel[1] * span[0]) / span_len
            worst = max(worst, perp)
        speeds.append(math.hypot(vel["x"], vel["z"]))

    last = samples[-1]
    completed = last.get("status") == "completed" and last.get("remaining") == 0
    errors = []
    if horizontal < MIN_DISPLACEMENT:
        errors.append("horizontalDisplacement %.6f < %.1f" % (horizontal, MIN_DISPLACEMENT))
    if horizontal > MAX_DISPLACEMENT:
        errors.append("horizontalDisplacement %.6f > %.1f" % (horizontal, MAX_DISPLACEMENT))
    if abs(delta[1]) > MAX_HEIGHT_DRIFT:
        errors.append("heightDrift %.6f > %.2f" % (abs(delta[1]), MAX_HEIGHT_DRIFT))
    if worst > MAX_LINE_DEVIATION:
        errors.append("maxLineDeviation %.6f > %.1f" % (worst, MAX_LINE_DEVIATION))
    if len(speeds) < MIN_ACTIVE_SAMPLES:
        errors.append("activeSamples %d < %d" % (len(speeds), MIN_ACTIVE_SAMPLES))
    if speeds and not all(abs(v - EXPECTED_SPEED) <= SPEED_TOLERANCE for v in speeds):
        errors.append("speed outside envelope")
    if not completed:
        errors.append("operation not completed with remainingUpdates 0")

    return {
        "pass": not errors,
        "error": None if not errors else "; ".join(errors),
        "horizontalDisplacement": horizontal,
        "maxDisplacement": MAX_DISPLACEMENT,
        "delta": list(delta),
        "maxLineDeviation": worst,
        "minSpeed": min(speeds) if speeds else None,
        "maxSpeed": max(speeds) if speeds else None,
        "activeSamples": len(speeds),
        "completed": completed,
        "operationId": operation_id,
    }


def validate_release_stable(release_samples, controller_status, operation_id, anchor_pos):
    """Require several post-completion observes with near-stable position and neutral speed."""
    if not isinstance(controller_status, dict):
        return {"pass": False, "error": "release controller status missing"}
    if controller_status.get("status") != "completed" or controller_status.get("remainingUpdates") != 0:
        return {"pass": False, "error": "controller not completed/neutral: %s" % controller_status}
    if controller_status.get("requestId") != operation_id:
        return {"pass": False, "error": "release status requestId mismatch"}
    if not isinstance(release_samples, (list, tuple)) or len(release_samples) < MIN_RELEASE_SAMPLES:
        return {"pass": False, "error": "need >= %d release samples" % MIN_RELEASE_SAMPLES}
    if not isinstance(anchor_pos, dict) or any(not _is_finite_number(anchor_pos.get(a)) for a in ("x", "y", "z")):
        return {"pass": False, "error": "release anchor position invalid"}

    speeds = []
    max_shift = 0.0
    for idx, sample in enumerate(release_samples):
        life = validate_lifecycle_sample(sample, operation_id, anchor_pos)
        if not life["pass"]:
            return {"pass": False, "error": "release sample %d: %s" % (idx, life["error"])}
        if sample.get("status") != "completed" or sample.get("remaining") != 0:
            return {"pass": False, "error": "release sample %d not completed/neutral" % idx}
        pos = sample["position"]
        vel = sample["velocity"]
        shift = math.hypot(pos["x"] - anchor_pos["x"], pos["z"] - anchor_pos["z"])
        max_shift = max(max_shift, shift)
        speed = math.hypot(vel["x"], vel["z"])
        speeds.append(speed)
        if speed > RELEASE_SPEED_TOLERANCE:
            return {"pass": False, "error": "release XZ speed %.6f > %.2f" % (speed, RELEASE_SPEED_TOLERANCE),
                    "xzSpeed": speed, "sampleCount": len(release_samples)}
    if max_shift > RELEASE_POSITION_TOLERANCE:
        return {"pass": False, "error": "release position drift %.6f > %.2f" % (max_shift, RELEASE_POSITION_TOLERANCE),
                "maxShift": max_shift, "sampleCount": len(release_samples)}
    return {
        "pass": True,
        "error": None,
        "sampleCount": len(release_samples),
        "maxShift": max_shift,
        "maxXzSpeed": max(speeds) if speeds else 0.0,
    }


def _build_sample(c_info, obs, hold_start):
    return {
        "phase": "forward",
        "t": round(time.monotonic() - hold_start, 3),
        "requestId": c_info.get("requestId"),
        "status": c_info.get("status"),
        "remaining": c_info.get("remainingUpdates"),
        "found": obs.get("found"),
        "position": obs.get("position"),
        "velocity": obs.get("velocity"),
    }


def run(args):
    arg_error = validate_runner_args(args)
    if arg_error:
        raise SystemExit(arg_error)

    exe = pathlib.Path(args.exe).resolve()
    if not exe.is_file():
        raise SystemExit("exe not found: %s" % exe)

    out_dir = pathlib.Path(args.out_dir).resolve()
    if out_dir.exists():
        raise SystemExit("out dir already exists: %s" % out_dir)
    # All CLI validation above must succeed before mkdir / spawn.
    out_dir.mkdir(parents=True)

    staged_exe, package = stage_fixture_package(exe, out_dir, getattr(args, "fixture_scene", DEFAULT_FIXTURE))
    launcher_session = session_of(os.getpid())
    report = {
        "schema": "ivy.runtime_bridge.controller_acceptance/1",
        "startedAt": time.monotonic(),
        "exe": str(exe),
        "stagedExe": str(staged_exe),
        "sceneArg": SCENE_ARG,
        "exeSha256": hashlib.sha256(exe.read_bytes()).hexdigest(),
        "launcherSession": launcher_session,
        "profile": {"lx": FIXED_LX, "ly": FIXED_LY, "updates": FIXED_UPDATES},
        "checks": {},
        "status": "FAIL",
    }
    child = None
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
                [str(staged_exe), "--scene", SCENE_ARG],
                cwd=str(package), env=env, stdout=stdout, stderr=stderr)
        report["pid"] = child.pid
        report["childSession"] = session_of(child.pid)
        if report["childSession"] == 0 or launcher_session == 0:
            raise AssertionError("not running in an interactive session")

        link = Link(child.pid, run_id, report)

        deadline = time.monotonic() + args.ready_timeout
        status = None
        while True:
            status = link.req("status")
            if status.get("ready") and status.get("playing"):
                break
            if time.monotonic() > deadline:
                raise AssertionError(
                    "runtime not ready within %.0fs (state=%r readyDetail=%r scene=%r)"
                    % (args.ready_timeout, status.get("state"), status.get("readyDetail"), status.get("scene")))
            if child.poll() is not None:
                raise AssertionError("runtime exited before ready: %s" % child.returncode)
            time.sleep(0.05)

        capabilities = sorted(status.get("capabilities") or [])
        if "controller" not in capabilities:
            raise AssertionError("missing controller capability in bridge status: %s" % capabilities)

        scene_name = pathlib.PurePosixPath(str(status.get("scene", "")).replace("\\", "/")).name
        if scene_name != args.expected_scene:
            raise AssertionError("unexpected scene %r (expected %r)" % (scene_name, args.expected_scene))

        report["checks"]["ready"] = {
            "pass": True,
            "scene": status.get("scene"),
            "state": status.get("state"),
            "capabilities": capabilities,
        }

        time.sleep(args.settle)

        initial_obs = link.observe("Player")
        if initial_obs.get("found") is not True:
            raise AssertionError("Player target not found in snapshot")
        initial_pos = initial_obs["position"]
        if not isinstance(initial_pos, dict) or any(not _is_finite_number(initial_pos.get(a)) for a in ("x", "y", "z")):
            raise AssertionError("initial Player position non-finite")
        report["checks"]["observe_player"] = {
            "pass": True,
            "initialPosition": initial_pos,
            "initialVelocity": initial_obs.get("velocity"),
        }

        before_reply = link.capture()
        before_cap = before_reply.get("capture") or {}
        before_name = pathlib.PurePath(str(before_cap.get("basename") or "")).name
        before_path = out_dir / before_name
        if not before_path.is_file():
            raise AssertionError("before capture PNG file not found: %s" % before_name)
        before_val = validate_png_bytes(before_path.read_bytes(), before_cap.get("width"), before_cap.get("height"))
        if not before_val["pass"]:
            raise AssertionError("before PNG validation failed: %s" % before_val["error"])
        sha_before = before_val["sha256"]

        rejection_specs = [
            ({"verb": "controller", "lx": 2.0, "ly": 0.0, "updates": 60}, "lx2"),
            ({"verb": "controller", "lx": True, "ly": 0.0, "updates": 60}, "lx_bool"),
            ({"verb": "controller", "lx": 0.0, "ly": "1", "updates": 60}, "ly_string"),
            ({"verb": "controller", "lx": 0.0, "ly": 0.0, "updates": 0}, "updates0"),
            ({"verb": "controller", "lx": 0.0, "ly": 0.0, "updates": 121}, "updates121"),
            ({"verb": "controller", "lx": 0.0, "ly": 1.0, "updates": 60, "extra": "invalid"}, "extra_key"),
        ]
        rejection_results = []
        for payload, label in rejection_specs:
            probe_res = link.probe_rejection(payload, label)
            rejection_results.append(probe_res)
            if not probe_res["pass"]:
                raise AssertionError("rejection check failed for %s: %s" % (label, probe_res["error"]))
        report["checks"]["rejections"] = {
            "pass": all(r["pass"] for r in rejection_results),
            "results": rejection_results,
        }

        ack = link.req("controller", {"lx": FIXED_LX, "ly": FIXED_LY, "updates": FIXED_UPDATES})
        if ack.get("ok") is not True or ack.get("status") != "queued":
            raise AssertionError("forward controller ack not queued: %s" % ack)
        operation_id = ack.get("requestId")
        if type(operation_id) is not int or operation_id < 1:
            raise AssertionError("queued ack missing requestId: %s" % ack)
        report["operationId"] = operation_id

        samples = []
        hold_start = time.monotonic()
        hold_deadline = hold_start + args.timeout
        while True:
            cur_status = link.req("status")
            c_info = cur_status.get("controller") or {}
            obs = link.observe("Player")
            sample = _build_sample(c_info, obs, hold_start)
            life = validate_lifecycle_sample(sample, operation_id, initial_pos)
            if not life["pass"]:
                raise AssertionError("lifecycle sample failed: %s" % life["error"])
            samples.append(sample)
            if sample["status"] == "completed" and sample["remaining"] == 0:
                break
            if time.monotonic() > hold_deadline:
                raise AssertionError("forward controller operation did not complete within deadline")
            time.sleep(args.poll_interval)

        after_obs = link.observe("Player")
        if after_obs.get("found") is not True:
            raise AssertionError("Player target missing after controller hold")
        end_pos = after_obs["position"]
        if not isinstance(end_pos, dict) or any(not _is_finite_number(end_pos.get(a)) for a in ("x", "y", "z")):
            raise AssertionError("end Player position non-finite")

        after_reply = link.capture()
        after_cap = after_reply.get("capture") or {}
        after_name = pathlib.PurePath(str(after_cap.get("basename") or "")).name
        after_path = out_dir / after_name
        if not after_path.is_file():
            raise AssertionError("after capture PNG file not found: %s" % after_name)
        after_val = validate_png_bytes(after_path.read_bytes(), after_cap.get("width"), after_cap.get("height"))
        if not after_val["pass"]:
            raise AssertionError("after PNG validation failed: %s" % after_val["error"])
        sha_after = after_val["sha256"]

        report["samples"] = samples
        movement = validate_walking_samples(samples, initial_pos, end_pos, operation_id)
        report["checks"]["movement"] = movement
        if not movement["pass"]:
            raise AssertionError("movement check failed: %s" % movement["error"])

        time.sleep(args.release_settle)
        release_samples = []
        release_status = None
        for _ in range(MIN_RELEASE_SAMPLES):
            release_status = link.req("status")
            c_info = release_status.get("controller") or {}
            obs = link.observe("Player")
            sample = _build_sample(c_info, obs, hold_start)
            sample["phase"] = "release"
            release_samples.append(sample)
            time.sleep(args.poll_interval)

        release = validate_release_stable(
            release_samples, (release_status or {}).get("controller") or {}, operation_id, end_pos)
        report["releaseSamples"] = release_samples
        report["checks"]["release"] = release
        if not release["pass"]:
            raise AssertionError("release stability failed: %s" % release["error"])

        report["checks"]["capture"] = {
            "pass": True,
            "beforeSha256": sha_before,
            "afterSha256": sha_after,
            "beforeBytes": before_val["bytes"],
            "afterBytes": after_val["bytes"],
            "note": "PNG hashes prove real capture files only; not precise camera angles",
        }

        quit_reply = link.req("quit")
        quit_ok = quit_reply.get("ok") is True and quit_reply.get("requiresReceipt") is True
        report["quitReceipt"] = {
            "ok": quit_reply.get("ok"),
            "requiresReceipt": quit_reply.get("requiresReceipt"),
            "pass": quit_ok,
        }
        if not quit_ok:
            raise AssertionError("invalid quit receipt reply: %s" % quit_reply)
        link.close()

        try:
            exit_code = child.wait(timeout=args.shutdown_timeout)
        except subprocess.TimeoutExpired:
            exit_code = None

        shutdown_pass = validate_shutdown(exit_code)
        report["shutdown"] = {
            "exitCode": exit_code,
            "status": "PASS" if shutdown_pass else ("TIMEOUT" if exit_code is None else "CRASH"),
            "pass": shutdown_pass,
        }
        if not shutdown_pass:
            raise AssertionError("shutdown not clean exit 0: exitCode=%s" % exit_code)

        report["status"] = "PASS"
    except Exception:
        report["error"] = traceback.format_exc()
    finally:
        if child is not None and child.poll() is None:
            child.kill()
            child.wait(timeout=5)
        report["childCleanedUp"] = child is None or child.poll() is not None
        report["elapsedSeconds"] = round(time.monotonic() - report["startedAt"], 2)
        (out_dir / "result.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
        print(json.dumps({k: report[k] for k in ("status", "runId", "pid", "exeSha256", "checks", "shutdown", "error", "elapsedSeconds") if k in report}, ensure_ascii=False, indent=2))

    return 0 if report["status"] == "PASS" else 1


def main():
    parser = argparse.ArgumentParser(description="Left-stick controller runtime acceptance runner for Ivy test bridge")
    parser.add_argument("--exe", required=True, help="Debug GAME_RUNTIME executable to stage and launch")
    parser.add_argument("--out-dir", required=True, help="new directory for fixture package, PNGs, logs and result.json")
    parser.add_argument("--fixture-scene", default=str(DEFAULT_FIXTURE), help="path to scene_runtime_bridge_player.json")
    parser.add_argument("--expected-scene", default=EXPECTED_SCENE)
    parser.add_argument("--settle", type=float, default=0.6)
    parser.add_argument("--release-settle", type=float, default=0.3)
    parser.add_argument("--ready-timeout", type=float, default=25.0)
    parser.add_argument("--shutdown-timeout", type=float, default=15.0)
    parser.add_argument("--timeout", type=float, default=10.0)
    parser.add_argument("--poll-interval", type=float, default=0.05)
    args = parser.parse_args()
    # Fail closed before any out-dir creation / process spawn.
    arg_error = validate_runner_args(args)
    if arg_error:
        raise SystemExit(arg_error)
    return run(args)


if __name__ == "__main__":
    raise SystemExit(main())
