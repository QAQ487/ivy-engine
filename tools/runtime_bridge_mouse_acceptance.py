"""Import-safe stdlib mouse runtime acceptance runner for the Ivy runtime test bridge.

Asserts bounded mouse injection lifecycle, physical/player isolation, view digest differences,
explicit protocol rejections, and clean shutdown against an isolated fixture package.
"""
import argparse
import ctypes as C
from ctypes import wintypes as W
import hashlib
import json
import math
import os
import pathlib
import shutil
import struct
import subprocess
import sys
import time
import traceback
import uuid

HERE = pathlib.Path(__file__).resolve().parent
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

from runtime_test_client import Client

ENV_CLEAR = (
    "IVY_FRAME_CAPTURE",
    "IVY_DISABLE_RENDER_THREAD",
    "IVY_PLAY_SCENE_SWITCH_CHECK",
    "IVY_SCENE_HOT_LOAD_CHECK",
    "IVY_RUNTIME_TEST_BRIDGE",
    "IVY_RUNTIME_TEST_RUN_ID",
    "IVY_RUNTIME_TEST_OUTPUT_DIR",
    "IVY_RUNTIME_TEST_FIXED_CAMERA",
    "IVY_SKIP_CLOSE_CONFIRM",
)

REPO_ROOT = HERE.parent
DEFAULT_FIXTURE = REPO_ROOT / "testbed" / "rom" / "scene" / "scene_runtime_bridge_player.json"
EXPECTED_SCENE = "scene_runtime_bridge_player.json"
SCENE_ARG = "rom/scene/scene_runtime_bridge_player.json"

_K = None


def session_of(pid):
    """Lazy Windows kernel32 session lookup (import-safe)."""
    global _K
    if _K is None:
        _K = C.WinDLL("kernel32", use_last_error=True)
        _K.ProcessIdToSessionId.argtypes = [W.DWORD, C.POINTER(W.DWORD)]
    val = W.DWORD()
    if not _K.ProcessIdToSessionId(pid, C.byref(val)):
        raise C.WinError(C.get_last_error())
    return val.value


def vec(d):
    if isinstance(d, dict):
        return (d.get("x", 0.0), d.get("y", 0.0), d.get("z", 0.0))
    return (d[0], d[1], d[2])


def validate_png_bytes(data, expected_width=None, expected_height=None):
    if not isinstance(data, (bytes, bytearray)):
        return {"pass": False, "error": "data not bytes", "bytes": 0}
    nbytes = len(data)
    if nbytes < 24:
        return {"pass": False, "error": "file too small for PNG header", "bytes": nbytes}
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        return {"pass": False, "error": "invalid PNG magic signature", "bytes": nbytes}
    if not (data[8:12] == struct.pack(">I", 13) and data[12:16] == b"IHDR"):
        return {"pass": False, "error": "invalid PNG IHDR chunk header", "bytes": nbytes}
    width, height = struct.unpack(">II", data[16:24])
    sha = hashlib.sha256(data).hexdigest()
    if nbytes <= 4096:
        return {"pass": False, "error": "PNG size <= 4096 bytes", "bytes": nbytes, "width": width, "height": height, "sha256": sha}
    if width <= 100 or height <= 100:
        return {"pass": False, "error": "PNG dimensions too small (<= 100)", "bytes": nbytes, "width": width, "height": height, "sha256": sha}
    if expected_width is not None and width != expected_width:
        return {"pass": False, "error": f"width mismatch: {width} != {expected_width}", "bytes": nbytes, "width": width, "height": height, "sha256": sha}
    if expected_height is not None and height != expected_height:
        return {"pass": False, "error": f"height mismatch: {height} != {expected_height}", "bytes": nbytes, "width": width, "height": height, "sha256": sha}
    return {"pass": True, "error": None, "bytes": nbytes, "width": width, "height": height, "sha256": sha}


