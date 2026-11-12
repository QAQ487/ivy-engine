"""Schema v1 validation, path resolution, and assertion ops for runtime-bridge scenarios.

Linux-safe (stdlib only). Unknown keys/types are rejected with path-based errors.
"""
from __future__ import annotations

import math
import re
from typing import Any

from runtime_bridge_keyboard_keys import KEYBOARD_KEYS, is_forbidden_keyboard_combo

SCHEMA_VERSION = 1
SCENARIO_SCHEMA_ID = "ivy.runtime_bridge.scenario/1"

MAX_NAME_LEN = 128
MAX_STEPS = 1000
MAX_OBJECT_LEN = 64
MAX_TIMEOUT_SEC = 600.0
MIN_TIMEOUT_SEC = 1.0
DEFAULT_TIMEOUT_SEC = 120.0
MAX_FRAMES = 600
MAX_SAMPLE_COUNT = 600
MAX_UPDATES = 120
MAX_MOUSE_DURATION_MS = 30000
MAX_KEYS = 9
MAX_LIST_LIMIT = 64

BRIDGE_STEP_TYPES = frozenset({
    "pause",
    "resume",
    "step",
    "set_transform",
    "set_velocity",
    "apply_impulse",
    "apply_torque_impulse",
    "reset_scene",
    "keyboard",
    "mouse",
    "controller",
    "capture",
    "perf",
    "list_objects",
    "observe",
})

RUNNER_STEP_TYPES = frozenset({
    "wait_ready",
    "wait_frames",
    "sample",
    "assert",
    "assert_event",
    "label",
    "comment",
})

STEP_TYPES = BRIDGE_STEP_TYPES | RUNNER_STEP_TYPES

ASSERT_OPS = frozenset({
    "eq",
    "approx",
    "gt",
    "lt",
    "between",
    "changed",
    "delta_gt",
    "axis_dot",
})

MOUSE_BUTTONS = frozenset({"LEFT", "RIGHT", "MIDDLE"})

CONTROLLER_BUTTONS = frozenset({
    "A", "B", "X", "Y", "LB", "RB", "START", "BACK",
    "LEFT_THUMB", "RIGHT_THUMB", "DPAD_UP", "DPAD_DOWN", "DPAD_LEFT", "DPAD_RIGHT",
})

_SCENE_RE = re.compile(r"^rom/(?:[A-Za-z0-9_.-]+/)*[A-Za-z0-9_.-]+\.json$")
_OBJECT_RE = re.compile(r"^[A-Za-z0-9_ ][A-Za-z0-9_ .-]{0,63}$")


class ScenarioValidationError(ValueError):
    """Validation failure with a JSON-pointer-like path for diagnostics."""

    def __init__(self, path: str, message: str):
        self.path = path or "$"
        self.message = message
        super().__init__("%s: %s" % (self.path, message))


def _err(path: str, message: str) -> None:
    raise ScenarioValidationError(path, message)


def _require_object(value: Any, path: str) -> dict:
    if not isinstance(value, dict) or isinstance(value, bool):
        _err(path, "expected object")
    return value


def _reject_unknown(obj: dict, allowed: set[str], path: str) -> None:
    unknown = sorted(set(obj) - allowed)
    if unknown:
        _err(path, "unknown key(s): %s" % ", ".join(unknown))


def _is_bool(value: Any) -> bool:
    return isinstance(value, bool)


def _is_int(value: Any) -> bool:
    return isinstance(value, int) and not isinstance(value, bool)


def _is_number(value: Any) -> bool:
    return (isinstance(value, (int, float)) and not isinstance(value, bool)
            and math.isfinite(float(value)))


def _require_str(value: Any, path: str, *, min_len: int = 1, max_len: int = 256) -> str:
    if not isinstance(value, str):
        _err(path, "expected string")
    if not min_len <= len(value) <= max_len:
        _err(path, "string length must be in [%d,%d]" % (min_len, max_len))
    return value


