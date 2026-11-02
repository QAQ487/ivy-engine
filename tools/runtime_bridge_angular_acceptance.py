"""Live angular-physics acceptance for the runtime test bridge (plan PHASE 4).

Requires Debug GAME_RUNTIME + world-manip verbs. Stages an isolated fixture
(skip_boot_preload + --scene), then runs deterministic scenarios:

  pause -> setup (set_transform / set_velocity / apply_*) -> step N -> observe

Does not claim a Windows runtime PASS from Linux. Encodes current intended
behavior; known-limitation cases are reported as expected-skip with a reason.

Pure math/validators live in runtime_bridge_angular_math.py (Linux-safe).
"""
from __future__ import annotations

import argparse
import json
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

from runtime_bridge_angular_math import (  # noqa: E402
    KNOWN_LIMITATION_SCENARIOS,
    PHYSICS_DT,
    SPHERE_RADIUS,
    TOL_AXIS_DOT,
    TOL_ANG_VEL_ABS,
    TOL_ANG_VEL_REL,
    TOL_LIN_VEL_ABS,
    TOL_QUAT_ANGLE_DEG,
    TOL_QUAT_NORM,
    TOL_RESET_ABS,
    TOL_RESET_REL,
    TOL_ROLL_SLIP_SHRINK,
    TOL_TIP_UP_DOT,
    quat4,
    validate_free_spin,
    validate_off_center_impulse,
    validate_reset_determinism,
    validate_rolling,
    validate_rotation_integration,
    validate_tipping,
    vec3,
    v_add,
)

REPO_ROOT = HERE.parent
DEFAULT_FIXTURE = REPO_ROOT / "testbed" / "rom" / "scene" / "scene_runtime_bridge_angular.json"
EXPECTED_SCENE = "scene_runtime_bridge_angular.json"
SCENE_ARG = "rom/scene/scene_runtime_bridge_angular.json"

FREE_SPIN_OBJECT = "FreeSpinBox"
ROLLING_OBJECT = "RollingSphere"
TIPPING_OBJECT = "TippingBox"
TORQUE_IMPULSE = (0.0, 0.0, 2.0)
OFF_CENTER_IMPULSE = (4.0, 0.0, 0.0)
OFF_CENTER_POINT_OFFSET = (0.0, 0.3, 0.0)
ROLL_LINEAR_VEL = (3.0, 0.0, 0.0)


def _windows_bridge_deps():
    """Import named-pipe client helpers only when launching a live runtime."""
    from runtime_bridge_mouse_acceptance import (
        ENV_CLEAR,
        Link,
        session_of,
        stage_fixture_scene_assets,
    )
    return ENV_CLEAR, Link, session_of, stage_fixture_scene_assets


# ---------------------------------------------------------------------------
# Live runner helpers
# ---------------------------------------------------------------------------

def stage_fixture_package(exe, out_dir, fixture_scene):
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
    (package / "rom" / "skip_boot_preload").write_text("", encoding="ascii")
    _, _, _, stage_fixture_scene_assets = _windows_bridge_deps()
    stage_fixture_scene_assets(package, fixture_src)
    return staged_exe, package


def wait_ready(link, timeout=25.0):
    deadline = time.monotonic() + timeout
    last = None
    while time.monotonic() < deadline:
        last = link.req("status")
        if last.get("ok") and last.get("ready") and last.get("playing"):
            return last
        time.sleep(0.05)
    raise TimeoutError("bridge not ready: %s" % last)


def wait_world_op(link, start_request_id, timeout=8.0):
    deadline = time.monotonic() + timeout
    last = None
    while time.monotonic() < deadline:
        last = link.req("status")
        op = last.get("worldOp") or {}
        if op.get("requestId") != start_request_id:
            time.sleep(0.01)
            continue
        if op.get("status") == "completed":
            return last
        if op.get("status") == "failed":
            raise RuntimeError("worldOp failed: %s" % op.get("error"))
        time.sleep(0.01)
    raise TimeoutError("worldOp completion deadline: %s" % last)


def queue_world(link, verb, payload=None):
    reply = link.req(verb, payload)
    if not reply.get("ok"):
        raise RuntimeError("%s rejected: %s" % (verb, reply))
    wait_world_op(link, reply["requestId"])
    return reply


