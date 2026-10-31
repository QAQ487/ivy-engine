"""Pure validators for controller consumer-probe frames.

Frames are observations of public Controller_* getters after Controller_Update.
Exact float equality is intentional for the Debug inject path, which returns the
applied float without XInput BYTE deadzone remapping. Physical-pad quantization
is therefore out of scope for these acceptance checks.
"""
from __future__ import annotations

import json

# Matches engine/testing/controller_probe_trace.h kControllerProbeCapacity.
PROBE_CAPACITY = 256
# IVY_RUNTIME_TEST_RUN_ID is read into a 65-byte buffer (64 chars + NUL).
MAX_RUN_ID_CHARS = 64

BUTTONS = (
    "A", "B", "X", "Y", "LB", "RB", "START", "BACK",
    "LEFT_THUMB", "RIGHT_THUMB", "DPAD_UP", "DPAD_DOWN", "DPAD_LEFT", "DPAD_RIGHT",
)
_BUTTON_BITS = {
    "DPAD_UP": 0x0001,
    "DPAD_DOWN": 0x0002,
    "DPAD_LEFT": 0x0004,
    "DPAD_RIGHT": 0x0008,
    "START": 0x0010,
    "BACK": 0x0020,
    "LEFT_THUMB": 0x0040,
    "RIGHT_THUMB": 0x0080,
    "LB": 0x0100,
    "RB": 0x0200,
    "A": 0x1000,
    "B": 0x2000,
    "X": 0x4000,
    "Y": 0x8000,
}
_EXACT_TRIGGERS = (0.0, 0.25, 0.5, 1.0)


def button_mask(names):
    mask = 0
    for name in names:
        if name not in _BUTTON_BITS:
            raise ValueError("unknown button %s" % name)
        mask |= _BUTTON_BITS[name]
    return mask


def _mask_from_names(names):
    if not isinstance(names, list) or any(not isinstance(n, str) for n in names):
        raise ValueError("button name list invalid")
    return button_mask(names)


def validate_probe_reply(reply):
    if not isinstance(reply, dict) or reply.get("ok") is not True:
        return False, "probe reply not ok"
    if reply.get("protocolVersion") != 1:
        return False, "bad protocolVersion"
    if "frames" not in reply or not isinstance(reply["frames"], list):
        return False, "frames missing"
    if type(reply.get("latestSequence")) is not int or reply["latestSequence"] < 0:
        return False, "latestSequence invalid"
    if type(reply.get("dropped")) is not int or reply["dropped"] < 0:
        return False, "dropped invalid"
    if not isinstance(reply.get("overflow"), bool):
        return False, "overflow invalid"
    if reply["overflow"] and reply["dropped"] < 1:
        return False, "overflow without dropped"
    for frame in reply["frames"]:
        ok, err = validate_probe_frame(frame)
        if not ok:
            return False, err
    return True, ""


def validate_probe_frame(frame):
    if not isinstance(frame, dict):
        return False, "frame not object"
    required = ("sequence", "lx", "ly", "rx", "ry", "lt", "rt", "pressed", "pressedEdge", "releasedEdge", "connected")
    for key in required:
        if key not in frame:
            return False, "frame missing %s" % key
    if type(frame["sequence"]) is not int or frame["sequence"] < 1:
        return False, "sequence invalid"
    for axis in ("lx", "ly", "rx", "ry", "lt", "rt"):
        value = frame[axis]
        if isinstance(value, bool) or not isinstance(value, (int, float)):
            return False, "%s not numeric" % axis
    if not isinstance(frame["connected"], bool):
        return False, "connected not bool"
    for key in ("pressed", "pressedEdge", "releasedEdge"):
        names = frame[key]
        if not isinstance(names, list) or any(n not in _BUTTON_BITS for n in names):
            return False, "%s button names invalid" % key
        if len(names) != len(set(names)):
            return False, "%s has duplicates" % key
    return True, ""


def _eq(a, b):
    return float(a) == float(b)