def _require_finite(value: Any, path: str) -> float:
    if not _is_number(value):
        _err(path, "expected finite number")
    return float(value)


def _require_int_range(value: Any, path: str, lo: int, hi: int) -> int:
    if not _is_int(value):
        _err(path, "expected integer")
    if not lo <= value <= hi:
        _err(path, "integer must be in [%d,%d]" % (lo, hi))
    return value


def _require_vec(value: Any, path: str, length: int | tuple[int, ...] = 3) -> list[float]:
    allowed = (length,) if isinstance(length, int) else tuple(length)
    if not isinstance(value, list):
        _err(path, "expected array")
    if len(value) not in allowed:
        _err(path, "expected length %s" % (" or ".join(str(x) for x in allowed)))
    out = []
    for i, item in enumerate(value):
        out.append(_require_finite(item, "%s[%d]" % (path, i)))
    return out


def validate_scene_path(scene: Any, path: str = "$.scene") -> str:
    """Require rom/...json with no '..', no absolute drive, posix separators."""
    text = _require_str(scene, path, min_len=8, max_len=256)
    normalized = text.replace("\\", "/")
    if normalized != text:
        _err(path, "use forward slashes only")
    if normalized.startswith("/") or re.match(r"^[A-Za-z]:", normalized):
        _err(path, "scene path must be rom/-relative (not absolute)")
    if ".." in normalized.split("/"):
        _err(path, "scene path must not contain '..'")
    if not normalized.startswith("rom/"):
        _err(path, "scene path must start with rom/")
    if not normalized.endswith(".json"):
        _err(path, "scene path must end with .json")
    if not _SCENE_RE.fullmatch(normalized):
        _err(path, "scene path has invalid characters or shape")
    return normalized


def _require_object_name(value: Any, path: str) -> str:
    name = _require_str(value, path, min_len=1, max_len=MAX_OBJECT_LEN)
    if ".." in name or "/" in name or "\\" in name:
        _err(path, "object name must not contain path separators or '..'")
    if not _OBJECT_RE.fullmatch(name):
        _err(path, "object name has invalid characters")
    return name


def _validate_pause_resume(step: dict, path: str) -> dict:
    _reject_unknown(step, {"type"}, path)
    return {"type": step["type"]}


def _validate_step_frames(step: dict, path: str) -> dict:
    _reject_unknown(step, {"type", "frames"}, path)
    frames = _require_int_range(step.get("frames"), "%s.frames" % path, 1, MAX_FRAMES)
    return {"type": "step", "frames": frames}


def _validate_set_transform(step: dict, path: str) -> dict:
    _reject_unknown(step, {"type", "object", "position", "rotation", "scale", "zeroVelocity"}, path)
    out: dict[str, Any] = {
        "type": "set_transform",
        "object": _require_object_name(step.get("object"), "%s.object" % path),
    }
    if "position" in step:
        out["position"] = _require_vec(step["position"], "%s.position" % path, 3)
    if "rotation" in step:
        out["rotation"] = _require_vec(step["rotation"], "%s.rotation" % path, (3, 4))
    if "scale" in step:
        out["scale"] = _require_vec(step["scale"], "%s.scale" % path, 3)
    if not any(k in out for k in ("position", "rotation", "scale")):
        _err(path, "set_transform requires position, rotation, and/or scale")
    if "zeroVelocity" in step:
        if not _is_bool(step["zeroVelocity"]):
            _err("%s.zeroVelocity" % path, "expected boolean")
        out["zeroVelocity"] = step["zeroVelocity"]
    else:
        out["zeroVelocity"] = True
    return out


def _validate_set_velocity(step: dict, path: str) -> dict:
    _reject_unknown(step, {"type", "object", "linear", "angular"}, path)
    out: dict[str, Any] = {
        "type": "set_velocity",
        "object": _require_object_name(step.get("object"), "%s.object" % path),
    }
    if "linear" in step:
        out["linear"] = _require_vec(step["linear"], "%s.linear" % path, 3)
    if "angular" in step:
        out["angular"] = _require_vec(step["angular"], "%s.angular" % path, 3)
    if "linear" not in out and "angular" not in out:
        _err(path, "set_velocity requires linear and/or angular")
    return out


