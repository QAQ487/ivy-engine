"""Unit tests and pure validation helpers check for mouse runtime acceptance."""
import hashlib
import json
import os
import pathlib
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib

HERE = pathlib.Path(__file__).resolve().parent
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

import runtime_bridge_mouse_acceptance as runner



def make_dummy_png(width=1920, height=1080, payload_bytes=5000):
    sig = b"\x89PNG\r\n\x1a\n"
    # IHDR: 13 bytes
    ihdr_data = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    ihdr_crc = struct.pack(">I", zlib.crc32(b"IHDR" + ihdr_data) & 0xFFFFFFFF)
    ihdr = struct.pack(">I", len(ihdr_data)) + b"IHDR" + ihdr_data + ihdr_crc
    # IDAT
    raw_idat = b"x" * max(1, payload_bytes - 100)
    compressed = zlib.compress(raw_idat, level=0)
    idat_crc = struct.pack(">I", zlib.crc32(b"IDAT" + compressed) & 0xFFFFFFFF)
    idat = struct.pack(">I", len(compressed)) + b"IDAT" + compressed + idat_crc
    # IEND
    iend_crc = struct.pack(">I", zlib.crc32(b"IEND") & 0xFFFFFFFF)
    iend = struct.pack(">I", 0) + b"IEND" + iend_crc
    data = sig + ihdr + idat + iend
    if len(data) <= 4096:
        # pad IDAT to exceed 4096
        padding = b"\x00" * (4100 - len(data))
        raw_idat = raw_idat + padding
        compressed = zlib.compress(raw_idat, level=0)
        idat_crc = struct.pack(">I", zlib.crc32(b"IDAT" + compressed) & 0xFFFFFFFF)
        idat = struct.pack(">I", len(compressed)) + b"IDAT" + compressed + idat_crc
        data = sig + ihdr + idat + iend
    return data


