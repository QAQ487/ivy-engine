"""Unit tests for angular acceptance math/validators (no live runtime)."""
from __future__ import annotations

import json
import math
import pathlib
import unittest

import runtime_bridge_angular_math as runner

HERE = pathlib.Path(__file__).resolve().parent
REPO = HERE.parent
FIXTURE = REPO / "testbed" / "rom" / "scene" / "scene_runtime_bridge_angular.json"


class AngularMathTests(unittest.TestCase):
    def test_inv_inertia_box_matches_engine_formula(self):
        # FreeSpinBox: halfExtents 0.4 -> size 0.8, scale 0.8 => fullSize 0.64.
        m, s = 1.0, 0.64
        i = (m / 12.0) * (s * s + s * s)
        inv = 1.0 / i
        got = runner.apply_inv_inertia_local({"x": inv, "y": inv, "z": inv}, (0.0, 0.0, 2.0))
        self.assertAlmostEqual(got[2], inv * 2.0, places=5)
        self.assertEqual(got[0], 0.0)
        self.assertEqual(got[1], 0.0)

    def test_angular_drag_factor_matches_integrate(self):
        dt = runner.PHYSICS_DT
        factor = runner.angular_drag_factor(0.05, dt)
        self.assertAlmostEqual(factor, 1.0 / (1.0 + 0.05 * dt), places=12)
        w0 = (0.0, 0.0, 10.0)
        w30 = runner.expected_ang_vel_after_steps(w0, 0.05, 30, dt)
        self.assertAlmostEqual(w30[2], 10.0 * (factor ** 30), places=10)

    def test_quat_integration_preserves_norm_and_advances(self):
        q0 = (0.0, 0.0, 0.0, 1.0)
        omega = (0.0, 0.0, 1.0)
        q = q0
        for _ in range(60):
            q = runner.integrate_quat_step(q, omega, runner.PHYSICS_DT)
        self.assertAlmostEqual(runner.quat_len(q), 1.0, places=6)
        # ~1 rad over 1 second at 60 Hz with the half-omega*q step.
        angle = runner.quat_angle_deg(q0, q)
        self.assertGreater(angle, 40.0)
        self.assertLess(angle, 70.0)

    def test_rolling_slip_no_slip_is_zero(self):
        # vx = -wz * R
        self.assertAlmostEqual(runner.rolling_slip(2.0, -4.0, 0.5), 0.0, places=12)
        self.assertGreater(runner.rolling_slip(2.0, 0.0, 0.5), 1.0)

    def test_body_up_axis_identity(self):
        up = runner.body_up_axis_from_quat((0.0, 0.0, 0.0, 1.0))
        self.assertAlmostEqual(up[0], 0.0, places=6)
        self.assertAlmostEqual(up[1], 1.0, places=6)
        self.assertAlmostEqual(up[2], 0.0, places=6)