def wait_steps_drain(link, timeout=8.0):
    deadline = time.monotonic() + timeout
    last = None
    while time.monotonic() < deadline:
        last = link.req("status")
        if int(last.get("pendingSteps") or 0) == 0:
            return last
        time.sleep(0.02)
    raise TimeoutError("pendingSteps drain deadline: %s" % last)


def step_frames(link, frames):
    reply = link.req("step", {"frames": int(frames)})
    if not reply.get("ok"):
        raise RuntimeError("step rejected: %s" % reply)
    wait_steps_drain(link)
    return reply


def sample_record(obs, physics_tick=None):
    return {
        "physicsTick": physics_tick,
        "position": obs.get("position"),
        "velocity": obs.get("velocity"),
        "angularVelocity": obs.get("angularVelocity"),
        "rotationQuaternion": obs.get("rotationQuaternion"),
        "rigidbody": obs.get("rigidbody"),
    }


def run_free_spin(link, report):
    name = FREE_SPIN_OBJECT
    queue_world(link, "set_transform", {
        "object": name,
        "position": [0.0, 4.0, 0.0],
        "rotation": [0.0, 0.0, 0.0],
        "zeroVelocity": True,
    })
    # One tick so localInverseInertia is refreshed from the collider.
    step_frames(link, 1)
    queue_world(link, "apply_torque_impulse", {
        "object": name,
        "torqueImpulse": list(TORQUE_IMPULSE),
    })
    after_imp = link.observe(name)
    steps = 30
    samples = [sample_record(after_imp)]
    for _ in range(steps):
        step_frames(link, 1)
        obs = link.observe(name)
        samples.append(sample_record(obs))
    result = validate_free_spin(after_imp, TORQUE_IMPULSE, steps, samples[-1])
    # Integrate uses the pre-drag omega each tick; observed sample[i] is the
    # omega Integrate will use on the next tick (post-drag of previous).
    q0 = quat4(after_imp.get("rotationQuaternion"))
    integrate_omegas = [vec3(s.get("angularVelocity")) for s in samples[:steps]]
    rot = validate_rotation_integration(
        q0, integrate_omegas, quat4(samples[-1].get("rotationQuaternion")))
    report["checks"]["free_spin"] = result
    report["checks"]["rotation_integration"] = rot
    report["samples"]["free_spin"] = samples
    if not result["pass"]:
        raise RuntimeError("free_spin failed: %s" % result)
    if not rot["pass"]:
        raise RuntimeError("rotation_integration failed: %s" % rot)
    return after_imp, samples[-1]


def run_off_center(link, report):
    name = FREE_SPIN_OBJECT
    queue_world(link, "set_transform", {
        "object": name,
        "position": [0.0, 5.0, 0.0],
        "rotation": [0.0, 0.0, 0.0],
        "zeroVelocity": True,
    })
    step_frames(link, 1)
    before = link.observe(name)
    rb = before.get("rigidbody") or {}
    mass = float(rb.get("mass") or 1.0)
    inv = rb.get("localInverseInertia") or {"x": 6.0, "y": 6.0, "z": 6.0}
    pos = vec3(before.get("position"))
    point = v_add(pos, OFF_CENTER_POINT_OFFSET)
    queue_world(link, "apply_impulse", {
        "object": name,
        "impulse": list(OFF_CENTER_IMPULSE),
        "point": list(point),
    })
    after = link.observe(name)
    result = validate_off_center_impulse(
        before, after, OFF_CENTER_IMPULSE, point, mass, inv)
    report["checks"]["off_center_impulse"] = result
    report["samples"]["off_center_impulse"] = [
        sample_record(before), sample_record(after)]
    if not result["pass"]:
        raise RuntimeError("off_center_impulse failed: %s" % result)
    return after


