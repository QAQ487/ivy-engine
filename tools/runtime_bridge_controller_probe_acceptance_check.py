#!/usr/bin/env python3
"""Pure unit tests for controller probe acceptance helpers (no Windows runtime)."""
import unittest
from types import SimpleNamespace

import runtime_bridge_controller_probe_acceptance as runner
from runtime_bridge_controller_probe import (
    RESPONSE_BYTE_LIMIT,
    find_trigger_window,
    frames_after_sequence,
    max_controller_probe_response_bytes,
)


def _frame(lt=0.0, rt=0.0, sequence=1, pressed=None, pressed_edge=None, released_edge=None):
    return {
        "sequence": sequence,
        "lx": 0.0, "ly": 0.0, "rx": 0.0, "ry": 0.0, "lt": lt, "rt": rt,
        "pressed": list(pressed or []),
        "pressedEdge": list(pressed_edge or []),
        "releasedEdge": list(released_edge or []),
        "connected": True,
    }


class ProbeAcceptanceCheck(unittest.TestCase):
    def test_validate_runner_args(self):
        good = SimpleNamespace(
            exe="x", out_dir="y", fixture_scene="z.json",
            settle=0.5, ready_timeout=10.0, shutdown_timeout=10.0, completion_timeout=5.0)
        self.assertIsNone(runner.validate_runner_args(good))
        bad = SimpleNamespace(**{**good.__dict__, "exe": ""})
        self.assertEqual(runner.validate_runner_args(bad), "exe path required")
        bad = SimpleNamespace(**{**good.__dict__, "settle": 0.0})
        self.assertIn("settle", runner.validate_runner_args(bad))

    def test_response_bound_documented(self):
        self.assertGreaterEqual(RESPONSE_BYTE_LIMIT, max_controller_probe_response_bytes())
        self.assertGreater(RESPONSE_BYTE_LIMIT, 8192)

    def test_sequence_window_not_trailing_slice(self):
        # Regression: do not assert frames[-4:] after an arbitrary sleep.
        frames = [
            _frame(sequence=1),
            _frame(sequence=2),
            _frame(lt=1.0, sequence=3),
            _frame(lt=1.0, sequence=4),
            _frame(sequence=5),
            _frame(sequence=6),
            _frame(sequence=7),
        ]
        window = frames_after_sequence(frames, 1)
        found, err = find_trigger_window(window, True, 1.0, 2)
        self.assertIsNone(err)
        self.assertEqual([f["sequence"] for f in found], [2, 3, 4, 5])
        # Trailing idle would make [-4:] wrong if the window grew.
        wrong = window[-4:]
        self.assertNotEqual([f["sequence"] for f in wrong], [2, 3, 4, 5])


if __name__ == "__main__":
    unittest.main()