class AngularValidatorTests(unittest.TestCase):
    def test_validate_free_spin_pass(self):
        inv = 5.0
        torque = (0.0, 0.0, 2.0)
        w0 = (0.0, 0.0, inv * 2.0)
        steps = 10
        drag = 0.05
        wn = runner.expected_ang_vel_after_steps(w0, drag, steps)
        after = {
            "angularVelocity": {"x": w0[0], "y": w0[1], "z": w0[2]},
            "rigidbody": {
                "angularDrag": drag,
                "localInverseInertia": {"x": inv, "y": inv, "z": inv},
            },
        }
        later = {"angularVelocity": {"x": wn[0], "y": wn[1], "z": wn[2]}}
        result = runner.validate_free_spin(after, torque, steps, later)
        self.assertTrue(result["pass"], result)

    def test_validate_free_spin_fails_wrong_axis(self):
        after = {
            "angularVelocity": {"x": 2.0, "y": 0.0, "z": 0.0},
            "rigidbody": {
                "angularDrag": 0.0,
                "localInverseInertia": {"x": 1.0, "y": 1.0, "z": 1.0},
            },
        }
        later = {"angularVelocity": {"x": 2.0, "y": 0.0, "z": 0.0}}
        result = runner.validate_free_spin(after, (0.0, 0.0, 2.0), 5, later)
        self.assertFalse(result["pass"])

    def test_validate_off_center_impulse(self):
        before = {
            "position": {"x": 0.0, "y": 5.0, "z": 0.0},
            "velocity": {"x": 0.0, "y": 0.0, "z": 0.0},
            "angularVelocity": {"x": 0.0, "y": 0.0, "z": 0.0},
        }
        impulse = (4.0, 0.0, 0.0)
        point = (0.0, 5.3, 0.0)
        mass = 1.0
        inv = {"x": 10.0, "y": 10.0, "z": 10.0}
        r = (0.0, 0.3, 0.0)
        tau = runner.v_cross(r, impulse)
        dw = runner.apply_inv_inertia_local(inv, tau)
        after = {
            "velocity": {"x": 4.0, "y": 0.0, "z": 0.0},
            "angularVelocity": {"x": dw[0], "y": dw[1], "z": dw[2]},
        }
        result = runner.validate_off_center_impulse(
            before, after, impulse, point, mass, inv)
        self.assertTrue(result["pass"], result)
        self.assertAlmostEqual(result["tau"][2], -1.2, places=6)

    def test_validate_rotation_integration(self):
        q0 = (0.0, 0.0, 0.0, 1.0)
        omegas = [(0.0, 0.0, 0.5)] * 20
        qn = runner.integrate_quat_n(q0, omegas)
        result = runner.validate_rotation_integration(q0, omegas, qn)
        self.assertTrue(result["pass"], result)
        bad = runner.integrate_quat_n(q0, [(0.0, 0.0, 2.0)] * 20)
        result_bad = runner.validate_rotation_integration(q0, omegas, bad)
        self.assertFalse(result_bad["pass"])

    def test_validate_rolling_slip_shrinks(self):
        samples = [
            {"vx": 3.0, "wz": 0.0},
            {"vx": 2.5, "wz": -2.0},
            {"vx": 2.2, "wz": -3.5},
            {"vx": 2.0, "wz": -4.0},
        ]
        result = runner.validate_rolling(samples, radius=0.5)
        self.assertTrue(result["pass"], result)
        bad = [
            {"vx": 3.0, "wz": 0.0},
            {"vx": 3.0, "wz": 1.0},
            {"vx": 3.0, "wz": 2.0},
        ]
        self.assertFalse(runner.validate_rolling(bad, radius=0.5)["pass"])

    def test_validate_rolling_terry_windows_samples(self):
        """Real angular-runtime samples from Terry @ e55467d (sphere_rolling).

        Early frames already show RH Y-up sign (vx>0 ⇒ wz<0). Slip shrinks toward
        wz = -vx/R (R=0.5 ⇒ wz = -2*vx). Mid/late frames decelerate to rest — the
        axis check must use an early developing sample, not a post-stop mid index.
        """
        samples = [
            {"vx": 1.528, "wz": -1.008},
            {"vx": 0.793, "wz": -1.009},
            {"vx": 0.355, "wz": -0.497},
            {"vx": 0.148, "wz": -0.295},
            {"vx": 0.080, "wz": -0.160},  # near no-slip: wz ≈ -2*vx
            {"vx": 0.040, "wz": -0.080},
            {"vx": 0.020, "wz": -0.040},
            {"vx": 0.010, "wz": -0.020},
            {"vx": 0.005, "wz": -0.010},
            {"vx": 0.002, "wz": -0.004},
            {"vx": 0.000, "wz": 0.000},
            {"vx": 0.000, "wz": 0.000},
        ]
        self.assertEqual(runner.expected_rolling_wz_sign(1.528), -1)
        self.assertLess(
            runner.rolling_slip(samples[4]["vx"], samples[4]["wz"], 0.5), 0.01)
        result = runner.validate_rolling(samples, radius=0.5)
        self.assertTrue(result["pass"], result)
        self.assertTrue(result["axisOk"], result)
        self.assertTrue(result["slipOk"], result)
        self.assertEqual(result["expectedWzSign"], -1)
        self.assertEqual(result["convention"], "RH_Yup_wz_eq_minus_vx_over_R")
        # Axis sample must be early (developing), not a late near-rest frame.
        self.assertGreater(abs(result["axisSample"]["wz"]), 0.2)

    def test_rolling_convention_matches_cross3(self):
        # r=(0,-R,0), ω=(0,0,wz) ⇒ ω×r = (wz*R, 0, 0); no-slip vx = -wz*R.
        r = (0.0, -0.5, 0.0)
        omega = (0.0, 0.0, -3.0)  # negative wz
        omega_cross_r = runner.v_cross(omega, r)
        self.assertAlmostEqual(omega_cross_r[0], -3.0 * 0.5, places=9)  # -1.5
        # v + ω×r = 0 ⇒ vx = +1.5 when wz = -3 (same sign relation as Terry samples).
        self.assertAlmostEqual(runner.rolling_slip(1.5, -3.0, 0.5), 0.0, places=12)

    def test_validate_tipping_upright(self):
        obs = {"rotationQuaternion": {"x": 0.0, "y": 0.0, "z": 0.0, "w": 1.0}}
        self.assertTrue(runner.validate_tipping(obs)["pass"])
        s = math.sin(math.pi / 4.0)
        c = math.cos(math.pi / 4.0)
        tipped = {"rotationQuaternion": {"x": 0.0, "y": 0.0, "z": s, "w": c}}
        self.assertFalse(runner.validate_tipping(tipped)["pass"])

    def test_validate_reset_determinism(self):
        a = {
            "velocity": {"x": 1.0, "y": 0.0, "z": 0.0},
            "angularVelocity": {"x": 0.0, "y": 0.0, "z": 2.5},
        }
        b = {
            "velocity": {"x": 1.0, "y": 0.0, "z": 0.0},
            "angularVelocity": {"x": 0.0, "y": 0.0, "z": 2.5},
        }
        self.assertTrue(runner.validate_reset_determinism(a, b)["pass"])
        b2 = {
            "velocity": {"x": 1.0, "y": 0.0, "z": 0.0},
            "angularVelocity": {"x": 0.0, "y": 0.0, "z": 2.6},
        }
        self.assertFalse(runner.validate_reset_determinism(a, b2)["pass"])


