#!/usr/bin/env python3
"""Data-driven runtime-bridge scenario runner (plan PHASE 5).

Reads a JSON scenario, optionally stages a Debug GAME_RUNTIME fixture, drives
bridge verbs + runner-only steps, writes result.json, and always cleans up
unless --keep-open on PASS.

Linux-safe core (schema + FakeBridge). Live named-pipe launch is Windows-only.
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
from typing import Any

HERE = pathlib.Path(__file__).resolve().parent
REPO_ROOT = HERE.parent
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

from runtime_bridge_scenario_schema import (  # noqa: E402
    SCENARIO_SCHEMA_ID,
    ScenarioValidationError,
    eval_assert,
    resolve_path,
    validate_scenario,
)

WORLD_VERBS = frozenset({
    "reset_scene",
    "set_transform",
    "set_velocity",
    "apply_impulse",
    "apply_torque_impulse",
})


def load_and_validate_scenario(path: pathlib.Path) -> dict:
    text = pathlib.Path(path).read_text(encoding="utf-8")
    try:
        data = json.loads(text)
    except json.JSONDecodeError as exc:
        raise SystemExit("invalid JSON in %s: %s" % (path, exc)) from exc
    try:
        return validate_scenario(data)
    except ScenarioValidationError as exc:
        raise SystemExit("scenario validation failed: %s" % exc) from exc


def resolve_fixture_scene(scene_arg: str, repo_root: pathlib.Path | None = None) -> pathlib.Path:
    """Map rom/scene/foo.json to a repo file under testbed/ or resources/."""
    root = pathlib.Path(repo_root) if repo_root is not None else REPO_ROOT
    normalized = scene_arg.replace("\\", "/")
    candidates = [
        root / "testbed" / normalized,
        root / normalized,
    ]
    if normalized.startswith("rom/"):
        candidates.append(root / "resources" / normalized[4:])
    for candidate in candidates:
        if candidate.is_file():
            return candidate.resolve()
    raise SystemExit("fixture scene not found for %s (tried %s)"
                     % (scene_arg, ", ".join(str(c) for c in candidates)))


def stage_fixture_package(exe: pathlib.Path, out_dir: pathlib.Path, scene_arg: str):
    """Copy Debug OutDir, pin scene via default_scene + skip_boot_preload, stage assets."""
    from runtime_bridge_mouse_acceptance import stage_fixture_scene_assets

    exe = pathlib.Path(exe).resolve()
    fixture_src = resolve_fixture_scene(scene_arg)
    expected_name = pathlib.PurePosixPath(scene_arg).name
    package = out_dir / "fixture"
    if package.exists():
        raise SystemExit("fixture package already exists: %s" % package)
    shutil.copytree(exe.parent, package)
    staged_exe = package / exe.name
    if not staged_exe.is_file():
        raise SystemExit("staged exe missing: %s" % staged_exe)
    rom_scene = package / "rom" / "scene"
    rom_scene.mkdir(parents=True, exist_ok=True)
    shutil.copy2(fixture_src, rom_scene / expected_name)
    (rom_scene / "default_scene.txt").write_text(expected_name + "\n", encoding="ascii")
    (package / "rom" / "skip_boot_preload").write_text("", encoding="ascii")
    stage_fixture_scene_assets(package, fixture_src)
    return staged_exe, package


# ---------------------------------------------------------------------------
# Bridge adapters
# ---------------------------------------------------------------------------

class FakeBridge:
    """In-memory bridge for offline unit tests (no exe / no named pipe)."""

    def __init__(self, initial_positions: dict[str, tuple[float, float, float]] | None = None):
        self.ready = True
        self.playing = True
        self.paused = False
        self.pending_steps = 0
        self.frame = 0
        self.physics_tick = 0
        self.quit_called = False
        self.closed = False
        self.objects = dict(initial_positions or {"DynamicBox": (0.0, 2.0, 0.0)})
        self.velocities = {name: (0.0, 0.0, 0.0) for name in self.objects}
        self.angular = {name: (0.0, 0.0, 0.0) for name in self.objects}
        self._req = 0
        self.events: list[dict] = []

    def _next_id(self) -> int:
        self._req += 1
        return self._req

    def _obs(self, name: str) -> dict:
        px, py, pz = self.objects.get(name, (0.0, 0.0, 0.0))
        vx, vy, vz = self.velocities.get(name, (0.0, 0.0, 0.0))
        ax, ay, az = self.angular.get(name, (0.0, 0.0, 0.0))
        return {
            "ok": True,
            "found": name in self.objects,
            "target": name,
            "requestId": self._req,
            "position": {"x": px, "y": py, "z": pz},
            "velocity": {"x": vx, "y": vy, "z": vz},
            "angularVelocity": {"x": ax, "y": ay, "z": az},
            "rotation": {"x": 0.0, "y": 0.0, "z": 0.0},
            "rotationQuaternion": {"x": 0.0, "y": 0.0, "z": 0.0, "w": 1.0},
            "scale": {"x": 1.0, "y": 1.0, "z": 1.0},
            "rigidbody": {
                "kinematic": False,
                "mass": 1.0,
                "drag": 0.0,
                "angularDrag": 0.05,
                "friction": 0.5,
                "useGravity": False,
                "localInverseInertia": {"x": 1.0, "y": 1.0, "z": 1.0},
            },
        }

    def request(self, verb: str, payload: dict | None = None) -> dict:
        payload = payload or {}
        rid = self._next_id()
        if verb == "status":
            return {
                "ok": True,
                "ready": self.ready,
                "playing": self.playing,
                "paused": self.paused,
                "pendingSteps": self.pending_steps,
                "frame": self.frame,
                "physicsTick": self.physics_tick,
                "requestId": rid,
                "worldOp": {"status": "idle", "requestId": rid},
            }
        if verb == "pause":
            self.paused = True
            return {"ok": True, "paused": True, "requestId": rid}
        if verb == "resume":
            self.paused = False
            return {"ok": True, "paused": False, "requestId": rid}
        if verb == "step":
            frames = int(payload.get("frames") or 1)
            self.pending_steps = 0
            self.frame += frames
            self.physics_tick += frames
            # Integrate linear velocity simply while stepped.
            for name, (vx, vy, vz) in list(self.velocities.items()):
                px, py, pz = self.objects[name]
                dt = frames / 60.0
                self.objects[name] = (px + vx * dt, py + vy * dt, pz + vz * dt)
            return {"ok": True, "requestId": rid, "frames": frames}
        if verb == "list_objects":
            objects = [{"name": n, "components": ["Rigidbody"]} for n in sorted(self.objects)]
            return {
                "ok": True,
                "requestId": rid,
                "objects": objects,
                "total": len(objects),
                "truncated": False,
            }
        if verb == "observe":
            target = payload.get("target") or payload.get("object")
            return self._obs(str(target))
        if verb == "set_transform":
            name = payload["object"]
            if "position" in payload:
                pos = payload["position"]
                self.objects[name] = (float(pos[0]), float(pos[1]), float(pos[2]))
            if payload.get("zeroVelocity", True):
                self.velocities[name] = (0.0, 0.0, 0.0)
                self.angular[name] = (0.0, 0.0, 0.0)
            return {"ok": True, "status": "queued", "requestId": rid, "verb": verb}
        if verb == "set_velocity":
            name = payload["object"]
            if "linear" in payload:
                lin = payload["linear"]
                self.velocities[name] = (float(lin[0]), float(lin[1]), float(lin[2]))
            if "angular" in payload:
                ang = payload["angular"]
                self.angular[name] = (float(ang[0]), float(ang[1]), float(ang[2]))
            return {"ok": True, "status": "queued", "requestId": rid, "verb": verb}
        if verb == "apply_impulse":
            name = payload["object"]
            impulse = payload["impulse"]
            vx, vy, vz = self.velocities.get(name, (0.0, 0.0, 0.0))
            self.velocities[name] = (vx + float(impulse[0]), vy + float(impulse[1]),
                                     vz + float(impulse[2]))
            return {"ok": True, "status": "queued", "requestId": rid, "verb": verb}
        if verb == "apply_torque_impulse":
            name = payload["object"]
            torque = payload["torqueImpulse"]
            ax, ay, az = self.angular.get(name, (0.0, 0.0, 0.0))
            self.angular[name] = (ax + float(torque[0]), ay + float(torque[1]),
                                  az + float(torque[2]))
            return {"ok": True, "status": "queued", "requestId": rid, "verb": verb}
        if verb == "reset_scene":
            for name in list(self.objects):
                if name == "DynamicBox":
                    self.objects[name] = (0.0, 2.0, 0.0)
                self.velocities[name] = (0.0, 0.0, 0.0)
                self.angular[name] = (0.0, 0.0, 0.0)
            return {"ok": True, "status": "queued", "requestId": rid, "verb": verb}
        if verb in ("keyboard", "mouse", "controller", "capture", "perf"):
            return {"ok": True, "status": "completed", "requestId": rid}
        if verb == "events":
            return {
                "ok": True,
                "protocolVersion": 1,
                "requestId": rid,
                "events": list(self.events),
                "latestSequence": len(self.events),
                "dropped": 0,
            }
        if verb == "quit":
            self.quit_called = True
            return {"ok": True, "requiresReceipt": True, "requestId": rid}
        raise RuntimeError("FakeBridge unknown verb %s" % verb)

    def wait_world_op(self, start_request_id: int, timeout: float = 8.0) -> dict:
        return {
            "ok": True,
            "worldOp": {"status": "completed", "requestId": start_request_id},
        }

    def wait_steps_drain(self, timeout: float = 8.0) -> dict:
        self.pending_steps = 0
        return self.request("status")

    def close(self) -> None:
        self.closed = True


class LiveBridge:
    """Thin adapter over mouse_acceptance.Link + Client helpers."""

    def __init__(self, link, client):
        self.link = link
        self.client = client

    def request(self, verb: str, payload: dict | None = None) -> dict:
        if verb == "observe":
            target = (payload or {}).get("target") or (payload or {}).get("object")
            return self.link.observe(target)
        if verb == "keyboard":
            p = payload or {}
            reply = self.client.keyboard(p["keys"], p["updates"], self.link._next())
            self.link.counter = max(self.link.counter, int(reply.get("requestId") or self.link.counter))
            return reply
        if verb == "mouse":
            p = payload or {}
            reply = self.client.mouse(
                p["dx"], p["dy"], p.get("buttons") or [], p["updates"],
                self.link._next(), wheel=p.get("wheel", 0))
            self.link.counter = max(self.link.counter, int(reply.get("requestId") or self.link.counter))
            return reply
        if verb == "controller":
            p = payload or {}
            reply = self.client.controller(
                p["lx"], p["ly"], p["updates"], self.link._next(),
                rx=p.get("rx"), ry=p.get("ry"), lt=p.get("lt"), rt=p.get("rt"),
                buttons=p.get("buttons"))
            self.link.counter = max(self.link.counter, int(reply.get("requestId") or self.link.counter))
            return reply
        if verb == "capture":
            reply = self.client.capture(self.link._next())
            self.link.counter = max(self.link.counter, int(reply.get("requestId") or self.link.counter))
            return reply
        if verb in WORLD_VERBS:
            if verb == "reset_scene":
                reply = self.client.reset_scene(
                    self.link._next(), scene=(payload or {}).get("scene"))
            elif verb == "set_transform":
                p = payload or {}
                reply = self.client.set_transform(
                    p["object"], self.link._next(),
                    position=p.get("position"), rotation=p.get("rotation"),
                    scale=p.get("scale"), zero_velocity=p.get("zeroVelocity", True))
            elif verb == "set_velocity":
                p = payload or {}
                reply = self.client.set_velocity(
                    p["object"], self.link._next(),
                    linear=p.get("linear"), angular=p.get("angular"))
            elif verb == "apply_impulse":
                p = payload or {}
                reply = self.client.apply_impulse(
                    p["object"], p["impulse"], self.link._next(), point=p.get("point"))
            elif verb == "apply_torque_impulse":
                p = payload or {}
                reply = self.client.apply_torque_impulse(
                    p["object"], p["torqueImpulse"], self.link._next())
            else:
                raise RuntimeError("unreachable world verb")
            self.link.counter = max(self.link.counter, int(reply.get("requestId") or self.link.counter))
            return reply
        return self.link.req(verb, payload)

    def wait_world_op(self, start_request_id: int, timeout: float = 8.0) -> dict:
        # Client helpers already await worldOp; status poll kept for FakeBridge parity.
        deadline = time.monotonic() + timeout
        last = None
        while time.monotonic() < deadline:
            last = self.request("status")
            op = last.get("worldOp") or {}
            if op.get("requestId") == start_request_id and op.get("status") == "completed":
                return last
            if op.get("requestId") == start_request_id and op.get("status") == "failed":
                raise RuntimeError("worldOp failed: %s" % op.get("error"))
            time.sleep(0.01)
        raise TimeoutError("worldOp completion deadline: %s" % last)

    def wait_steps_drain(self, timeout: float = 8.0) -> dict:
        deadline = time.monotonic() + timeout
        last = None
        while time.monotonic() < deadline:
            last = self.request("status")
            if int(last.get("pendingSteps") or 0) == 0:
                return last
            time.sleep(0.02)
        raise TimeoutError("pendingSteps drain deadline: %s" % last)

    def close(self) -> None:
        self.link.close()


# ---------------------------------------------------------------------------
# Step execution
# ---------------------------------------------------------------------------

class ScenarioContext:
    def __init__(self):
        self.observes: dict[str, dict] = {}
        self.prior_observes: dict[str, dict] = {}
        self.samples: list[dict] = []
        self.list_objects: dict | None = None
        self.events: dict | None = None
        self.previous_values: dict[str, Any] = {}
        self.paused = False
        self.last_status: dict | None = None

    def store_observe(self, name: str, reply: dict) -> None:
        if name in self.observes:
            self.prior_observes[name] = self.observes[name]
        self.observes[name] = reply


def _payload_for_bridge(step: dict) -> dict | None:
    data = {k: v for k, v in step.items() if k != "type"}
    if step["type"] == "observe":
        return {"target": step["object"]}
    if step["type"] == "list_objects":
        return {"offset": step.get("offset", 0), "limit": step.get("limit", 32)}
    if step["type"] == "step":
        return {"frames": step["frames"]}
    if step["type"] == "perf" and "frames" in step:
        return {"frames": step["frames"]}
    if step["type"] == "reset_scene":
        return {"scene": step["scene"]} if "scene" in step else {}
    if not data:
        return None
    return data


def _check_deadline(deadline: float) -> None:
    if time.monotonic() > deadline:
        raise TimeoutError("scenario timeoutSec exceeded")


def execute_step(step: dict, bridge: Any, ctx: ScenarioContext, deadline: float) -> dict:
    """Execute one normalized step; return a result record for result.json."""
    _check_deadline(deadline)
    started = time.monotonic()
    record: dict[str, Any] = {
        "type": step["type"],
        "request": None,
        "response": None,
        "assertion": None,
        "pass": True,
        "elapsedSec": 0.0,
    }

    stype = step["type"]

    if stype in ("label", "comment"):
        record["text"] = step.get("text", "")
        record["elapsedSec"] = round(time.monotonic() - started, 4)
        return record

    if stype == "wait_ready":
        timeout = float(step.get("timeoutSec", 25.0))
        wait_deadline = min(deadline, time.monotonic() + timeout)
        last = None
        while time.monotonic() < wait_deadline:
            _check_deadline(deadline)
            last = bridge.request("status")
            record["response"] = last
            ctx.last_status = last
            if last.get("ok") and last.get("ready") and last.get("playing"):
                ctx.paused = bool(last.get("paused"))
                record["elapsedSec"] = round(time.monotonic() - started, 4)
                return record
            time.sleep(0.05)
        record["pass"] = False
        record["error"] = "wait_ready timeout: %s" % last
        record["elapsedSec"] = round(time.monotonic() - started, 4)
        raise TimeoutError(record["error"])

    if stype == "wait_frames":
        frames = int(step["frames"])
        status = bridge.request("status")
        ctx.last_status = status
        if status.get("paused"):
            reply = bridge.request("step", {"frames": frames})
            record["request"] = {"verb": "step", "payload": {"frames": frames}}
            record["response"] = reply
            if not reply.get("ok"):
                record["pass"] = False
                raise RuntimeError("step rejected during wait_frames: %s" % reply)
            drain = bridge.wait_steps_drain()
            record["drain"] = drain
            ctx.paused = True
        else:
            start_frame = int(status.get("frame") or 0)
            target = start_frame + frames
            last = status
            while int((last or {}).get("frame") or 0) < target:
                _check_deadline(deadline)
                time.sleep(0.01)
                last = bridge.request("status")
            record["response"] = last
            ctx.last_status = last
        record["elapsedSec"] = round(time.monotonic() - started, 4)
        return record

    if stype == "sample":
        name = step["object"]
        fields = step["fields"]
        every = int(step["everyFrames"])
        count = int(step["count"])
        samples = []
        for i in range(count):
            _check_deadline(deadline)
            step_reply = bridge.request("step", {"frames": every})
            if not step_reply.get("ok"):
                record["pass"] = False
                raise RuntimeError("sample step rejected: %s" % step_reply)
            bridge.wait_steps_drain()
            obs = bridge.request("observe", {"target": name})
            if not obs.get("found"):
                record["pass"] = False
                raise RuntimeError("sample observe not found: %s" % name)
            entry = {field: obs.get(field) for field in fields}
            entry["_index"] = i
            samples.append(entry)
            ctx.store_observe(name, obs)
        ctx.samples = samples
        record["response"] = {"count": len(samples), "object": name}
        record["samples"] = samples
        record["elapsedSec"] = round(time.monotonic() - started, 4)
        return record

    if stype == "assert":
        target = step["target"]
        op = step["op"]
        try:
            actual = resolve_path(target, {
                "observes": ctx.observes,
                "samples": ctx.samples,
                "list_objects": ctx.list_objects,
            })
        except KeyError as exc:
            record["pass"] = False
            record["assertion"] = {"pass": False, "reason": "resolve failed: %s" % exc,
                                   "target": target, "op": op}
            record["elapsedSec"] = round(time.monotonic() - started, 4)
            raise AssertionError("assert resolve failed for %s: %s" % (target, exc)) from exc
        previous = ctx.previous_values.get(target)
        if previous is None and op == "changed":
            try:
                previous = resolve_path(target, {
                    "observes": ctx.prior_observes,
                    "samples": ctx.samples,
                    "list_objects": ctx.list_objects,
                })
            except KeyError:
                previous = None
        result = eval_assert(
            op, actual, value=step.get("value"), tol=step.get("tol"), previous=previous)
        result["target"] = target
        record["assertion"] = result
        record["pass"] = bool(result.get("pass"))
        ctx.previous_values[target] = actual
        record["elapsedSec"] = round(time.monotonic() - started, 4)
        if not record["pass"]:
            raise AssertionError("assert failed: %s" % result)
        return record

    if stype == "assert_event":
        reply = bridge.request("events")
        record["request"] = {"verb": "events"}
        record["response"] = reply
        ctx.events = reply
        events = reply.get("events") if isinstance(reply, dict) else None
        if not isinstance(events, list):
            record["pass"] = False
            raise AssertionError("assert_event: events missing")
        matched = events
        if "checkpoint" in step:
            matched = [e for e in matched if e.get("checkpoint") == step["checkpoint"]]
        if "actor" in step:
            matched = [e for e in matched if e.get("actor") == step["actor"]]
        min_count = int(step.get("minCount", 1))
        ok = len(matched) >= min_count
        record["assertion"] = {
            "pass": ok,
            "matched": len(matched),
            "minCount": min_count,
            "checkpoint": step.get("checkpoint"),
            "actor": step.get("actor"),
        }
        record["pass"] = ok
        record["elapsedSec"] = round(time.monotonic() - started, 4)
        if not ok:
            raise AssertionError("assert_event failed: %s" % record["assertion"])
        return record

    # Bridge verbs
    payload = _payload_for_bridge(step)
    record["request"] = {"verb": stype, "payload": payload}
    reply = bridge.request(stype, payload)
    record["response"] = reply
    if stype == "observe":
        ctx.store_observe(step["object"], reply)
        if reply.get("found") is not True:
            record["pass"] = False
            raise RuntimeError("observe not found: %s" % step["object"])
    elif stype == "list_objects":
        ctx.list_objects = reply
        if reply.get("ok") is not True:
            record["pass"] = False
            raise RuntimeError("list_objects failed: %s" % reply)
    elif stype == "pause":
        ctx.paused = True
        if reply.get("ok") is not True:
            record["pass"] = False
            raise RuntimeError("pause failed: %s" % reply)
    elif stype == "resume":
        ctx.paused = False
        if reply.get("ok") is not True:
            record["pass"] = False
            raise RuntimeError("resume failed: %s" % reply)
    elif stype == "step":
        if reply.get("ok") is not True:
            record["pass"] = False
            raise RuntimeError("step failed: %s" % reply)
        bridge.wait_steps_drain()
    elif stype in WORLD_VERBS:
        # LiveBridge Client helpers already await; FakeBridge returns queued — await.
        if reply.get("status") == "queued" and not hasattr(bridge, "client"):
            bridge.wait_world_op(reply["requestId"])
        elif reply.get("ok") is not True:
            record["pass"] = False
            raise RuntimeError("%s failed: %s" % (stype, reply))
    else:
        if isinstance(reply, dict) and reply.get("ok") is False:
            record["pass"] = False
            raise RuntimeError("%s failed: %s" % (stype, reply))

    record["elapsedSec"] = round(time.monotonic() - started, 4)
    return record


def run_scenario_with_bridge(
    scenario: dict,
    bridge: Any,
    out_dir: pathlib.Path | None = None,
    keep_open: bool = False,
    child: Any = None,
    shutdown_timeout: float = 15.0,
) -> dict:
    """Execute a validated scenario against any bridge adapter. Always writes result.json when out_dir set."""
    started = time.monotonic()
    deadline = started + float(scenario.get("timeoutSec", 120.0))
    report: dict[str, Any] = {
        "schema": SCENARIO_SCHEMA_ID,
        "name": scenario.get("name"),
        "scene": scenario.get("scene"),
        "startedAt": started,
        "steps": [],
        "status": "FAIL",
        "keepOpen": bool(keep_open),
    }
    ctx = ScenarioContext()
    try:
        for index, step in enumerate(scenario["steps"]):
            try:
                record = execute_step(step, bridge, ctx, deadline)
            except Exception as exc:
                record = {
                    "type": step.get("type"),
                    "index": index,
                    "pass": False,
                    "error": str(exc),
                    "traceback": traceback.format_exc(),
                }
                report["steps"].append(record)
                raise
            record["index"] = index
            report["steps"].append(record)
            if not record.get("pass", True):
                raise AssertionError("step %d failed: %s" % (index, record))

        if keep_open:
            report["status"] = "PASS"
            report["quit"] = {"skipped": True, "reason": "keep-open"}
        else:
            quit_reply = bridge.request("quit")
            quit_ok = (
                isinstance(quit_reply, dict)
                and quit_reply.get("ok") is True
                and quit_reply.get("requiresReceipt") is True
            )
            report["quit"] = {
                "pass": quit_ok,
                "response": quit_reply,
            }
            if not quit_ok:
                raise RuntimeError("invalid quit reply: %s" % quit_reply)
            if child is not None:
                try:
                    exit_code = child.wait(timeout=shutdown_timeout)
                except subprocess.TimeoutExpired as exc:
                    raise RuntimeError("runtime did not exit after quit") from exc
                report["exitCode"] = exit_code
                if exit_code != 0:
                    raise RuntimeError("runtime exit code %s" % exit_code)
            report["status"] = "PASS"
        return report
    except Exception as exc:
        report["error"] = str(exc)
        report["traceback"] = traceback.format_exc()
        report["status"] = "FAIL"
        if child is not None and child.poll() is None:
            child.kill()
            try:
                child.wait(timeout=5)
            except Exception:
                pass
        elif not keep_open:
            try:
                if not getattr(bridge, "quit_called", False):
                    bridge.request("quit")
            except Exception:
                pass
        return report
    finally:
        try:
            bridge.close()
        except Exception:
            pass
        report["elapsedSec"] = round(time.monotonic() - started, 3)
        if out_dir is not None:
            out_dir = pathlib.Path(out_dir)
            out_dir.mkdir(parents=True, exist_ok=True)
            (out_dir / "result.json").write_text(
                json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")


def run_live(args: argparse.Namespace) -> int:
    from runtime_bridge_mouse_acceptance import ENV_CLEAR, Link, session_of
    from runtime_test_client import Client

    exe = pathlib.Path(args.exe).resolve()
    if not exe.is_file():
        raise SystemExit("exe not found: %s" % exe)
    out_dir = pathlib.Path(args.out_dir).resolve()
    if out_dir.exists():
        raise SystemExit("out dir already exists: %s" % out_dir)
    out_dir.mkdir(parents=True)

    scenario = load_and_validate_scenario(pathlib.Path(args.scenario))
    staged_exe, package = stage_fixture_package(exe, out_dir, scenario["scene"])

    report_meta = {
        "exe": str(exe),
        "stagedExe": str(staged_exe),
        "sceneArg": scenario["scene"],
        "launcherSession": None,
    }
    try:
        report_meta["launcherSession"] = session_of(os.getpid())
    except Exception:
        report_meta["launcherSession"] = None

    child = None
    bridge = None
    run_id = uuid.uuid4().hex
    try:
        env = dict(os.environ)
        for key in ENV_CLEAR:
            env.pop(key, None)
        env["IVY_RUNTIME_TEST_BRIDGE"] = "1"
        env["IVY_RUNTIME_TEST_RUN_ID"] = run_id
        env["IVY_RUNTIME_TEST_OUTPUT_DIR"] = str(out_dir)
        env["IVY_RUNTIME_TEST_FIXED_CAMERA"] = "1"
        env["IVY_SKIP_CLOSE_CONFIRM"] = "1"
        child = subprocess.Popen(
            [str(staged_exe), "--scene", scenario["scene"]],
            cwd=str(package),
            env=env,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
        )
        link_report = {"startedAt": time.monotonic(), "runId": run_id}
        link = Link(child.pid, run_id, link_report)
        bridge = LiveBridge(link, link.client)
        report = run_scenario_with_bridge(
            scenario,
            bridge,
            out_dir=out_dir,
            keep_open=bool(args.keep_open),
            child=child,
            shutdown_timeout=float(args.shutdown_timeout),
        )
        report.update(report_meta)
        report["runId"] = run_id
        report["pid"] = child.pid
        (out_dir / "result.json").write_text(
            json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
        print(json.dumps({
            "status": report["status"],
            "outDir": str(out_dir),
            "name": scenario["name"],
        }, ensure_ascii=False))
        return 0 if report["status"] == "PASS" else 1
    finally:
        if child is not None and child.stderr:
            try:
                err = child.stderr.read()
                if err:
                    (out_dir / "runtime.stderr.txt").write_bytes(err)
                child.stderr.close()
            except Exception:
                pass
        if child is not None and child.poll() is None and not args.keep_open:
            child.kill()
            try:
                child.wait(timeout=5)
            except Exception:
                pass


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", help="Debug IvyEngineRuntime.exe (required unless --validate-only)")
    parser.add_argument("--scenario", required=True, help="path to scenario JSON")
    parser.add_argument("--out-dir", help="new output directory for result.json / captures")
    parser.add_argument("--keep-open", action="store_true",
                        help="on PASS, skip quit and leave the runtime process running")
    parser.add_argument("--shutdown-timeout", type=float, default=15.0)
    parser.add_argument("--validate-only", action="store_true",
                        help="validate scenario JSON and exit (no exe)")
    parser.add_argument("--fake", action="store_true",
                        help="run against in-memory FakeBridge (offline)")
    args = parser.parse_args(argv)

    if args.validate_only:
        scenario = load_and_validate_scenario(pathlib.Path(args.scenario))
        print(json.dumps({"ok": True, "name": scenario["name"], "steps": len(scenario["steps"])}))
        return 0

    if args.fake:
        if not args.out_dir:
            raise SystemExit("--out-dir is required with --fake")
        out_dir = pathlib.Path(args.out_dir).resolve()
        if out_dir.exists():
            raise SystemExit("out dir already exists: %s" % out_dir)
        scenario = load_and_validate_scenario(pathlib.Path(args.scenario))
        bridge = FakeBridge()
        # Seed common fixture objects so example scenarios can dry-run offline.
        if "FreeSpinBox" in json.dumps(scenario):
            bridge.objects["FreeSpinBox"] = (0.0, 4.0, 0.0)
            bridge.velocities["FreeSpinBox"] = (0.0, 0.0, 0.0)
            bridge.angular["FreeSpinBox"] = (0.0, 0.0, 0.0)
        if "Player" in json.dumps(scenario):
            bridge.objects["Player"] = (0.0, 0.5, 0.0)
            bridge.velocities["Player"] = (0.0, 0.0, 0.0)
            bridge.angular["Player"] = (0.0, 0.0, 0.0)
        report = run_scenario_with_bridge(
            scenario, bridge, out_dir=out_dir, keep_open=bool(args.keep_open))
        print(json.dumps({"status": report["status"], "outDir": str(out_dir)}, ensure_ascii=False))
        return 0 if report["status"] == "PASS" else 1

    if not args.exe or not args.out_dir:
        raise SystemExit("--exe and --out-dir are required unless --validate-only/--fake")
    return run_live(args)


if __name__ == "__main__":
    raise SystemExit(main())