def run_rolling(link, report):
    name = ROLLING_OBJECT
    queue_world(link, "set_transform", {
        "object": name,
        "position": [4.0, 0.5, 0.0],
        "rotation": [0.0, 0.0, 0.0],
        "zeroVelocity": True,
    })
    queue_world(link, "set_velocity", {
        "object": name,
        "linear": list(ROLL_LINEAR_VEL),
        "angular": [0.0, 0.0, 0.0],
    })
    samples = []
    # Settle onto ground then record rolling development.
    step_frames(link, 5)
    for _ in range(12):
        step_frames(link, 5)
        obs = link.observe(name)
        vel = vec3(obs.get("velocity"))
        ang = vec3(obs.get("angularVelocity"))
        samples.append({
            "vx": vel[0], "vy": vel[1], "vz": vel[2],
            "wx": ang[0], "wy": ang[1], "wz": ang[2],
            "position": obs.get("position"),
        })
    result = validate_rolling(samples, SPHERE_RADIUS)
    report["checks"]["sphere_rolling"] = result
    report["samples"]["sphere_rolling"] = samples
    if not result["pass"]:
        raise RuntimeError("sphere_rolling failed: %s" % result)
    return samples


def run_tipping(link, report):
    name = TIPPING_OBJECT
    # Place tilted near the ground so gravity tips it flat.
    queue_world(link, "set_transform", {
        "object": name,
        "position": [-4.0, 0.71, 0.0],
        "rotation": [0.0, 0.0, 35.0],
        "zeroVelocity": True,
    })
    samples = []
    for _ in range(20):
        step_frames(link, 15)
        obs = link.observe(name)
        samples.append(sample_record(obs))
    result = validate_tipping(samples[-1])
    report["checks"]["tipping"] = result
    report["samples"]["tipping"] = samples
    if not result["pass"]:
        # Tipping depends on contact torque; report as finding rather than
        # silently skipping — raise so the run fails unless we later mark skip.
        raise RuntimeError("tipping failed: %s" % result)
    return samples[-1]


def run_reset_determinism(link, report, first_after_impulse):
    queue_world(link, "reset_scene", {})
    wait_ready(link, timeout=15.0)
    pause = link.req("pause")
    if pause.get("paused") is not True:
        raise RuntimeError("pause after reset failed: %s" % pause)
    name = FREE_SPIN_OBJECT
    queue_world(link, "set_transform", {
        "object": name,
        "position": [0.0, 4.0, 0.0],
        "rotation": [0.0, 0.0, 0.0],
        "zeroVelocity": True,
    })
    step_frames(link, 1)
    queue_world(link, "apply_torque_impulse", {
        "object": name,
        "torqueImpulse": list(TORQUE_IMPULSE),
    })
    second = link.observe(name)
    result = validate_reset_determinism(first_after_impulse, second)
    report["checks"]["reset_determinism"] = result
    report["samples"]["reset_determinism"] = [
        sample_record(first_after_impulse), sample_record(second)]
    if not result["pass"]:
        raise RuntimeError("reset_determinism failed: %s" % result)
    return second