def validate_trigger_hold_frames(frames, left, hold_value, updates):
    if updates < 1:
        return False, "updates < 1"
    if hold_value not in _EXACT_TRIGGERS:
        return False, "hold value not an exact inject sample"
    if len(frames) != updates + 2:
        return False, "trigger frame count"
    for i, frame in enumerate(frames):
        if not (_eq(frame["lx"], 0.0) and _eq(frame["ly"], 0.0)):
            return False, "left stick moved"
        if not (_eq(frame["rx"], 0.0) and _eq(frame["ry"], 0.0)):
            return False, "right stick moved"
        if frame["pressed"] or frame["pressedEdge"] or frame["releasedEdge"]:
            return False, "buttons changed"
        expected = 0.0 if i in (0, len(frames) - 1) else hold_value
        actual = frame["lt"] if left else frame["rt"]
        other = frame["rt"] if left else frame["lt"]
        if not _eq(actual, expected):
            return False, "trigger value mismatch"
        if not _eq(other, 0.0):
            return False, "sibling trigger not neutral"
        if frame["connected"] is not True:
            return False, "connected false"
    return True, ""


def validate_button_hold_frames(frames, button, updates):
    if button not in _BUTTON_BITS:
        return False, "unknown button"
    if updates < 1:
        return False, "updates < 1"
    if len(frames) != updates + 2:
        return False, "button frame count"
    for i, frame in enumerate(frames):
        for axis in ("lx", "ly", "rx", "ry", "lt", "rt"):
            if not _eq(frame[axis], 0.0):
                return False, "axes moved during button-only"
        if frame["connected"] is not True:
            return False, "connected false"
        pressed = set(frame["pressed"])
        edge = set(frame["pressedEdge"])
        released = set(frame["releasedEdge"])
        if i == 0:
            if pressed or edge or released:
                return False, "prep not neutral"
        elif i + 1 == len(frames):
            if pressed or edge or released != {button}:
                return False, "release edge mismatch"
        elif i == 1:
            if pressed != {button} or edge != {button} or released:
                return False, "first-hold press edge mismatch"
        else:
            if pressed != {button} or edge or released:
                return False, "held state mismatch"
    return True, ""


def validate_combined_frames(frames, lt, rt, buttons, updates):
    if updates < 1:
        return False, "updates < 1"
    if len(frames) != updates + 2:
        return False, "combined frame count"
    expected = set(buttons)
    for i, frame in enumerate(frames):
        if not (_eq(frame["lx"], 0.0) and _eq(frame["ly"], 0.0)):
            return False, "left stick moved"
        if not (_eq(frame["rx"], 0.0) and _eq(frame["ry"], 0.0)):
            return False, "right stick moved"
        if frame["connected"] is not True:
            return False, "connected false"
        if i in (0, len(frames) - 1):
            if not (_eq(frame["lt"], 0.0) and _eq(frame["rt"], 0.0)):
                return False, "prep/release triggers not neutral"
        else:
            if not (_eq(frame["lt"], lt) and _eq(frame["rt"], rt)):
                return False, "combined hold trigger mismatch"
        pressed = set(frame["pressed"])
        edge = set(frame["pressedEdge"])
        released = set(frame["releasedEdge"])
        if i == 0:
            if pressed or edge or released:
                return False, "combined prep not neutral"
        elif i + 1 == len(frames):
            if pressed or edge or released != expected:
                return False, "combined release mismatch"
        elif i == 1:
            if pressed != expected or edge != expected or released:
                return False, "combined first-hold mismatch"
        else:
            if pressed != expected or edge or released:
                return False, "combined held mismatch"
    return True, ""


def validate_right_stick_keeps_left_neutral(frames, rx, ry, updates):
    if updates < 1:
        return False, "updates < 1"
    if len(frames) != updates + 2:
        return False, "right-stick frame count"
    for i, frame in enumerate(frames):
        if not (_eq(frame["lx"], 0.0) and _eq(frame["ly"], 0.0)):
            return False, "left stick moved"
        if frame["pressed"] or frame["pressedEdge"] or frame["releasedEdge"]:
            return False, "buttons changed"
        if not (_eq(frame["lt"], 0.0) and _eq(frame["rt"], 0.0)):
            return False, "triggers changed"
        expect_rx = 0.0 if i in (0, len(frames) - 1) else rx
        expect_ry = 0.0 if i in (0, len(frames) - 1) else ry
        if not (_eq(frame["rx"], expect_rx) and _eq(frame["ry"], expect_ry)):
            return False, "right stick mismatch"
    return True, ""


def validate_eventual_neutral_suffix(frames, min_neutral_tail=3):
    if len(frames) < min_neutral_tail:
        return False, "not enough frames"
    for frame in frames[-min_neutral_tail:]:
        for axis in ("lx", "ly", "rx", "ry", "lt", "rt"):
            if not _eq(frame[axis], 0.0):
                return False, "stuck axis after cancel"
        if frame["pressed"] or frame["pressedEdge"] or frame["releasedEdge"]:
            return False, "stuck button after cancel"
    return True, ""


