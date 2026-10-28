"""RED tests for phase-4 jump-sample acceptance (no implementation yet)."""

from __future__ import annotations

import unittest

import runtime_bridge_phase4_acceptance as runner


def _sample(y: float, vy: float, x: float = 0.0, z: float = 0.0, vx: float = 0.0, vz: float = 0.0) -> dict:
    return {
        "position": {"x": x, "y": y, "z": z},
        "velocity": {"x": vx, "y": vy, "z": vz},
    }


def _valid_jump_sequence(baseline_y: float = 0.5) -> list[dict]:
    return [
        _sample(baseline_y, 0.0),
        _sample(0.9, 3.0),
        _sample(1.3, 0.0),
        _sample(0.9, -3.0),
        _sample(baseline_y, 0.0),
        _sample(baseline_y, 0.0),
        _sample(baseline_y, 0.0),
    ]


class ValidateJumpSamplesTests(unittest.TestCase):
    def test_valid_sequence_passes(self) -> None:
        baseline_y = 0.5
        result = runner.validate_jump_samples(_valid_jump_sequence(baseline_y), baseline_y)
        self.assertIsInstance(result, dict)
        self.assertTrue(result["pass"])

    def test_flat_samples_fail(self) -> None:
        baseline_y = 0.5
        samples = [_sample(baseline_y, 0.0) for _ in range(5)]
        result = runner.validate_jump_samples(samples, baseline_y)
        self.assertIsInstance(result, dict)
        self.assertFalse(result["pass"])

    def test_missing_descent_fails(self) -> None:
        baseline_y = 0.5
        samples = [
            _sample(baseline_y, 0.0),
            _sample(0.9, 3.0),
            _sample(1.3, 0.0),
            _sample(baseline_y, 0.0),
            _sample(baseline_y, 0.0),
            _sample(baseline_y, 0.0),
        ]
        result = runner.validate_jump_samples(samples, baseline_y)
        self.assertIsInstance(result, dict)
        self.assertFalse(result["pass"])

    def test_apex_vy0_above_floor_does_not_count_as_landing(self) -> None:
        baseline_y = 0.5
        samples = [
            _sample(baseline_y, 0.0),
            _sample(0.9, 3.0),
            _sample(1.3, 0.0),
            _sample(1.3, 0.0),
            _sample(1.3, 0.0),
        ]
        result = runner.validate_jump_samples(samples, baseline_y)
        self.assertIsInstance(result, dict)
        self.assertFalse(result["pass"])

    def test_missing_position_fails(self) -> None:
        baseline_y = 0.5
        samples = _valid_jump_sequence(baseline_y)
        samples[1] = {"velocity": {"x": 0.0, "y": 3.0, "z": 0.0}}
        result = runner.validate_jump_samples(samples, baseline_y)
        self.assertIsInstance(result, dict)
        self.assertFalse(result["pass"])

    def test_missing_velocity_fails(self) -> None:
        baseline_y = 0.5
        samples = _valid_jump_sequence(baseline_y)
        samples[1] = {"position": {"x": 0.0, "y": 0.9, "z": 0.0}}
        result = runner.validate_jump_samples(samples, baseline_y)
        self.assertIsInstance(result, dict)
        self.assertFalse(result["pass"])

    def test_nan_position_fails(self) -> None:
        baseline_y = 0.5
        samples = _valid_jump_sequence(baseline_y)
        samples[1] = _sample(float("nan"), 3.0)
        result = runner.validate_jump_samples(samples, baseline_y)
        self.assertIsInstance(result, dict)
        self.assertFalse(result["pass"])

    def test_nan_velocity_fails(self) -> None:
        baseline_y = 0.5
        samples = _valid_jump_sequence(baseline_y)
        samples[1] = _sample(0.9, float("nan"))
        result = runner.validate_jump_samples(samples, baseline_y)
        self.assertIsInstance(result, dict)
        self.assertFalse(result["pass"])

    def test_empty_samples_fail(self) -> None:
        result = runner.validate_jump_samples([], 0.5)
        self.assertIsInstance(result, dict)
        self.assertFalse(result["pass"])

    def test_stable_landing_requires_three_final_consecutive_near_baseline(self) -> None:
        baseline_y = 0.5
        samples = [
            _sample(baseline_y, 0.0),
            _sample(0.9, 3.0),
            _sample(1.3, 0.0),
            _sample(0.9, -3.0),
            _sample(baseline_y, 0.0),
            _sample(baseline_y, 0.0),
        ]
        result = runner.validate_jump_samples(samples, baseline_y)
        self.assertIsInstance(result, dict)
        self.assertFalse(result["pass"])

    def test_landing_outside_y_tolerance_fails(self) -> None:
        baseline_y = 0.5
        samples = [
            _sample(baseline_y, 0.0),
            _sample(0.9, 3.0),
            _sample(1.3, 0.0),
            _sample(0.9, -3.0),
            _sample(baseline_y + 0.06, 0.0),
            _sample(baseline_y + 0.06, 0.0),
            _sample(baseline_y + 0.06, 0.0),
        ]
        result = runner.validate_jump_samples(samples, baseline_y)
        self.assertIsInstance(result, dict)
        self.assertFalse(result["pass"])

    def test_landing_outside_vy_tolerance_fails(self) -> None:
        baseline_y = 0.5
        samples = [
            _sample(baseline_y, 0.0),
            _sample(0.9, 3.0),
            _sample(1.3, 0.0),
            _sample(0.9, -3.0),
            _sample(baseline_y, 0.11),
            _sample(baseline_y, 0.11),
            _sample(baseline_y, 0.11),
        ]
        result = runner.validate_jump_samples(samples, baseline_y)
        self.assertIsInstance(result, dict)
        self.assertFalse(result["pass"])

    def test_min_rise_below_threshold_fails(self) -> None:
        baseline_y = 0.5
        samples = [
            _sample(baseline_y, 0.0),
            _sample(baseline_y + 0.2, 1.0),
            _sample(baseline_y + 0.2, 0.0),
            _sample(baseline_y + 0.1, -1.0),
            _sample(baseline_y, 0.0),
            _sample(baseline_y, 0.0),
            _sample(baseline_y, 0.0),
        ]
        result = runner.validate_jump_samples(samples, baseline_y)
        self.assertIsInstance(result, dict)
        self.assertFalse(result["pass"])

    def test_missing_positive_vy_before_landing_fails(self) -> None:
        baseline_y = 0.5
        samples = [
            _sample(baseline_y, 0.0),
            _sample(0.9, 0.0),
            _sample(1.3, 0.0),
            _sample(0.9, -3.0),
            _sample(baseline_y, 0.0),
            _sample(baseline_y, 0.0),
            _sample(baseline_y, 0.0),
        ]
        result = runner.validate_jump_samples(samples, baseline_y)
        self.assertIsInstance(result, dict)
        self.assertFalse(result["pass"])

    def test_missing_negative_vy_before_landing_fails(self) -> None:
        baseline_y = 0.5
        samples = [
            _sample(baseline_y, 0.0),
            _sample(0.9, 3.0),
            _sample(1.3, 0.0),
            _sample(0.9, 0.0),
            _sample(baseline_y, 0.0),
            _sample(baseline_y, 0.0),
            _sample(baseline_y, 0.0),
        ]
        result = runner.validate_jump_samples(samples, baseline_y)
        self.assertIsInstance(result, dict)
        self.assertFalse(result["pass"])


if __name__ == "__main__":
    unittest.main()