def run(args):
    ENV_CLEAR, Link, session_of, _stage = _windows_bridge_deps()
    exe = pathlib.Path(args.exe).resolve()
    if not exe.is_file():
        raise SystemExit("exe not found: %s" % exe)
    out_dir = pathlib.Path(args.out_dir).resolve()
    if out_dir.exists():
        raise SystemExit("out dir already exists: %s" % out_dir)
    out_dir.mkdir(parents=True)

    staged_exe, package = stage_fixture_package(exe, out_dir, args.fixture_scene)
    report = {
        "schema": "ivy.runtime_bridge.angular_acceptance/1",
        "startedAt": time.monotonic(),
        "exe": str(exe),
        "stagedExe": str(staged_exe),
        "sceneArg": SCENE_ARG,
        "physicsDt": PHYSICS_DT,
        "tolerances": {
            "axisDot": TOL_AXIS_DOT,
            "angVelAbs": TOL_ANG_VEL_ABS,
            "angVelRel": TOL_ANG_VEL_REL,
            "linVelAbs": TOL_LIN_VEL_ABS,
            "quatAngleDeg": TOL_QUAT_ANGLE_DEG,
            "quatNorm": TOL_QUAT_NORM,
            "rollSlipShrink": TOL_ROLL_SLIP_SHRINK,
            "tipUpDot": TOL_TIP_UP_DOT,
            "resetAbs": TOL_RESET_ABS,
            "resetRel": TOL_RESET_REL,
        },
        "knownLimitations": list(KNOWN_LIMITATION_SCENARIOS),
        "launcherSession": session_of(os.getpid()),
        "checks": {},
        "samples": {},
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
        env["IVY_RUNTIME_TEST_BRIDGE"] = "1"
        env["IVY_RUNTIME_TEST_RUN_ID"] = run_id
        env["IVY_RUNTIME_TEST_OUTPUT_DIR"] = str(out_dir)
        env["IVY_RUNTIME_TEST_FIXED_CAMERA"] = "1"
        env["IVY_SKIP_CLOSE_CONFIRM"] = "1"
        child = subprocess.Popen(
            [str(staged_exe), "--scene", SCENE_ARG],
            cwd=str(package),
            env=env,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
        )
        report["pid"] = child.pid
        link = Link(child.pid, run_id, report)
        wait_ready(link)

        listed = link.req("list_objects", {"limit": 64})
        names = [o.get("name") for o in listed.get("objects", [])]
        needed = {FREE_SPIN_OBJECT, ROLLING_OBJECT, TIPPING_OBJECT, "Ground"}
        report["checks"]["list_objects"] = {
            "pass": needed.issubset(set(names)),
            "names": names,
        }
        if not needed.issubset(set(names)):
            raise RuntimeError("fixture objects missing: %s" % (needed - set(names),))

        pause = link.req("pause")
        report["checks"]["pause"] = {"pass": pause.get("paused") is True, "response": pause}
        if pause.get("paused") is not True:
            raise RuntimeError("pause failed")

        # Probe observe rigidbody extensions once.
        probe = link.observe(FREE_SPIN_OBJECT)
        rb = probe.get("rigidbody") or {}
        report["checks"]["observe_rigidbody_fields"] = {
            "pass": all(k in rb for k in (
                "kinematic", "mass", "drag", "angularDrag", "friction",
                "useGravity", "localInverseInertia")),
            "rigidbody": rb,
        }
        if not report["checks"]["observe_rigidbody_fields"]["pass"]:
            raise RuntimeError("observe missing rigidbody fields: %s" % rb)

        first_impulse, _ = run_free_spin(link, report)
        run_off_center(link, report)
        run_rolling(link, report)
        run_tipping(link, report)
        run_reset_determinism(link, report, first_impulse)

        status_before_quit = link.req("status")
        report["checks"]["paused_before_quit"] = {
            "pass": status_before_quit.get("paused") is True,
            "paused": status_before_quit.get("paused"),
            "pendingSteps": status_before_quit.get("pendingSteps"),
        }
        quit_reply = link.req("quit")
        quit_ok = quit_reply.get("ok") is True and quit_reply.get("requiresReceipt") is True
        report["checks"]["quit"] = {
            "pass": quit_ok,
            "ok": quit_reply.get("ok"),
            "requiresReceipt": quit_reply.get("requiresReceipt"),
        }
        if not quit_ok:
            raise RuntimeError("invalid quit reply: %s" % quit_reply)
        link.close()
        try:
            exit_code = child.wait(timeout=15)
        except subprocess.TimeoutExpired:
            raise RuntimeError("runtime did not exit within 15s after quit while paused")
        report["exitCode"] = exit_code
        report["checks"]["exit"] = {"pass": exit_code == 0, "exitCode": exit_code}
        if exit_code != 0:
            raise RuntimeError("runtime exit code %s" % exit_code)

        report["status"] = "PASS"
        return 0
    except Exception as exc:
        report["error"] = str(exc)
        report["traceback"] = traceback.format_exc()
        if child is not None and child.poll() is None:
            child.kill()
            try:
                child.wait(timeout=5)
            except Exception:
                pass
        return 1
    finally:
        if link is not None:
            link.close()
        if child is not None and child.stderr:
            err = child.stderr.read()
            if err:
                (out_dir / "runtime.stderr.txt").write_bytes(err)
            child.stderr.close()
        report["elapsedSec"] = round(time.monotonic() - report["startedAt"], 3)
        (out_dir / "result.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
        print(json.dumps({"status": report["status"], "outDir": str(out_dir)}, ensure_ascii=False))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", required=True, help="path to Debug IvyEngineRuntime.exe")
    parser.add_argument("--out-dir", required=True)
    parser.add_argument("--fixture-scene", default=str(DEFAULT_FIXTURE),
                        help="path to scene_runtime_bridge_angular.json")
    return run(parser.parse_args())


if __name__ == "__main__":
    raise SystemExit(main())