def _validate_apply_impulse(step: dict, path: str) -> dict:
    _reject_unknown(step, {"type", "object", "impulse", "point"}, path)
    out: dict[str, Any] = {
        "type": "apply_impulse",
        "object": _require_object_name(step.get("object"), "%s.object" % path),
        "impulse": _require_vec(step.get("impulse"), "%s.impulse" % path, 3),
    }
    if "point" in step:
        out["point"] = _require_vec(step["point"], "%s.point" % path, 3)
    return out


def _validate_apply_torque(step: dict, path: str) -> dict:
    _reject_unknown(step, {"type", "object", "torqueImpulse"}, path)
    return {
        "type": "apply_torque_impulse",
        "object": _require_object_name(step.get("object"), "%s.object" % path),
        "torqueImpulse": _require_vec(step.get("torqueImpulse"), "%s.torqueImpulse" % path, 3),
    }


def _validate_reset_scene(step: dict, path: str) -> dict:
    _reject_unknown(step, {"type", "scene"}, path)
    out: dict[str, Any] = {"type": "reset_scene"}
    if "scene" in step:
        out["scene"] = validate_scene_path(step["scene"], "%s.scene" % path)
    return out


def _validate_keyboard(step: dict, path: str) -> dict:
    _reject_unknown(step, {"type", "keys", "updates"}, path)
    keys = step.get("keys")
    if not isinstance(keys, list) or not keys:
        _err("%s.keys" % path, "expected non-empty array")
    if len(keys) > MAX_KEYS:
        _err("%s.keys" % path, "at most %d keys" % MAX_KEYS)
    out_keys = []
    seen = set()
    for i, key in enumerate(keys):
        kpath = "%s.keys[%d]" % (path, i)
        name = _require_str(key, kpath, min_len=1, max_len=16)
        if name not in KEYBOARD_KEYS:
            _err(kpath, "unknown key %r" % name)
        if name in seen:
            _err(kpath, "duplicate key %r" % name)
        seen.add(name)
        out_keys.append(name)
    if is_forbidden_keyboard_combo(out_keys):
        _err("%s.keys" % path, "forbidden combo ALT+F4")
    updates = _require_int_range(step.get("updates"), "%s.updates" % path, 1, MAX_UPDATES)
    return {"type": "keyboard", "keys": out_keys, "updates": updates}


def _validate_mouse(step: dict, path: str) -> dict:
    _reject_unknown(step, {"type", "dx", "dy", "buttons", "updates", "durationMs", "wheel"}, path)
    dx = _require_finite(step.get("dx"), "%s.dx" % path)
    dy = _require_finite(step.get("dy"), "%s.dy" % path)
    if not -600 <= dx <= 600:
        _err("%s.dx" % path, "dx must be in [-600,600]")
    if not -600 <= dy <= 600:
        _err("%s.dy" % path, "dy must be in [-600,600]")
    buttons = step.get("buttons", [])
    if not isinstance(buttons, list):
        _err("%s.buttons" % path, "expected array")
    out_buttons = []
    seen = set()
    for i, button in enumerate(buttons):
        bpath = "%s.buttons[%d]" % (path, i)
        name = _require_str(button, bpath, min_len=1, max_len=16)
        if name not in MOUSE_BUTTONS:
            _err(bpath, "unknown button %r" % name)
        if name in seen:
            _err(bpath, "duplicate button %r" % name)
        seen.add(name)
        out_buttons.append(name)
    has_updates = "updates" in step
    has_duration = "durationMs" in step
    if has_updates == has_duration:
        _err(path, "mouse requires exactly one of updates or durationMs")
    out: dict[str, Any] = {
        "type": "mouse",
        "dx": dx,
        "dy": dy,
        "buttons": out_buttons,
    }
    if has_updates:
        out["updates"] = _require_int_range(step.get("updates"), "%s.updates" % path, 1, MAX_UPDATES)
    else:
        out["durationMs"] = _require_int_range(
            step.get("durationMs"), "%s.durationMs" % path, 1, MAX_MOUSE_DURATION_MS)
    if "wheel" in step:
        wheel = _require_finite(step["wheel"], "%s.wheel" % path)
        if not -120 <= wheel <= 120:
            _err("%s.wheel" % path, "wheel must be in [-120,120]")
        out["wheel"] = wheel
    else:
        out["wheel"] = 0.0
    return out


