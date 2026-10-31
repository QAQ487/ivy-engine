"""Pure controller-verb validation for the runtime test bridge.

Left-stick payloads stay {lx, ly, updates}. rx, ry, lt, rt, and buttons are
optional. This module has no Windows imports so the acceptance checks can run
on Linux. Keep the accepted names and ranges aligned with
engine/testing/controller_inject_protocol.h.
"""
import math

BUTTONS = (
    "A", "B", "X", "Y", "LB", "RB", "START", "BACK",
    "LEFT_THUMB", "RIGHT_THUMB", "DPAD_UP", "DPAD_DOWN", "DPAD_LEFT", "DPAD_RIGHT",
)
_BUTTON_SET = frozenset(BUTTONS)
_ENVELOPE = frozenset({"protocolVersion", "requestId", "runId", "verb"})
_FIELDS = frozenset({"lx", "ly", "rx", "ry", "lt", "rt", "buttons", "updates"})
_AXES = ("lx", "ly", "rx", "ry", "lt", "rt")
_OPEN_QUEUE = frozenset({"idle", "completed", "cancelled"})


def _stick(value, name):
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
        raise ValueError("%s must be a finite number in [-1,1]" % name)
    if not -1.0 <= float(value) <= 1.0:
        raise ValueError("%s must be a finite number in [-1,1]" % name)
    return float(value)


def _trigger(value, name):
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
        raise ValueError("%s must be a finite number in [0,1]" % name)
    if not 0.0 <= float(value) <= 1.0:
        raise ValueError("%s must be a finite number in [0,1]" % name)
    return float(value)


def parse_controller_object(obj):
    """Validate a controller request object. Missing optional axes default to neutral."""
    if not isinstance(obj, dict):
        raise ValueError("controller payload must be an object")
    unknown = set(obj) - _ENVELOPE - _FIELDS
    if unknown:
        raise ValueError("unknown controller field")
    if "lx" not in obj or "ly" not in obj or "updates" not in obj:
        raise ValueError("controller requires lx, ly, and updates")
    updates = obj["updates"]
    if isinstance(updates, bool) or type(updates) is not int or not 1 <= updates <= 120:
        raise ValueError("updates must be an integer in [1,120]")
    sample = {
        "lx": _stick(obj["lx"], "lx"),
        "ly": _stick(obj["ly"], "ly"),
        "rx": 0.0,
        "ry": 0.0,
        "lt": 0.0,
        "rt": 0.0,
        "buttons": [],
        "updates": updates,
    }
    if "rx" in obj:
        sample["rx"] = _stick(obj["rx"], "rx")
    if "ry" in obj:
        sample["ry"] = _stick(obj["ry"], "ry")
    if "lt" in obj:
        sample["lt"] = _trigger(obj["lt"], "lt")
    if "rt" in obj:
        sample["rt"] = _trigger(obj["rt"], "rt")
    if "buttons" in obj:
        buttons = obj["buttons"]
        if not isinstance(buttons, list) or len(buttons) > 14:
            raise ValueError("buttons must be an array of at most 14 names")
        seen = []
        for name in buttons:
            if not isinstance(name, str) or name not in _BUTTON_SET or name in seen:
                raise ValueError("buttons must be unique known controller button names")
            seen.append(name)
        sample["buttons"] = seen
    return sample


def build_controller_payload(lx, ly, updates, rx=None, ry=None, lt=None, rt=None, buttons=None):
    """Build a backward-compatible controller payload. Omitted optionals are left off the wire."""
    obj = {"lx": lx, "ly": ly, "updates": updates}
    if rx is not None:
        obj["rx"] = rx
    if ry is not None:
        obj["ry"] = ry
    if lt is not None:
        obj["lt"] = lt
    if rt is not None:
        obj["rt"] = rt
    if buttons is not None:
        obj["buttons"] = list(buttons)
    parsed = parse_controller_object(obj)
    payload = {"lx": parsed["lx"], "ly": parsed["ly"], "updates": parsed["updates"]}
    if rx is not None:
        payload["rx"] = parsed["rx"]
    if ry is not None:
        payload["ry"] = parsed["ry"]
    if lt is not None:
        payload["lt"] = parsed["lt"]
    if rt is not None:
        payload["rt"] = parsed["rt"]
    if buttons is not None:
        payload["buttons"] = list(parsed["buttons"])
    return payload


def queue_accepts(status):
    """One controller operation at a time. Preparing is included even though the wire says queued."""
    return status in _OPEN_QUEUE


def _finite(value):
    return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)


def _neutral_axes(frame):
    return all(_finite(frame.get(key)) and frame.get(key) == 0 for key in _AXES)


def validate_injected_sequence(frames, command):
    """Check one prepare + N holds + one neutral release trace.

    Active frames must match the command, including a left stick that stays at
    the commanded lx/ly. Press edges happen only on the first hold. Release
    edges happen only on the final neutral frame.
    """
    if not isinstance(frames, list) or not frames or not isinstance(command, dict):
        return {"pass": False, "error": "trace is empty", "activeFrames": 0}
    updates = command.get("updates")
    buttons = command.get("buttons", [])
    if type(updates) is not int or not 1 <= updates <= 120 or not isinstance(buttons, list):
        return {"pass": False, "error": "command is invalid", "activeFrames": 0}
    if len(frames) != updates + 2:
        return {"pass": False, "error": "trace length", "activeFrames": 0}
    button_set = set(buttons)
    active = 0
    for index, frame in enumerate(frames):
        if not isinstance(frame, dict):
            return {"pass": False, "error": "frame is not an object", "activeFrames": active}
        for key in _AXES:
            if not _finite(frame.get(key)):
                return {"pass": False, "error": "nonfinite axis", "activeFrames": active}
        held = frame.get("buttons")
        pressed = frame.get("pressed_edges")
        released = frame.get("released_edges")
        if not isinstance(held, list) or not isinstance(pressed, list) or not isinstance(released, list):
            return {"pass": False, "error": "button lists", "activeFrames": active}
        if frame.get("isolating") is not True:
            return {"pass": False, "error": "frame is not isolating", "activeFrames": active}
        phase = frame.get("phase")
        if index == 0:
            if phase != "preparing" or not _neutral_axes(frame) or held or pressed or released:
                return {"pass": False, "error": "preparation is not neutral", "activeFrames": active}
        elif index == len(frames) - 1:
            if phase != "releasing" or not _neutral_axes(frame) or held or pressed or set(released) != button_set:
                return {"pass": False, "error": "release is not neutral", "activeFrames": active}
        else:
            if phase != "active":
                return {"pass": False, "error": "hold phase", "activeFrames": active}
            for key in _AXES:
                if frame.get(key) != command.get(key):
                    if key in ("lx", "ly"):
                        return {"pass": False, "error": "left stick changed", "activeFrames": active}
                    return {"pass": False, "error": "%s mismatch" % key, "activeFrames": active}
            if set(held) != button_set:
                return {"pass": False, "error": "held buttons", "activeFrames": active}
            if index == 1:
                if set(pressed) != button_set or released:
                    return {"pass": False, "error": "first press edge", "activeFrames": active}
            elif pressed or released:
                return {"pass": False, "error": "unexpected edge", "activeFrames": active}
            active += 1
    return {"pass": True, "error": "", "activeFrames": active}