def validate_samples(samples, initial_pos, pos_tolerance=0.05):
    if not samples or not isinstance(samples, (list, tuple)):
        return {"pass": False, "error": "samples empty or missing", "count": 0, "maxDrift": 0.0}
    max_drift = 0.0
    if not isinstance(initial_pos, dict):
        return {"pass": False, "error": "initial_pos not a dict", "count": len(samples), "maxDrift": max_drift}
    for axis in ("x", "y", "z"):
        val = initial_pos.get(axis)
        if val is None or not isinstance(val, (int, float)) or isinstance(val, bool) or not math.isfinite(val):
            return {"pass": False, "error": f"initial_pos non-finite {axis}: {val}", "count": len(samples), "maxDrift": max_drift}
    init_v = vec(initial_pos)
    for idx, s in enumerate(samples):
        pos = s.get("position")
        vel = s.get("velocity")
        if pos is None or not isinstance(pos, dict):
            return {"pass": False, "error": f"sample {idx} missing position", "count": len(samples), "maxDrift": max_drift}
        pos_v = (pos.get("x"), pos.get("y"), pos.get("z"))
        for axis_idx, (axis, val) in enumerate(zip(("x", "y", "z"), pos_v)):
            if val is None or not isinstance(val, (int, float)) or not math.isfinite(val):
                return {"pass": False, "error": f"sample {idx} non-finite position {axis}: {val}", "count": len(samples), "maxDrift": max_drift}
            drift = abs(val - init_v[axis_idx])
            if drift > max_drift:
                max_drift = drift
            if drift > pos_tolerance:
                return {"pass": False, "error": f"sample {idx} position drift on {axis} ({drift:.6f} > {pos_tolerance})", "count": len(samples), "maxDrift": max_drift}
        if vel is None or not isinstance(vel, dict):
            return {"pass": False, "error": f"sample {idx} missing velocity", "count": len(samples), "maxDrift": max_drift}
        if "x" not in vel or "z" not in vel:
            return {"pass": False, "error": f"sample {idx} velocity missing x or z field", "count": len(samples), "maxDrift": max_drift}
        vx = vel["x"]
        vz = vel["z"]
        if not (isinstance(vx, (int, float)) and not isinstance(vx, bool) and math.isfinite(vx) and
                isinstance(vz, (int, float)) and not isinstance(vz, bool) and math.isfinite(vz)):
            return {"pass": False, "error": f"sample {idx} non-finite velocity", "count": len(samples), "maxDrift": max_drift}
        if abs(vx) > 1e-4 or abs(vz) > 1e-4:
            return {"pass": False, "error": f"sample {idx} non-zero XZ velocity: vx={vx}, vz={vz}", "count": len(samples), "maxDrift": max_drift}
    return {"pass": True, "error": None, "count": len(samples), "maxDrift": max_drift}


def validate_rejection_response(response):
    if not isinstance(response, dict):
        return False
    return response.get("ok") is False


def validate_shutdown(exit_code):
    return exit_code == 0


def validate_frame_hashes(sha_before, sha_forward, sha_reverse):
    if not (sha_before and sha_forward and sha_reverse):
        return False
    return (sha_before != sha_forward) and (sha_forward != sha_reverse)


def collect_fixture_rom_paths(scene_data):
    """Collect unique rom/... asset path strings referenced by a scene JSON tree."""
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

    walk(scene_data)
    return sorted(found)


def fixture_asset_readable(package, rom_path):
    """True when package resolves rom/... via rom/ or resources/ (content_path fallback)."""
    normalized = rom_path.replace("\\", "/")
    if (package / pathlib.Path(normalized)).is_file():
        return True
    if normalized.startswith("rom/"):
        return (package / "resources" / normalized[4:]).is_file()
    return False


def resolve_repo_asset(rom_path, repo_root=None):
    """Locate a rom/... asset under testbed/rom, rom/, or resources/ in the repo."""
    root = pathlib.Path(repo_root) if repo_root is not None else REPO_ROOT
    normalized = rom_path.replace("\\", "/")
    candidates = [root / "testbed" / normalized, root / normalized]
    if normalized.startswith("rom/"):
        candidates.append(root / "resources" / normalized[4:])
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    return None


