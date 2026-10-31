"""Pure acceptance checks for right-stick, button, and trigger injection.

These tests do not launch a Windows runtime. They lock the payload rules and the
prepare / hold / release trace that a runtime session must satisfy.
"""
import pathlib
import sys
import unittest

HERE = pathlib.Path(__file__).resolve().parent
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

import runtime_bridge_controller_axes as axes


def _command(**overrides):
    command = {
        "lx": 0.0,
        "ly": 0.0,
        "rx": 0.5,
        "ry": -1.0,
        "lt": 0.25,
        "rt": 1.0,
        "buttons": ["A", "START"],
        "updates": 2,
    }
    command.update(overrides)
    return command


def _neutral(phase):
    return {
        "phase": phase,
        "isolating": True,
        "lx": 0.0,
        "ly": 0.0,
        "rx": 0.0,
        "ry": 0.0,
        "lt": 0.0,
        "rt": 0.0,
        "buttons": [],
        "pressed_edges": [],
        "released_edges": [],
    }


def _hold(command, first):
    buttons = list(command["buttons"])
    return {
        "phase": "active",
        "isolating": True,
        "lx": command["lx"],
        "ly": command["ly"],
        "rx": command["rx"],
        "ry": command["ry"],
        "lt": command["lt"],
        "rt": command["rt"],
        "buttons": buttons,
        "pressed_edges": buttons if first else [],
        "released_edges": [],
    }


def _good_trace(command):
    frames = [_neutral("preparing")]
    for index in range(command["updates"]):
        frames.append(_hold(command, index == 0))
    release = _neutral("releasing")
    release["released_edges"] = list(command["buttons"])
    frames.append(release)
    return frames


class ControllerPayloadTests(unittest.TestCase):
    def test_legacy_payload_omits_new_fields_and_defaults_them(self):
        payload = axes.build_controller_payload(0, 1, 3)
        self.assertEqual(payload, {"lx": 0.0, "ly": 1.0, "updates": 3})
        parsed = axes.parse_controller_object(payload)
        self.assertEqual(parsed["rx"], 0.0)
        self.assertEqual(parsed["ry"], 0.0)
        self.assertEqual(parsed["lt"], 0.0)
        self.assertEqual(parsed["rt"], 0.0)
        self.assertEqual(parsed["buttons"], [])

    def test_optional_fields_round_trip(self):
        payload = axes.build_controller_payload(
            0, 0, 2, rx=0.5, ry=-1, lt=0.25, rt=1, buttons=["A", "START", "LB", "DPAD_DOWN"]
        )
        parsed = axes.parse_controller_object(payload)
        self.assertEqual(parsed["lx"], 0.0)
        self.assertEqual(parsed["ly"], 0.0)
        self.assertEqual(parsed["rx"], 0.5)
        self.assertEqual(parsed["ry"], -1.0)
        self.assertEqual(parsed["lt"], 0.25)
        self.assertEqual(parsed["rt"], 1.0)
        self.assertEqual(parsed["buttons"], ["A", "START", "LB", "DPAD_DOWN"])

    def test_known_button_names_cover_the_controller_api(self):
        names = [
            "A", "B", "X", "Y", "LB", "RB", "START", "BACK",
            "LEFT_THUMB", "RIGHT_THUMB", "DPAD_UP", "DPAD_DOWN", "DPAD_LEFT", "DPAD_RIGHT",
        ]
        parsed = axes.parse_controller_object({"lx": 0, "ly": 0, "updates": 1, "buttons": names})
        self.assertEqual(parsed["buttons"], names)

    def test_rejects_out_of_range_nonfinite_bool_and_unknown_fields(self):
        bad = [
            {"lx": 1.1, "ly": 0, "updates": 1},
            {"lx": 0, "ly": -1.01, "updates": 1},
            {"lx": True, "ly": 0, "updates": 1},
            {"lx": 0, "ly": "1", "updates": 1},
            {"lx": float("nan"), "ly": 0, "updates": 1},
            {"lx": 0, "ly": float("inf"), "updates": 1},
            {"lx": 0, "ly": 0, "updates": 0},
            {"lx": 0, "ly": 0, "updates": 121},
            {"lx": 0, "ly": 0, "updates": 1.5},
            {"lx": 0, "ly": 0, "updates": True},
            {"lx": 0, "ly": 0, "updates": 1, "extra": 1},
            {"ly": 0, "updates": 1},
            {"lx": 0, "ly": 0, "rx": 1.01, "updates": 1},
            {"lx": 0, "ly": 0, "ry": True, "updates": 1},
            {"lx": 0, "ly": 0, "lt": -0.01, "updates": 1},
            {"lx": 0, "ly": 0, "rt": 1.01, "updates": 1},
            {"lx": 0, "ly": 0, "lt": False, "updates": 1},
            {"lx": 0, "ly": 0, "rt": float("nan"), "updates": 1},
            {"lx": 0, "ly": 0, "updates": 1, "buttons": "A"},
            {"lx": 0, "ly": 0, "updates": 1, "buttons": ["LT"]},
            {"lx": 0, "ly": 0, "updates": 1, "buttons": ["a"]},
            {"lx": 0, "ly": 0, "updates": 1, "buttons": ["A", "A"]},
            {"lx": 0, "ly": 0, "updates": 1, "buttons": ["NOPE"]},
            [],
        ]
        for obj in bad:
            with self.subTest(obj=obj):
                with self.assertRaises(ValueError):
                    axes.parse_controller_object(obj)

    def test_builder_rejects_before_a_payload_exists(self):
        with self.assertRaises(ValueError):
            axes.build_controller_payload(0, 0, 1, buttons=["A", "A"])
        with self.assertRaises(ValueError):
            axes.build_controller_payload(float("nan"), 0, 1)