def _validate_controller(step: dict, path: str) -> dict:
    allowed = {
        "type", "lx", "ly", "updates", "rx", "ry", "lt", "rt", "buttons",
    }
    _reject_unknown(step, allowed, path)
    lx = _require_finite(step.get("lx"), "%s.lx" % path)
    ly = _require_finite(step.get("ly"), "%s.ly" % path)
    if not -1 <= lx <= 1:
        _err("%s.lx" % path, "lx must be in [-1,1]")
    if not -1 <= ly <= 1:
        _err("%s.ly" % path, "ly must be in [-1,1]")
    updates = _require_int_range(step.get("updates"), "%s.updates" % path, 1, MAX_UPDATES)
    out: dict[str, Any] = {"type": "controller", "lx": lx, "ly": ly, "updates": updates}
    for axis, lo, hi in (("rx", -1.0, 1.0), ("ry", -1.0, 1.0), ("lt", 0.0, 1.0), ("rt", 0.0, 1.0)):
        if axis in step:
            val = _require_finite(step[axis], "%s.%s" % (path, axis))
            if not lo <= val <= hi:
                _err("%s.%s" % (path, axis), "%s must be in [%g,%g]" % (axis, lo, hi))
            out[axis] = val
    if "buttons" in step:
        buttons = step["buttons"]
        if not isinstance(buttons, list):
            _err("%s.buttons" % path, "expected array")
        if len(buttons) > 14:
            _err("%s.buttons" % path, "at most 14 buttons")
        out_buttons = []
        seen = set()
        for i, button in enumerate(buttons):
            bpath = "%s.buttons[%d]" % (path, i)
            name = _require_str(button, bpath, min_len=1, max_len=16)
            if name not in CONTROLLER_BUTTONS:
                _err(bpath, "unknown button %r" % name)
            if name in seen:
                _err(bpath, "duplicate button %r" % name)
            seen.add(name)
            out_buttons.append(name)
        out["buttons"] = out_buttons
    return out


def _validate_capture(step: dict, path: str) -> dict:
    _reject_unknown(step, {"type"}, path)
    return {"type": "capture"}


def _validate_perf(step: dict, path: str) -> dict:
    _reject_unknown(step, {"type", "frames"}, path)
    out: dict[str, Any] = {"type": "perf"}
    if "frames" in step:
        out["frames"] = _require_int_range(step["frames"], "%s.frames" % path, 1, MAX_FRAMES)
    return out


def _validate_list_objects(step: dict, path: str) -> dict:
    _reject_unknown(step, {"type", "offset", "limit"}, path)
    out: dict[str, Any] = {"type": "list_objects"}
    if "offset" in step:
        out["offset"] = _require_int_range(step["offset"], "%s.offset" % path, 0, 100000)
    else:
        out["offset"] = 0
    if "limit" in step:
        out["limit"] = _require_int_range(step["limit"], "%s.limit" % path, 1, MAX_LIST_LIMIT)
    else:
        out["limit"] = 32
    return out


def _validate_observe(step: dict, path: str) -> dict:
    _reject_unknown(step, {"type", "object", "target"}, path)
    if "object" in step and "target" in step:
        _err(path, "specify only one of object or target")
    raw = step.get("object", step.get("target"))
    return {"type": "observe", "object": _require_object_name(raw, "%s.object" % path)}