def frames_from_json_list(frames):
    """Normalize JSON probe frames into validator dicts (button lists already present)."""
    out = []
    for frame in frames:
        ok, err = validate_probe_frame(frame)
        if not ok:
            raise ValueError(err)
        out.append(frame)
    return out


def max_controller_probe_response_bytes():
    """Worst-case UTF-8 size of a full-capacity controller_probe JSON reply.

    Mirrors nlohmann dump(-1) (compact, no spaces) used by the bridge worker.
    Floats use a fat decimal form so the bound covers Grisu2-style expansions.
    """
    buttons = list(BUTTONS)
    frame = {
        "sequence": 9223372036854775807,
        "lx": -0.10000000149011612,
        "ly": -0.10000000149011612,
        "rx": -0.10000000149011612,
        "ry": -0.10000000149011612,
        "lt": 0.10000000149011612,
        "rt": 0.10000000149011612,
        "pressed": buttons,
        "pressedEdge": buttons,
        "releasedEdge": buttons,
        "connected": True,
    }
    frames_json = ",".join(
        json.dumps(frame, separators=(",", ":"), ensure_ascii=True)
        for _ in range(PROBE_CAPACITY)
    )
    envelope = (
        '{"protocolVersion":1,"requestId":2147483647,"runId":"%s","ok":true,'
        '"frames":[%s],"latestSequence":9223372036854775807,'
        '"dropped":4294967295,"overflow":true}'
    ) % ("a" * MAX_RUN_ID_CHARS, frames_json)
    return len(envelope.encode("utf-8"))


# Client pipe response limit: full 256-frame probe + headroom, still bounded.
_RESPONSE_HEADROOM = 16384
RESPONSE_BYTE_LIMIT = max_controller_probe_response_bytes() + _RESPONSE_HEADROOM


def accumulate_pipe_response(chunks, max_bytes=RESPONSE_BYTE_LIMIT):
    """Assemble a newline-terminated JSON response from byte chunks.

    Rejects bodies larger than max_bytes (oversize). Used by the Windows client
    and by pure unit tests that feed synthetic chunk streams.
    """
    if type(max_bytes) is not int or max_bytes < 1:
        raise ValueError("max_bytes must be a positive int")
    data = bytearray()
    for chunk in chunks:
        if not isinstance(chunk, (bytes, bytearray)) or not chunk:
            raise ConnectionError("short pipe transfer")
        nl = chunk.find(b"\n")
        if nl >= 0:
            data += chunk[:nl]
            if len(data) > max_bytes:
                raise ValueError("oversize response")
            return json.loads(data.decode("utf-8"))
        data += chunk
        if len(data) > max_bytes:
            raise ValueError("oversize response")
    raise ConnectionError("response missing newline")


def frames_after_sequence(frames, sequence_before):
    """Return probe frames with sequence strictly greater than sequence_before."""
    if type(sequence_before) is not int or sequence_before < 0:
        raise ValueError("sequence_before must be a non-negative int")
    out = []
    for frame in frames:
        ok, err = validate_probe_frame(frame)
        if not ok:
            raise ValueError(err)
        if frame["sequence"] > sequence_before:
            out.append(frame)
    return out


def find_validated_window(frames, need, validator):
    """Find the first contiguous window of `need` frames that passes validator."""
    if need < 1:
        return None, "need < 1"
    if len(frames) < need:
        return None, "not enough frames in sequence window"
    for i in range(0, len(frames) - need + 1):
        chunk = frames[i:i + need]
        ok, err = validator(chunk)
        if ok:
            return chunk, None
    return None, "no matching window in sequence range"


def find_trigger_window(frames, left, hold_value, updates):
    need = updates + 2

    def _validate(chunk):
        return validate_trigger_hold_frames(chunk, left, hold_value, updates)

    return find_validated_window(frames, need, _validate)


def find_button_window(frames, button, updates):
    need = updates + 2

    def _validate(chunk):
        return validate_button_hold_frames(chunk, button, updates)

    return find_validated_window(frames, need, _validate)


def find_combined_window(frames, lt, rt, buttons, updates):
    need = updates + 2

    def _validate(chunk):
        return validate_combined_frames(chunk, lt, rt, buttons, updates)

    return find_validated_window(frames, need, _validate)


def find_right_stick_window(frames, rx, ry, updates):
    need = updates + 2

    def _validate(chunk):
        return validate_right_stick_keeps_left_neutral(chunk, rx, ry, updates)

    return find_validated_window(frames, need, _validate)
