#!/usr/bin/env python3
"""Unit tests for controller consumer-probe validators (no Windows runtime)."""
import unittest

from runtime_bridge_controller_probe import (
    BUTTONS,
    validate_button_hold_frames,
    validate_combined_frames,
    validate_eventual_neutral_suffix,
    validate_probe_reply,
    validate_right_stick_keeps_left_neutral,
    validate_trigger_hold_frames,
)


def _frame(lt=0.0, rt=0.0, lx=0.0, ly=0.0, rx=0.0, ry=0.0,
           pressed=None, pressed_edge=None, released_edge=None, sequence=1):
    return {
        "sequence": sequence,
        "lx": lx, "ly": ly, "rx": rx, "ry": ry, "lt": lt, "rt": rt,
        "pressed": list(pressed or []),
        "pressedEdge": list(pressed_edge or []),
        "releasedEdge": list(released_edge or []),
        "connected": True,
    }


def _trigger_trace(left, value, updates):
    frames = [_frame(sequence=1)]
    for i in range(updates):
        frames.append(_frame(
            lt=value if left else 0.0,
            rt=0.0 if left else value,
            sequence=i + 2,
        ))
    frames.append(_frame(sequence=updates + 2))
    return frames


def _button_trace(button, updates):
    frames = [_frame(sequence=1)]
    for i in range(updates):
        frames.append(_frame(
            pressed=[button],
            pressed_edge=[button] if i == 0 else [],
            sequence=i + 2,
        ))
    frames.append(_frame(released_edge=[button], sequence=updates + 2))
    return frames


class ControllerProbeCheck(unittest.TestCase):
    def test_trigger_acceptance(self):
        for value in (0.0, 0.25, 0.5, 1.0):
            ok, err = validate_trigger_hold_frames(_trigger_trace(True, value, 2), True, value, 2)
            self.assertTrue(ok, err)
            ok, err = validate_trigger_hold_frames(_trigger_trace(False, value, 3), False, value, 3)
            self.assertTrue(ok, err)

    def test_trigger_rejects_interference(self):
        bad = _trigger_trace(True, 0.25, 2)
        bad[1]["lx"] = 0.1
        self.assertFalse(validate_trigger_hold_frames(bad, True, 0.25, 2)[0])
        bad = _trigger_trace(True, 0.25, 2)
        bad[2]["rt"] = 1.0
        self.assertFalse(validate_trigger_hold_frames(bad, True, 0.25, 2)[0])

    def test_every_button(self):
        self.assertEqual(len(BUTTONS), 14)
        for button in BUTTONS:
            ok, err = validate_button_hold_frames(_button_trace(button, 2), button, 2)
            self.assertTrue(ok, "%s: %s" % (button, err))
            bad = _button_trace(button, 2)
            bad[1]["pressedEdge"] = []
            self.assertFalse(validate_button_hold_frames(bad, button, 2)[0])
            bad = _button_trace(button, 2)
            bad[3]["releasedEdge"] = []
            self.assertFalse(validate_button_hold_frames(bad, button, 2)[0])
            bad = _button_trace(button, 2)
            bad[1]["ly"] = 1.0
            self.assertFalse(validate_button_hold_frames(bad, button, 2)[0])

    def test_combined(self):
        buttons = ["A", "LB", "DPAD_UP"]
        frames = [
            _frame(sequence=1),
            _frame(lt=0.25, rt=1.0, pressed=buttons, pressed_edge=buttons, sequence=2),
            _frame(lt=0.25, rt=1.0, pressed=buttons, sequence=3),
            _frame(released_edge=buttons, sequence=4),
        ]
        ok, err = validate_combined_frames(frames, 0.25, 1.0, buttons, 2)
        self.assertTrue(ok, err)
        frames[1]["lx"] = 0.2
        self.assertFalse(validate_combined_frames(frames, 0.25, 1.0, buttons, 2)[0])

    def test_right_stick_left_neutral(self):
        frames = [
            _frame(sequence=1),
            _frame(rx=0.5, ry=-1.0, sequence=2),
            _frame(rx=0.5, ry=-1.0, sequence=3),
            _frame(sequence=4),
        ]
        ok, err = validate_right_stick_keeps_left_neutral(frames, 0.5, -1.0, 2)
        self.assertTrue(ok, err)
        frames[2]["lx"] = 0.01
        self.assertFalse(validate_right_stick_keeps_left_neutral(frames, 0.5, -1.0, 2)[0])

    def test_cancel_neutral_suffix(self):
        frames = [
            _frame(lt=1.0, pressed=["B"], pressed_edge=["B"], sequence=1),
            _frame(released_edge=["B"], sequence=2),
            _frame(sequence=3),
            _frame(sequence=4),
            _frame(sequence=5),
        ]
        ok, err = validate_eventual_neutral_suffix(frames, 3)
        self.assertTrue(ok, err)
        frames[4]["rt"] = 1.0
        self.assertFalse(validate_eventual_neutral_suffix(frames, 3)[0])

    def test_probe_reply_schema(self):
        reply = {
            "ok": True,
            "protocolVersion": 1,
            "frames": [_frame()],
            "latestSequence": 1,
            "dropped": 0,
            "overflow": False,
        }
        ok, err = validate_probe_reply(reply)
        self.assertTrue(ok, err)
        reply["overflow"] = True
        self.assertFalse(validate_probe_reply(reply)[0])
        reply["dropped"] = 1
        ok, err = validate_probe_reply(reply)
        self.assertTrue(ok, err)


if __name__ == "__main__":
    unittest.main()