def _validate_wait_ready(step: dict, path: str) -> dict:
    _reject_unknown(step, {"type", "timeoutSec"}, path)
    out: dict[str, Any] = {"type": "wait_ready"}
    if "timeoutSec" in step:
        timeout = _require_finite(step["timeoutSec"], "%s.timeoutSec" % path)
        if not 1.0 <= timeout <= 120.0:
            _err("%s.timeoutSec" % path, "timeoutSec must be in [1,120]")
        out["timeoutSec"] = timeout
    else:
        out["timeoutSec"] = 25.0
    return out


def _validate_wait_frames(step: dict, path: str) -> dict:
    _reject_unknown(step, {"type", "frames"}, path)
    return {
        "type": "wait_frames",
        "frames": _require_int_range(step.get("frames"), "%s.frames" % path, 1, MAX_FRAMES),
    }


_SAMPLE_FIELDS = frozenset({
    "position",
    "velocity",
    "rotation",
    "rotationQuaternion",
    "scale",
    "angularVelocity",
    "rigidbody",
})


def _validate_sample(step: dict, path: str) -> dict:
    _reject_unknown(step, {"type", "object", "fields", "everyFrames", "count"}, path)
    fields = step.get("fields")
    if not isinstance(fields, list) or not fields:
        _err("%s.fields" % path, "expected non-empty array")
    out_fields = []
    seen = set()
    for i, field in enumerate(fields):
        fpath = "%s.fields[%d]" % (path, i)
        name = _require_str(field, fpath, min_len=1, max_len=64)
        if name not in _SAMPLE_FIELDS:
            _err(fpath, "unknown sample field %r" % name)
        if name in seen:
            _err(fpath, "duplicate field %r" % name)
        seen.add(name)
        out_fields.append(name)
    return {
        "type": "sample",
        "object": _require_object_name(step.get("object"), "%s.object" % path),
        "fields": out_fields,
        "everyFrames": _require_int_range(
            step.get("everyFrames"), "%s.everyFrames" % path, 1, MAX_FRAMES),
        "count": _require_int_range(step.get("count"), "%s.count" % path, 1, MAX_SAMPLE_COUNT),
    }


def _validate_assert(step: dict, path: str) -> dict:
    _reject_unknown(step, {"type", "target", "op", "value", "tol"}, path)
    target = _require_str(step.get("target"), "%s.target" % path, min_len=1, max_len=256)
    if ".." in target.split("."):
        _err("%s.target" % path, "target must not contain '..'")
    op = _require_str(step.get("op"), "%s.op" % path, min_len=1, max_len=32)
    if op not in ASSERT_OPS:
        _err("%s.op" % path, "unknown assert op %r" % op)
    out: dict[str, Any] = {"type": "assert", "target": target, "op": op}
    if op == "changed":
        if "value" in step:
            _err(path, "changed assert must not set value")
    elif op == "approx" and "value" not in step:
        # Compare against the previous observe/assert of the same target.
        pass
    elif op == "between":
        value = step.get("value")
        if not isinstance(value, list) or len(value) != 2:
            _err("%s.value" % path, "between value must be [lo, hi]")
        lo = _require_finite(value[0], "%s.value[0]" % path)
        hi = _require_finite(value[1], "%s.value[1]" % path)
        if lo > hi:
            _err("%s.value" % path, "between lo must be <= hi")
        out["value"] = [lo, hi]
    elif op == "axis_dot":
        out["value"] = _require_vec(step.get("value"), "%s.value" % path, 3)
    elif op == "delta_gt":
        out["value"] = _require_finite(step.get("value"), "%s.value" % path)
        if out["value"] < 0:
            _err("%s.value" % path, "delta_gt value must be >= 0")
    elif "value" not in step:
        _err("%s.value" % path, "value is required for op %s" % op)
    else:
        value = step["value"]
        if isinstance(value, list):
            out["value"] = _require_vec(value, "%s.value" % path, (1, 2, 3, 4))
        elif _is_bool(value) or isinstance(value, str):
            out["value"] = value
        else:
            out["value"] = _require_finite(value, "%s.value" % path)
    if "tol" in step:
        tol = _require_finite(step["tol"], "%s.tol" % path)
        if tol < 0:
            _err("%s.tol" % path, "tol must be >= 0")
        out["tol"] = tol
    elif op in ("approx", "axis_dot"):
        out["tol"] = 0.98 if op == "axis_dot" else 1e-3
    return out