def stage_fixture_scene_assets(package, fixture_src, repo_root=None):
    """Copy scene-referenced assets into the fixture; fail fast listing any still missing."""
    scene_data = json.loads(pathlib.Path(fixture_src).read_text(encoding="utf-8"))
    missing = []
    for rom_path in collect_fixture_rom_paths(scene_data):
        if fixture_asset_readable(package, rom_path):
            continue
        src = resolve_repo_asset(rom_path, repo_root=repo_root)
        if src is None:
            missing.append(rom_path)
            continue
        dest = package / pathlib.Path(rom_path)
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dest)
    if missing:
        raise SystemExit(
            "fixture scene references missing assets:\n  %s" % "\n  ".join(missing)
        )


def stage_fixture_package(exe, out_dir, fixture_scene):
    """Copy the Debug runtime tree and pin the player bridge fixture via default_scene + --scene."""
    exe = pathlib.Path(exe).resolve()
    fixture_src = pathlib.Path(fixture_scene).resolve()
    if not fixture_src.is_file():
        raise SystemExit("fixture scene not found: %s" % fixture_src)
    package = out_dir / "fixture"
    if package.exists():
        raise SystemExit("fixture package already exists: %s" % package)
    shutil.copytree(exe.parent, package)
    staged_exe = package / exe.name
    if not staged_exe.is_file():
        raise SystemExit("staged exe missing: %s" % staged_exe)
    rom_scene = package / "rom" / "scene"
    rom_scene.mkdir(parents=True, exist_ok=True)
    shutil.copy2(fixture_src, rom_scene / EXPECTED_SCENE)
    (rom_scene / "default_scene.txt").write_text(EXPECTED_SCENE + "\n", encoding="ascii")
    (package / "rom" / "skip_boot_preload").write_text("", encoding="ascii")
    stage_fixture_scene_assets(package, fixture_src)
    return staged_exe, package


class Link:
    """Request helper that manages reconnection and captures."""

    def __init__(self, pid, run_id, report):
        self.pid, self.run_id, self.report = pid, run_id, report
        self.client = Client(pid, run_id, timeout=5)
        self.counter = 0

    def _next(self):
        self.counter += 1
        return self.counter

    def req(self, verb, payload=None):
        for attempt in (0, 1):
            request_id = self._next()
            try:
                return self.client.request(verb, request_id, payload)
            except OSError as exc:
                self.report.setdefault("reconnects", []).append(
                    {"t": round(time.monotonic() - self.report["startedAt"], 3), "error": str(exc)})
                try:
                    self.client.close()
                except Exception:
                    pass
                self.client = Client(self.pid, self.run_id, timeout=5)
                if attempt:
                    raise
        raise RuntimeError("unreachable")

    def observe(self, target):
        return self.req("observe", {"target": target})

    def capture(self, timeout=6.0):
        reply = self.client.capture(self._next(), timeout)
        self.counter = max(self.counter, int(reply.get("requestId") or self.counter))
        return reply

    def probe_rejection(self, payload_dict, label):
        q = dict(payload_dict)
        q.setdefault("protocolVersion", 1)
        q.setdefault("requestId", self._next())
        q.setdefault("runId", self.run_id)
        frame = json.dumps(q, separators=(",", ":")).encode("utf-8") + b"\n"
        try:
            self.client.send_raw(frame)
            resp = self.client.read_response()
        except Exception as exc:
            return {"label": label, "pass": False, "error": f"exception: {exc}", "response": None}
        is_ok_false = validate_rejection_response(resp)
        return {
            "label": label,
            "pass": is_ok_false,
            "error": None if is_ok_false else f"expected ok:false, got {resp}",
            "response": resp,
        }

    def close(self):
        try:
            self.client.close()
        except Exception:
            pass