class AngularFixtureTests(unittest.TestCase):
    def test_fixture_objects_exist(self):
        self.assertTrue(FIXTURE.is_file())
        data = json.loads(FIXTURE.read_text(encoding="utf-8"))
        names = {n.get("name") for n in data.get("nodes", [])}
        for required in ("Ground", "FreeSpinBox", "RollingSphere", "TippingBox"):
            self.assertIn(required, names)

    def test_ground_has_kinematic_friction(self):
        data = json.loads(FIXTURE.read_text(encoding="utf-8"))
        ground = next(n for n in data["nodes"] if n["name"] == "Ground")
        rb = ground["components"]["Rigidbody"]
        self.assertTrue(rb["isKinematic"])
        self.assertGreaterEqual(rb["friction"], 0.5)

    def test_rolling_sphere_is_sphere_collider(self):
        data = json.loads(FIXTURE.read_text(encoding="utf-8"))
        sphere = next(n for n in data["nodes"] if n["name"] == "RollingSphere")
        self.assertEqual(sphere["components"]["Collider"]["colliderType"], 2)
        self.assertGreaterEqual(sphere["components"]["Rigidbody"]["friction"], 0.5)
        self.assertTrue(sphere["components"]["Rigidbody"]["useGravity"])

    def test_free_spin_airborne_no_gravity(self):
        data = json.loads(FIXTURE.read_text(encoding="utf-8"))
        box = next(n for n in data["nodes"] if n["name"] == "FreeSpinBox")
        self.assertFalse(box["components"]["Rigidbody"]["useGravity"])
        self.assertGreaterEqual(box["components"]["SceneTransform"]["posY"], 2.0)

    def test_known_limitation_box_friction_skip(self):
        skips = [s for s in runner.KNOWN_LIMITATION_SCENARIOS if s["id"] == "box_friction_rolling"]
        self.assertEqual(len(skips), 1)
        self.assertEqual(skips[0]["status"], "expected-skip")
        self.assertIn("friction torque", skips[0]["reason"].lower())

    def test_fixture_rom_assets_resolvable_in_repo(self):
        """Scene-referenced rom/ paths must exist under testbed/rom or resources/."""
        data = json.loads(FIXTURE.read_text(encoding="utf-8"))
        found = set()

        def walk(node):
            if isinstance(node, dict):
                for value in node.values():
                    walk(value)
            elif isinstance(node, list):
                for value in node:
                    walk(value)
            elif isinstance(node, str):
                normalized = node.replace("\\", "/")
                if normalized.startswith("rom/") and ".." not in normalized.split("/"):
                    found.add(normalized)

        walk(data)
        self.assertTrue(found)
        for rom_path in sorted(found):
            candidates = [
                REPO / "testbed" / rom_path,
                REPO / rom_path,
                REPO / "resources" / rom_path[4:],
            ]
            self.assertTrue(
                any(c.is_file() for c in candidates),
                msg="unresolved fixture asset %s" % rom_path,
            )


if __name__ == "__main__":
    unittest.main()