def _validate_assert_event(step: dict, path: str) -> dict:
    _reject_unknown(step, {"type", "checkpoint", "minCount", "actor"}, path)
    out: dict[str, Any] = {"type": "assert_event"}
    if "checkpoint" in step:
        out["checkpoint"] = _require_str(
            step["checkpoint"], "%s.checkpoint" % path, min_len=1, max_len=64)
    if "actor" in step:
        out["actor"] = _require_str(step["actor"], "%s.actor" % path, min_len=1, max_len=64)
    if "minCount" in step:
        out["minCount"] = _require_int_range(step["minCount"], "%s.minCount" % path, 1, 64)
    else:
        out["minCount"] = 1
    return out


def _validate_label(step: dict, path: str) -> dict:
    _reject_unknown(step, {"type", "text"}, path)
    return {
        "type": step["type"],
        "text": _require_str(step.get("text"), "%s.text" % path, min_len=0, max_len=512),
    }


_STEP_VALIDATORS = {
    "pause": _validate_pause_resume,
    "resume": _validate_pause_resume,
    "step": _validate_step_frames,
    "set_transform": _validate_set_transform,
    "set_velocity": _validate_set_velocity,
    "apply_impulse": _validate_apply_impulse,
    "apply_torque_impulse": _validate_apply_torque,
    "reset_scene": _validate_reset_scene,
    "keyboard": _validate_keyboard,
    "mouse": _validate_mouse,
    "controller": _validate_controller,
    "capture": _validate_capture,
    "perf": _validate_perf,
    "list_objects": _validate_list_objects,
    "observe": _validate_observe,
    "wait_ready": _validate_wait_ready,
    "wait_frames": _validate_wait_frames,
    "sample": _validate_sample,
    "assert": _validate_assert,
    "assert_event": _validate_assert_event,
    "label": _validate_label,
    "comment": _validate_label,
}


def validate_step(step: Any, path: str) -> dict:
    obj = _require_object(step, path)
    step_type = obj.get("type")
    if not isinstance(step_type, str):
        _err("%s.type" % path, "expected string type")
    if step_type not in STEP_TYPES:
        _err("%s.type" % path, "unknown step type %r" % step_type)
    return _STEP_VALIDATORS[step_type](obj, path)


def validate_scenario(data: Any) -> dict:
    """Validate and normalize a scenario document. Raises ScenarioValidationError."""
    obj = _require_object(data, "$")
    _reject_unknown(obj, {"version", "name", "scene", "timeoutSec", "steps"}, "$")
    version = obj.get("version")
    if version != SCHEMA_VERSION:
        _err("$.version", "version must be %d" % SCHEMA_VERSION)
    name = _require_str(obj.get("name"), "$.name", min_len=1, max_len=MAX_NAME_LEN)
    scene = validate_scene_path(obj.get("scene"), "$.scene")
    if "timeoutSec" in obj:
        timeout = _require_finite(obj["timeoutSec"], "$.timeoutSec")
        if not MIN_TIMEOUT_SEC <= timeout <= MAX_TIMEOUT_SEC:
            _err("$.timeoutSec", "timeoutSec must be in [%g,%g]"
                 % (MIN_TIMEOUT_SEC, MAX_TIMEOUT_SEC))
    else:
        timeout = DEFAULT_TIMEOUT_SEC
    steps_raw = obj.get("steps")
    if not isinstance(steps_raw, list) or not steps_raw:
        _err("$.steps", "expected non-empty array")
    if len(steps_raw) > MAX_STEPS:
        _err("$.steps", "at most %d steps" % MAX_STEPS)
    steps = [validate_step(step, "$.steps[%d]" % i) for i, step in enumerate(steps_raw)]
    return {
        "version": SCHEMA_VERSION,
        "name": name,
        "scene": scene,
        "timeoutSec": timeout,
        "steps": steps,
    }