def run(args):
    exe = pathlib.Path(args.exe).resolve()
    if not exe.is_file():
        raise SystemExit("exe not found: %s" % exe)

    out_dir = pathlib.Path(args.out_dir).resolve()
    if out_dir.exists():
        raise SystemExit("out dir already exists: %s" % out_dir)
    out_dir.mkdir(parents=True)

    staged_exe, package = stage_fixture_package(exe, out_dir, args.fixture_scene)
    launcher_session = session_of(os.getpid())
    report = {
        "schema": "ivy.runtime_bridge.mouse_acceptance/1",
        "startedAt": time.monotonic(),
        "exe": str(exe),
        "stagedExe": str(staged_exe),
        "sceneArg": SCENE_ARG,
        "exeSha256": hashlib.sha256(exe.read_bytes()).hexdigest(),
        "launcherSession": launcher_session,
        "checks": {},
        "status": "FAIL",
    }
    child = None
    try:
        run_id = uuid.uuid4().hex
        report["runId"] = run_id
        env = dict(os.environ)
        for key in ENV_CLEAR:
            env.pop(key, None)
        env.update({
            "IVY_RUNTIME_TEST_BRIDGE": "1",
            "IVY_RUNTIME_TEST_RUN_ID": run_id,
            "IVY_RUNTIME_TEST_OUTPUT_DIR": str(out_dir),
            "IVY_RUNTIME_TEST_FIXED_CAMERA": "1",
            "IVY_SKIP_CLOSE_CONFIRM": "1",
        })

        with (out_dir / "runtime-out.log").open("wb") as stdout, (out_dir / "runtime-err.log").open("wb") as stderr:
            child = subprocess.Popen(
                [str(staged_exe), "--scene", SCENE_ARG],
                cwd=str(package), env=env, stdout=stdout, stderr=stderr)
        report["pid"] = child.pid
        report["childSession"] = session_of(child.pid)
        if report["childSession"] == 0 or launcher_session == 0:
            raise AssertionError("not running in an interactive session")

        link = Link(child.pid, run_id, report)

        # 1. Bounded ready + playing check
        deadline = time.monotonic() + args.ready_timeout
        status = None
        while True:
            status = link.req("status")
            if status.get("ready") and status.get("playing"):
                break
            if time.monotonic() > deadline:
                raise AssertionError(
                    "runtime not ready within %.0fs (state=%r readyDetail=%r scene=%r)"
                    % (args.ready_timeout, status.get("state"), status.get("readyDetail"), status.get("scene")))
            if child.poll() is not None:
                raise AssertionError("runtime exited before ready: %s" % child.returncode)
            time.sleep(0.05)

        capabilities = sorted(status.get("capabilities") or [])
        if "mouse" not in capabilities:
            raise AssertionError("missing mouse capability in bridge status: %s" % capabilities)

        scene_name = pathlib.PurePosixPath(str(status.get("scene", "")).replace("\\", "/")).name
        if scene_name != args.expected_scene:
            raise AssertionError("unexpected scene %r (expected %r)" % (scene_name, args.expected_scene))

        report["checks"]["ready"] = {
            "pass": True,
            "scene": status.get("scene"),
            "state": status.get("state"),
            "capabilities": capabilities,
        }

        time.sleep(args.settle)

        # 2. Observe Player prerequisite
        initial_obs = link.observe("Player")
        if not initial_obs.get("found"):
            raise AssertionError("Player target not found in snapshot")
        initial_pos = initial_obs["position"]
        report["checks"]["observe_player"] = {
            "pass": True,
            "initialPosition": initial_pos,
            "initialVelocity": initial_obs.get("velocity"),
        }

        # 3. Before PNG capture
        before_reply = link.capture()
        before_cap = before_reply.get("capture") or {}
        before_name = pathlib.PurePath(str(before_cap.get("basename") or "")).name
        before_path = out_dir / before_name
        if not before_path.is_file():
            raise AssertionError(f"before capture PNG file not found: {before_name}")
        before_data = before_path.read_bytes()
        before_val = validate_png_bytes(before_data, before_cap.get("width"), before_cap.get("height"))
        if not before_val["pass"]:
            raise AssertionError(f"before PNG validation failed: {before_val['error']}")
        sha_before = before_val["sha256"]

        # 4. Protocol rejection validation (dx601, extra key, duplicate LEFT, invalid SIDE, wheel999)
        rejection_specs = [
            ({"verb": "mouse", "dx": 601, "dy": 0, "buttons": [], "wheel": 0, "updates": 60}, "dx601"),
            ({"verb": "mouse", "dx": 0, "dy": 0, "buttons": [], "wheel": 0, "updates": 60, "extra": "invalid"}, "extra_key"),
            ({"verb": "mouse", "dx": 0, "dy": 0, "buttons": ["LEFT", "LEFT"], "wheel": 0, "updates": 60}, "duplicate_LEFT"),
            ({"verb": "mouse", "dx": 0, "dy": 0, "buttons": ["SIDE"], "wheel": 0, "updates": 60}, "invalid_SIDE"),
            ({"verb": "mouse", "dx": 0, "dy": 0, "buttons": [], "wheel": 999, "updates": 60}, "wheel999"),
        ]
        rejection_results = []
        for payload, label in rejection_specs:
            probe_res = link.probe_rejection(payload, label)
            rejection_results.append(probe_res)
            if not probe_res["pass"]:
                raise AssertionError(f"rejection check failed for {label}: {probe_res['error']}")
        report["checks"]["rejections"] = {
            "pass": all(r["pass"] for r in rejection_results),
            "results": rejection_results,
        }

        # 5. Injected mouse forward: dx 4, dy 0, wheel 0, buttons [], updates 60
        ack_fwd = link.req("mouse", {"dx": 4, "dy": 0, "buttons": [], "wheel": 0, "updates": args.updates})
        if ack_fwd.get("ok") is not True or ack_fwd.get("status") != "queued":
            raise AssertionError(f"forward mouse ack not queued: {ack_fwd}")

        samples = []
        fwd_start = time.monotonic()
        fwd_deadline = fwd_start + args.timeout
        while True:
            cur_status = link.req("status")
            m_info = cur_status.get("mouse") or {}
            obs = link.observe("Player")
            samples.append({
                "phase": "forward",
                "t": round(time.monotonic() - fwd_start, 3),
                "status": m_info.get("status"),
                "remaining": m_info.get("remainingUpdates"),
                "position": obs.get("position"),
                "velocity": obs.get("velocity"),
            })
            if m_info.get("status") == "completed" and m_info.get("remainingUpdates") == 0:
                break
            if time.monotonic() > fwd_deadline:
                raise AssertionError("forward mouse operation did not complete within deadline")
            time.sleep(args.poll_interval)

        # Capture afterForward PNG
        fwd_reply = link.capture()
        fwd_cap = fwd_reply.get("capture") or {}
        fwd_name = pathlib.PurePath(str(fwd_cap.get("basename") or "")).name
        fwd_path = out_dir / fwd_name
        if not fwd_path.is_file():
            raise AssertionError(f"afterForward capture PNG file not found: {fwd_name}")
        fwd_data = fwd_path.read_bytes()
        fwd_val = validate_png_bytes(fwd_data, fwd_cap.get("width"), fwd_cap.get("height"))
        if not fwd_val["pass"]:
            raise AssertionError(f"afterForward PNG validation failed: {fwd_val['error']}")
        sha_forward = fwd_val["sha256"]

        # 6. Injected mouse reverse: dx -4, dy 0, wheel 0, buttons [], updates 60
        ack_rev = link.req("mouse", {"dx": -4, "dy": 0, "buttons": [], "wheel": 0, "updates": args.updates})
        if ack_rev.get("ok") is not True or ack_rev.get("status") != "queued":
            raise AssertionError(f"reverse mouse ack not queued: {ack_rev}")

        rev_start = time.monotonic()
        rev_deadline = rev_start + args.timeout
        while True:
            cur_status = link.req("status")
            m_info = cur_status.get("mouse") or {}
            obs = link.observe("Player")
            samples.append({
                "phase": "reverse",
                "t": round(time.monotonic() - rev_start, 3),
                "status": m_info.get("status"),
                "remaining": m_info.get("remainingUpdates"),
                "position": obs.get("position"),
                "velocity": obs.get("velocity"),
            })
            if m_info.get("status") == "completed" and m_info.get("remainingUpdates") == 0:
                break
            if time.monotonic() > rev_deadline:
                raise AssertionError("reverse mouse operation did not complete within deadline")
            time.sleep(args.poll_interval)

        # Capture afterReverse PNG
        rev_reply = link.capture()
        rev_cap = rev_reply.get("capture") or {}
        rev_name = pathlib.PurePath(str(rev_cap.get("basename") or "")).name
        rev_path = out_dir / rev_name
        if not rev_path.is_file():
            raise AssertionError(f"afterReverse capture PNG file not found: {rev_name}")
        rev_data = rev_path.read_bytes()
        rev_val = validate_png_bytes(rev_data, rev_cap.get("width"), rev_cap.get("height"))
        if not rev_val["pass"]:
            raise AssertionError(f"afterReverse PNG validation failed: {rev_val['error']}")
        sha_reverse = rev_val["sha256"]

        report["samples"] = samples

        # 7. Player position and velocity validation across samples
        sample_val = validate_samples(samples, initial_pos, pos_tolerance=0.05)
        if not sample_val["pass"]:
            raise AssertionError(f"player samples validation failed: {sample_val['error']}")
        report["checks"]["player_isolation"] = sample_val

        # 8. View digest changes validation
        hashes_pass = validate_frame_hashes(sha_before, sha_forward, sha_reverse)
        report["checks"]["view_digest"] = {
            "pass": hashes_pass,
            "beforeSha256": sha_before,
            "forwardSha256": sha_forward,
            "reverseSha256": sha_reverse,
            "note": "Hash changes prove frame differences, not precise camera yaw or reverse restoration",
        }
        if not hashes_pass:
            raise AssertionError(f"view digest differences failed: before={sha_before}, fwd={sha_forward}, rev={sha_reverse}")

        # 9. Clean quit receipt and native exit 0
        quit_reply = link.req("quit")
        quit_ok = quit_reply.get("ok") is True and quit_reply.get("requiresReceipt") is True
        report["quitReceipt"] = {
            "ok": quit_reply.get("ok"),
            "requiresReceipt": quit_reply.get("requiresReceipt"),
            "pass": quit_ok,
        }
        if not quit_ok:
            raise AssertionError(f"invalid quit receipt reply: {quit_reply}")
        link.close()

        try:
            exit_code = child.wait(timeout=args.shutdown_timeout)
        except subprocess.TimeoutExpired:
            exit_code = None

        shutdown_pass = validate_shutdown(exit_code)
        report["shutdown"] = {
            "exitCode": exit_code,
            "status": "PASS" if shutdown_pass else ("TIMEOUT" if exit_code is None else "CRASH"),
            "pass": shutdown_pass,
        }
        if not shutdown_pass:
            raise AssertionError(f"shutdown not clean exit 0: exitCode={exit_code}")

        report["status"] = "PASS"
    except Exception:
        report["error"] = traceback.format_exc()
    finally:
        if child is not None and child.poll() is None:
            child.kill()
            child.wait(timeout=5)
        report["childCleanedUp"] = child is None or child.poll() is not None
        report["elapsedSeconds"] = round(time.monotonic() - report["startedAt"], 2)
        (out_dir / "result.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
        print(json.dumps({k: report[k] for k in ("status", "runId", "pid", "exeSha256", "checks", "shutdown", "error", "elapsedSeconds") if k in report}, ensure_ascii=False, indent=2))

    return 0 if report["status"] == "PASS" else 1


def main():
    parser = argparse.ArgumentParser(description="Mouse runtime acceptance runner for Ivy test bridge")
    parser.add_argument("--exe", required=True, help="Debug GAME_RUNTIME executable to stage and launch")
    parser.add_argument("--out-dir", required=True, help="new directory for fixture package, PNGs, logs and result.json")
    parser.add_argument(
        "--fixture-scene",
        default=str(DEFAULT_FIXTURE),
        help="path to scene_runtime_bridge_player.json",
    )
    parser.add_argument("--expected-scene", default=EXPECTED_SCENE)
    parser.add_argument("--updates", type=int, default=60)
    parser.add_argument("--settle", type=float, default=0.6)
    parser.add_argument("--ready-timeout", type=float, default=25.0)
    parser.add_argument("--shutdown-timeout", type=float, default=15.0)
    parser.add_argument("--timeout", type=float, default=10.0)
    parser.add_argument("--poll-interval", type=float, default=0.05)
    return run(parser.parse_args())


if __name__ == "__main__":
    raise SystemExit(main())