class MouseAcceptancePureValidationTests(unittest.TestCase):
    def test_malformed_png(self):
        valid = make_dummy_png(1920, 1080, 5000)
        self.assertTrue(runner.validate_png_bytes(valid)["pass"])

        # Truncated
        r = runner.validate_png_bytes(valid[:16])
        self.assertFalse(r["pass"])

        # Invalid magic
        bad_magic = b"NOTPNG\r\n" + valid[8:]
        r = runner.validate_png_bytes(bad_magic)
        self.assertFalse(r["pass"])

        # Small byte length <= 4096
        short_png = b"\x89PNG\r\n\x1a\n" + valid[8:100]
        r = runner.validate_png_bytes(short_png)
        self.assertFalse(r["pass"])

        # Dimension <= 100
        tiny_dim_png = make_dummy_png(50, 50, 5000)
        r = runner.validate_png_bytes(tiny_dim_png)
        self.assertFalse(r["pass"])

        # Expected dimensions mismatch
        r = runner.validate_png_bytes(valid, expected_width=1280, expected_height=720)
        self.assertFalse(r["pass"])

        # Non-bytes input
        r = runner.validate_png_bytes("string")
        self.assertFalse(r["pass"])

    def test_missing_observations_and_empty_samples(self):
        initial_pos = {"x": 0.0, "y": 0.5, "z": 0.0}
        # Empty samples must fail (no vacuous pass)
        r = runner.validate_samples([], initial_pos)
        self.assertFalse(r["pass"])

        # None or non-list
        r = runner.validate_samples(None, initial_pos)
        self.assertFalse(r["pass"])

        # Missing position
        r = runner.validate_samples([{"position": None, "velocity": {"x": 0, "y": 0, "z": 0}}], initial_pos)
        self.assertFalse(r["pass"])

    def test_changed_or_nonfinite_pos_and_velocity(self):
        initial_pos = {"x": 0.0, "y": 0.5, "z": 0.0}

        # NaN position
        samples_nan = [{"position": {"x": float("nan"), "y": 0.5, "z": 0.0}, "velocity": {"x": 0, "y": 0, "z": 0}}]
        self.assertFalse(runner.validate_samples(samples_nan, initial_pos)["pass"])

        # Inf position
        samples_inf = [{"position": {"x": 0.0, "y": float("inf"), "z": 0.0}, "velocity": {"x": 0, "y": 0, "z": 0}}]
        self.assertFalse(runner.validate_samples(samples_inf, initial_pos)["pass"])

        # Drift beyond tolerance
        samples_drift = [{"position": {"x": 0.2, "y": 0.5, "z": 0.0}, "velocity": {"x": 0, "y": 0, "z": 0}}]
        self.assertFalse(runner.validate_samples(samples_drift, initial_pos, pos_tolerance=0.05)["pass"])

        # Non-zero XZ velocity
        samples_vel_x = [{"position": {"x": 0.0, "y": 0.5, "z": 0.0}, "velocity": {"x": 0.1, "y": 0, "z": 0}}]
        self.assertFalse(runner.validate_samples(samples_vel_x, initial_pos)["pass"])

        samples_vel_z = [{"position": {"x": 0.0, "y": 0.5, "z": 0.0}, "velocity": {"x": 0, "y": 0, "z": -0.5}}]
        self.assertFalse(runner.validate_samples(samples_vel_z, initial_pos)["pass"])

    def test_timeout_and_disconnect_not_accepted_as_rejection(self):
        # Exceptions / timeouts must NOT be accepted as rejection success
        self.assertFalse(runner.validate_rejection_response(None))
        self.assertFalse(runner.validate_rejection_response(TimeoutError("deadline")))
        self.assertFalse(runner.validate_rejection_response(ConnectionResetError("closed")))
        self.assertFalse(runner.validate_rejection_response({"ok": True, "status": "queued"}))
        self.assertFalse(runner.validate_rejection_response("invalid"))

        # Explicit ok: false is required
        self.assertTrue(runner.validate_rejection_response({"ok": False, "error": "invalid_request"}))

    def test_crash_exit_and_shutdown_validation(self):
        # Timeout (None) fails
        self.assertFalse(runner.validate_shutdown(None))
        # Nonzero crashes fail
        self.assertFalse(runner.validate_shutdown(3221225477))  # 0xC0000005
        self.assertFalse(runner.validate_shutdown(-1073741819))
        self.assertFalse(runner.validate_shutdown(1))
        # Native exit 0 passes
        self.assertTrue(runner.validate_shutdown(0))

    def test_frame_hashes_changes(self):
        sha1 = "a" * 64
        sha2 = "b" * 64
        sha3 = "c" * 64
        # Hash changes prove frame difference
        self.assertTrue(runner.validate_frame_hashes(sha1, sha2, sha3))
        # Same before and forward fails
        self.assertFalse(runner.validate_frame_hashes(sha1, sha1, sha3))
        # Same forward and reverse fails
        self.assertFalse(runner.validate_frame_hashes(sha1, sha2, sha2))
        # Empty hashes fail
        self.assertFalse(runner.validate_frame_hashes("", sha2, sha3))

    def test_valid_evidence(self):
        initial_pos = {"x": 0.0, "y": 0.499262, "z": 0.0}
        samples = [
            {"position": {"x": 0.0, "y": 0.499262, "z": 0.0}, "velocity": {"x": 0.0, "y": 0.0, "z": 0.0}},
            {"position": {"x": 0.0001, "y": 0.499262, "z": -0.0001}, "velocity": {"x": 0.0, "y": 0.0, "z": 0.0}},
        ]
        r = runner.validate_samples(samples, initial_pos, pos_tolerance=0.01)
        self.assertTrue(r["pass"])
        self.assertEqual(r["count"], 2)

        png_data = make_dummy_png(1920, 1080, 6000)
        png_res = runner.validate_png_bytes(png_data, 1920, 1080)
        self.assertTrue(png_res["pass"])
        self.assertEqual(png_res["width"], 1920)
        self.assertEqual(png_res["height"], 1080)

        self.assertTrue(runner.validate_rejection_response({"ok": False, "error": "invalid_request"}))
        self.assertTrue(runner.validate_shutdown(0))

    def test_import_runner_safety(self):
        # Runner module must be importable without launching processes or touching filesystem
        script = "import runtime_bridge_mouse_acceptance; print('IMPORT_SAFE')"
        res = subprocess.run([sys.executable, "-c", script], capture_output=True, text=True, cwd=str(HERE))
        self.assertEqual(res.returncode, 0)
        self.assertIn("IMPORT_SAFE", res.stdout)

    def test_cli_no_gui_invalid_paths(self):
        runner_py = str(HERE / "runtime_bridge_mouse_acceptance.py")
        with tempfile.TemporaryDirectory() as td:
            temp_dir = pathlib.Path(td)
            nonexistent_exe = temp_dir / "nonexistent.exe"
            target_out = temp_dir / "out"

            # 1. Nonexistent exe must fail nonzero and not create out-dir
            res = subprocess.run(
                [sys.executable, runner_py, "--exe", str(nonexistent_exe), "--out-dir", str(target_out)],
                capture_output=True,
                text=True,
            )
            self.assertNotEqual(res.returncode, 0)
            self.assertFalse(target_out.exists())

            # 2. Existing out-dir must fail nonzero
            dummy_exe = temp_dir / "dummy.exe"
            dummy_exe.write_bytes(b"MZ\x00\x00")
            existing_out = temp_dir / "existing_out"
            existing_out.mkdir()
            res = subprocess.run(
                [sys.executable, runner_py, "--exe", str(dummy_exe), "--out-dir", str(existing_out)],
                capture_output=True,
                text=True,
            )
            self.assertNotEqual(res.returncode, 0)

    def test_collect_fixture_rom_paths_from_player_scene(self):
        scene = json.loads(runner.DEFAULT_FIXTURE.read_text(encoding="utf-8"))
        paths = runner.collect_fixture_rom_paths(scene)
        self.assertIn("rom/model/player/Y Bot_white.fbx", paths)
        self.assertIn("rom/model/player/Idle.fbx", paths)
        self.assertIn("rom/model/player/Running.fbx", paths)
        self.assertIn("rom/model/player/Jump01.fbx", paths)
        self.assertIn("rom/model/object/Box_Brown.fbx", paths)
        self.assertIn("rom/model/object/Box_Blue.fbx", paths)
        self.assertIn("rom/skybox/MySkybox.dds", paths)
        self.assertIn("rom/font/LemonSlice.ttf", paths)

    def test_stage_fixture_copies_player_models_and_fails_on_missing(self):
        with tempfile.TemporaryDirectory() as td:
            temp_dir = pathlib.Path(td)
            out_dir = temp_dir / "out"
            out_dir.mkdir()
            exe_dir = temp_dir / "Debug"
            exe_dir.mkdir()
            exe = exe_dir / "IvyEngineRuntime.exe"
            exe.write_bytes(b"MZ\x00\x00")
            # Simulate OutDir that already has resources/ but not testbed player FBX.
            (exe_dir / "resources" / "skybox").mkdir(parents=True)
            (exe_dir / "resources" / "skybox" / "MySkybox.dds").write_bytes(b"DDS")
            (exe_dir / "resources" / "font").mkdir(parents=True)
            (exe_dir / "resources" / "font" / "LemonSlice.ttf").write_bytes(b"TTF")

            staged_exe, package = runner.stage_fixture_package(exe, out_dir, runner.DEFAULT_FIXTURE)
            self.assertEqual(staged_exe, package / exe.name)
            for rel in (
                "rom/model/player/Y Bot_white.fbx",
                "rom/model/player/Idle.fbx",
                "rom/model/player/Running.fbx",
                "rom/model/player/Jump01.fbx",
                "rom/model/object/Box_Brown.fbx",
                "rom/model/object/Box_Blue.fbx",
            ):
                self.assertTrue((package / rel).is_file(), "expected staged %s" % rel)
                self.assertTrue(runner.fixture_asset_readable(package, rel))

            # Skybox/texture may stay on the resources/ fallback without a rom/ copy.
            self.assertTrue(runner.fixture_asset_readable(package, "rom/skybox/MySkybox.dds"))
            self.assertTrue(runner.fixture_asset_readable(package, "rom/font/LemonSlice.ttf"))

            broken = {
                "nodes": [{"components": {"visual": {"model": "rom/model/player/Missing.fbx"}}}],
                "settings": {"render": {"skyboxCubemap": "rom/skybox/NoSuch.dds"}},
            }
            broken_scene = temp_dir / "broken.json"
            broken_scene.write_text(json.dumps(broken), encoding="utf-8")
            empty_pkg = temp_dir / "empty_pkg"
            empty_pkg.mkdir()
            with self.assertRaises(SystemExit) as raised:
                runner.stage_fixture_scene_assets(empty_pkg, broken_scene, repo_root=temp_dir)
            message = str(raised.exception)
            self.assertIn("fixture scene references missing assets", message)
            self.assertIn("rom/model/player/Missing.fbx", message)
            self.assertIn("rom/skybox/NoSuch.dds", message)


if __name__ == "__main__":
    unittest.main()