# ---------------------------------------------------------------------------
# Observation / sample path resolution + assertion evaluation
# ---------------------------------------------------------------------------

def _as_vec3(value: Any) -> list[float] | None:
    if isinstance(value, dict):
        try:
            return [float(value["x"]), float(value["y"]), float(value["z"])]
        except (KeyError, TypeError, ValueError):
            return None
    if isinstance(value, (list, tuple)) and len(value) == 3:
        try:
            out = [float(v) for v in value]
        except (TypeError, ValueError):
            return None
        if all(math.isfinite(v) for v in out):
            return out
    return None


def _vec_len(v: list[float]) -> float:
    return math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2])


def _vec_norm(v: list[float]) -> list[float] | None:
    length = _vec_len(v)
    if length < 1e-12:
        return None
    return [v[0] / length, v[1] / length, v[2] / length]


def _dot(a: list[float], b: list[float]) -> float:
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def _parse_sample_index(token: str) -> int | None:
    if token == "sample":
        return -1
    if token.startswith("sample[") and token.endswith("]"):
        inner = token[7:-1]
        if inner == "-1" or re.fullmatch(r"-?\d+", inner):
            return int(inner)
    return None


def resolve_path(target: str, context: dict[str, Any]) -> Any:
    """Resolve an assert target against observe/sample/list context.

    Examples:
      DynamicBox.position.x
      sample[-1].angularVelocity.z
      sample.position.y   (alias of sample[-1])
      list_objects.count  (len of last list names when present)
    """
    if not isinstance(target, str) or not target:
        raise KeyError("empty target")
    parts = target.split(".")
    if not parts:
        raise KeyError("empty target")

    head = parts[0]
    sample_index = _parse_sample_index(head)
    if sample_index is not None:
        samples = context.get("samples") or []
        if not samples:
            raise KeyError("no samples recorded")
        try:
            node: Any = samples[sample_index]
        except IndexError as exc:
            raise KeyError("sample index out of range") from exc
        parts = parts[1:]
    elif head == "list_objects":
        listed = context.get("list_objects")
        if listed is None:
            raise KeyError("no list_objects recorded")
        node = listed
        parts = parts[1:]
    else:
        observes = context.get("observes") or {}
        if head not in observes:
            raise KeyError("no observe recorded for %r" % head)
        node = observes[head]
        parts = parts[1:]

    for part in parts:
        if isinstance(node, dict):
            if part == "length" and _as_vec3(node) is not None:
                node = _vec_len(_as_vec3(node))
                continue
            if part not in node:
                raise KeyError("missing field %r" % part)
            node = node[part]
            continue
        if isinstance(node, list):
            if part == "length":
                node = float(len(node))
                continue
            if part == "count":
                node = float(len(node))
                continue
            if re.fullmatch(r"-?\d+", part):
                node = node[int(part)]
                continue
            raise KeyError("cannot index list with %r" % part)
        if part == "length" and _is_number(node):
            node = abs(float(node))
            continue
        raise KeyError("cannot resolve %r on %s" % (part, type(node).__name__))
    return node


