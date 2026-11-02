"""Live acceptance for runtime-bridge world manipulation verbs.

Stages an isolated fixture (skip_boot_preload + --scene), then:
  pause -> set_transform DynamicBox -> apply_impulse -> step -> assert motion
  -> reset_scene -> assert initial transform restored.
"""
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

from runtime_bridge_mouse_acceptance import (
    ENV_CLEAR,
    Link,
    session_of,
    stage_fixture_scene_assets,
)
from runtime_test_client import Client

REPO_ROOT = HERE.parent
DEFAULT_FIXTURE = REPO_ROOT / "testbed" / "rom" / "scene" / "scene_runtime_bridge_world_manip.json"
EXPECTED_SCENE = "scene_runtime_bridge_world_manip.json"
SCENE_ARG = "rom/scene/scene_runtime_bridge_world_manip.json"
OBJECT_NAME = "DynamicBox"
INITIAL_POS = (0.0, 2.0, 0.0)
TELEPORT_POS = (3.0, 2.0, 0.0)
IMPULSE = (8.0, 0.0, 0.0)


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
    """Poll status.worldOp until completed/failed; keep Link request counter in sync."""
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


def pos_of(obs):
    p = obs.get("position") or {}
    return float(p.get("x", 0.0)), float(p.get("y", 0.0)), float(p.get("z", 0.0))


def vel_of(obs):
    v = obs.get("velocity") or {}
    return float(v.get("x", 0.0)), float(v.get("y", 0.0)), float(v.get("z", 0.0))


def run(args):
    exe = pathlib.Path(args.exe).resolve()
    if not exe.is_file():
        raise SystemExit("exe not found: %s" % exe)
    out_dir = pathlib.Path(args.out_dir).resolve()
    if out_dir.exists():
        raise SystemExit("out dir already exists: %s" % out_dir)
    out_dir.mkdir(parents=True)

    staged_exe, package = stage_fixture_package(exe, out_dir, args.fixture_scene)
    report = {
        "schema": "ivy.runtime_bridge.world_manip_acceptance/1",
        "startedAt": time.monotonic(),
        "exe": str(exe),
        "stagedExe": str(staged_exe),
        "sceneArg": SCENE_ARG,
        "launcherSession": session_of(os.getpid()),
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
        report["checks"]["list_has_box"] = {
            "pass": OBJECT_NAME in names,
            "names": names,
        }
        if OBJECT_NAME not in names:
            raise RuntimeError("DynamicBox missing from list_objects")

        initial = link.observe(OBJECT_NAME)
        report["checks"]["initial_observe"] = {
            "pass": initial.get("found") is True,
            "position": initial.get("position"),
            "velocity": initial.get("velocity"),
        }
        ix, iy, iz = pos_of(initial)
        if abs(ix - INITIAL_POS[0]) > 0.05 or abs(iy - INITIAL_POS[1]) > 0.05:
            raise RuntimeError("unexpected initial pose: %s" % (pos_of(initial),))

        pause = link.req("pause")
        report["checks"]["pause"] = {"pass": pause.get("paused") is True, "response": pause}

        queue_world(link, "set_transform", {
            "object": OBJECT_NAME,
            "position": list(TELEPORT_POS),
            "zeroVelocity": True,
        })
        after_tp = link.observe(OBJECT_NAME)
        tx, ty, tz = pos_of(after_tp)
        report["checks"]["teleport"] = {
            "pass": abs(tx - TELEPORT_POS[0]) < 0.05 and abs(ty - TELEPORT_POS[1]) < 0.05,
            "position": after_tp.get("position"),
            "velocity": after_tp.get("velocity"),
        }
        if not report["checks"]["teleport"]["pass"]:
            raise RuntimeError("teleport failed: %s" % (pos_of(after_tp),))

        queue_world(link, "apply_impulse", {
            "object": OBJECT_NAME,
            "impulse": list(IMPULSE),
        })
        after_imp = link.observe(OBJECT_NAME)
        vx, vy, vz = vel_of(after_imp)
        report["checks"]["impulse_velocity"] = {
            "pass": vx > 1.0,
            "velocity": after_imp.get("velocity"),
        }
        if vx <= 1.0:
            raise RuntimeError("expected +X velocity after impulse, got %s" % (vel_of(after_imp),))

        step = link.req("step", {"frames": 10})
        report["checks"]["step"] = {"pass": step.get("ok") is True, "response": step}
        # Allow pending steps to drain while paused.
        deadline = time.monotonic() + 5.0
        while time.monotonic() < deadline:
            st = link.req("status")
            if int(st.get("pendingSteps") or 0) == 0:
                break
            time.sleep(0.02)

        after_step = link.observe(OBJECT_NAME)
        sx, sy, sz = pos_of(after_step)
        moved_x = sx - TELEPORT_POS[0]
        report["checks"]["step_motion"] = {
            "pass": moved_x > 0.05,
            "position": after_step.get("position"),
            "deltaX": moved_x,
        }
        if moved_x <= 0.05:
            raise RuntimeError("expected +X displacement after step, delta=%s" % moved_x)

        queue_world(link, "reset_scene", {})
        wait_ready(link, timeout=15.0)
        restored = link.observe(OBJECT_NAME)
        rx, ry, rz = pos_of(restored)
        rvx, rvy, rvz = vel_of(restored)
        report["checks"]["reset_scene"] = {
            "pass": abs(rx - INITIAL_POS[0]) < 0.1 and abs(ry - INITIAL_POS[1]) < 0.1 and abs(rvx) < 0.5,
            "position": restored.get("position"),
            "velocity": restored.get("velocity"),
        }
        if not report["checks"]["reset_scene"]["pass"]:
            raise RuntimeError("reset_scene did not restore initial transform: pos=%s vel=%s"
                               % (pos_of(restored), vel_of(restored)))

        # Quit while still paused (contract: host flushes exit without resume/sim tick).
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
                        help="path to scene_runtime_bridge_world_manip.json")
    return run(parser.parse_args())


if __name__ == "__main__":
    raise SystemExit(main())