class ControllerSequenceTests(unittest.TestCase):
    def test_valid_combined_trace_passes_and_keeps_left_stick(self):
        command = _command()
        result = axes.validate_injected_sequence(_good_trace(command), command)
        self.assertTrue(result["pass"], result)
        self.assertEqual(result["activeFrames"], 2)

    def test_empty_and_short_traces_fail(self):
        command = _command()
        self.assertFalse(axes.validate_injected_sequence([], command)["pass"])
        self.assertFalse(axes.validate_injected_sequence(None, command)["pass"])
        self.assertFalse(axes.validate_injected_sequence(_good_trace(command)[:-1], command)["pass"])

    def test_release_that_keeps_right_stick_or_buttons_fails(self):
        command = _command()
        frames = _good_trace(command)
        frames[-1]["rx"] = 0.5
        self.assertFalse(axes.validate_injected_sequence(frames, command)["pass"])
        frames = _good_trace(command)
        frames[-1]["buttons"] = ["A"]
        self.assertFalse(axes.validate_injected_sequence(frames, command)["pass"])
        frames = _good_trace(command)
        frames[-1]["released_edges"] = []
        self.assertFalse(axes.validate_injected_sequence(frames, command)["pass"])

    def test_left_stick_drift_fails(self):
        command = _command(lx=0.0, ly=0.0)
        frames = _good_trace(command)
        frames[1]["lx"] = 0.2
        result = axes.validate_injected_sequence(frames, command)
        self.assertFalse(result["pass"])
        self.assertIn("left", result["error"])

    def test_repeated_press_edge_fails(self):
        command = _command()
        frames = _good_trace(command)
        frames[2]["pressed_edges"] = ["A"]
        self.assertFalse(axes.validate_injected_sequence(frames, command)["pass"])

    def test_missing_first_press_edge_fails(self):
        command = _command()
        frames = _good_trace(command)
        frames[1]["pressed_edges"] = []
        self.assertFalse(axes.validate_injected_sequence(frames, command)["pass"])

    def test_trigger_out_of_hold_fails(self):
        command = _command()
        frames = _good_trace(command)
        frames[1]["rt"] = 0.0
        self.assertFalse(axes.validate_injected_sequence(frames, command)["pass"])
        frames = _good_trace(command)
        frames[0]["lt"] = 0.25
        self.assertFalse(axes.validate_injected_sequence(frames, command)["pass"])

    def test_nonfinite_sample_fails(self):
        command = _command()
        frames = _good_trace(command)
        frames[1]["ry"] = float("nan")
        self.assertFalse(axes.validate_injected_sequence(frames, command)["pass"])

    def test_queue_accepts_only_when_idle_completed_or_cancelled(self):
        for status in ("queued", "preparing", "active", "releasing"):
            self.assertFalse(axes.queue_accepts(status), status)
        for status in ("idle", "completed", "cancelled"):
            self.assertTrue(axes.queue_accepts(status), status)
        self.assertFalse(axes.queue_accepts("weird"))

    def test_buttonless_release_has_no_edges(self):
        command = _command(buttons=[], rx=1.0, ry=0.0, lt=0.0, rt=0.0, updates=1)
        result = axes.validate_injected_sequence(_good_trace(command), command)
        self.assertTrue(result["pass"], result)


if __name__ == "__main__":
    unittest.main()