def eval_assert(
    op: str,
    actual: Any,
    value: Any = None,
    tol: float | None = None,
    previous: Any = None,
) -> dict[str, Any]:
    """Evaluate one assertion op. Returns {pass, ...detail}."""
    detail: dict[str, Any] = {"op": op, "actual": actual, "value": value, "tol": tol}

    def fail(reason: str) -> dict[str, Any]:
        detail["pass"] = False
        detail["reason"] = reason
        return detail

    def ok(**extra: Any) -> dict[str, Any]:
        detail["pass"] = True
        detail.update(extra)
        return detail

    if op == "changed":
        if previous is None:
            return fail("no previous value for changed")
        return ok() if actual != previous else fail("value did not change")

    if op == "delta_gt":
        if previous is None:
            return fail("no previous value for delta_gt")
        if not _is_number(value):
            return fail("delta_gt requires a finite min-delta value")
        min_delta = float(value)
        actual_vec = _as_vec3(actual)
        prev_vec = _as_vec3(previous)
        if actual_vec is not None and prev_vec is not None:
            # Horizontal (XZ) displacement — ignores vertical settle/drift.
            dx = actual_vec[0] - prev_vec[0]
            dz = actual_vec[2] - prev_vec[2]
            delta = math.sqrt(dx * dx + dz * dz)
            detail["delta"] = delta
            detail["deltaY"] = actual_vec[1] - prev_vec[1]
            return ok() if delta > min_delta else fail("horizontal delta not greater")
        if not (_is_number(actual) and _is_number(previous)):
            return fail("delta_gt requires numbers or vec3")
        delta = abs(float(actual) - float(previous))
        detail["delta"] = delta
        return ok() if delta > min_delta else fail("delta not greater")

    if op == "eq":
        if isinstance(value, float) or isinstance(actual, float):
            if not (_is_number(actual) and _is_number(value)):
                return fail("eq requires numbers for float compare")
            return ok() if float(actual) == float(value) else fail("not equal")
        return ok() if actual == value else fail("not equal")

    if op == "approx":
        t = 1e-3 if tol is None else float(tol)
        compare = previous if value is None else value
        if value is None and previous is None:
            return fail("approx without value requires a previous sample")
        detail["value"] = compare
        actual_vec = _as_vec3(actual)
        if actual_vec is not None and (
                isinstance(compare, list) and len(compare) == 3 or _as_vec3(compare) is not None):
            value_vec = _as_vec3(compare) if not isinstance(compare, list) else [float(v) for v in compare]
            if value_vec is None:
                return fail("approx vector mismatch")
            deltas = [abs(actual_vec[i] - value_vec[i]) for i in range(3)]
            detail["deltas"] = deltas
            return ok() if all(d <= t for d in deltas) else fail("approx vector mismatch")
        if not (_is_number(actual) and _is_number(compare)):
            return fail("approx requires finite numbers or vec3")
        return ok() if abs(float(actual) - float(compare)) <= t else fail("approx mismatch")

    if op in ("gt", "lt"):
        if not (_is_number(actual) and _is_number(value)):
            # Allow gt/lt on vector lengths when actual is vec3 and value is scalar.
            actual_vec = _as_vec3(actual)
            if actual_vec is None or not _is_number(value):
                return fail("%s requires finite numbers" % op)
            actual_num = _vec_len(actual_vec)
            detail["actualLength"] = actual_num
        else:
            actual_num = float(actual)
        bound = float(value)
        if op == "gt":
            return ok() if actual_num > bound else fail("not greater")
        return ok() if actual_num < bound else fail("not less")

    if op == "between":
        if not (_is_number(actual) and isinstance(value, (list, tuple)) and len(value) == 2):
            return fail("between requires number and [lo,hi]")
        lo, hi = float(value[0]), float(value[1])
        num = float(actual)
        return ok() if lo <= num <= hi else fail("outside range")

    if op == "axis_dot":
        actual_vec = _as_vec3(actual)
        expected_vec = _as_vec3(value) if not isinstance(value, list) else (
            [float(v) for v in value] if len(value) == 3 else None)
        if expected_vec is None and isinstance(value, list) and len(value) == 3:
            try:
                expected_vec = [float(v) for v in value]
            except (TypeError, ValueError):
                expected_vec = None
        if actual_vec is None or expected_vec is None:
            return fail("axis_dot requires vec3 actual and value")
        na = _vec_norm(actual_vec)
        ne = _vec_norm(expected_vec)
        if na is None or ne is None:
            return fail("axis_dot requires non-zero vectors")
        min_dot = 0.98 if tol is None else float(tol)
        dot = _dot(na, ne)
        detail["dot"] = dot
        return ok() if dot >= min_dot else fail("axis_dot below tolerance")

    return fail("unknown op")
