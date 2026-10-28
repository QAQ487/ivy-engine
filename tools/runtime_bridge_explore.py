"""Bounded stdlib Phase-6 explore host: parent LLM writes commands; host executes bridge acts.

File exchange only (command-N.json / response-N.json / ready.json / summary.json).
No autonomous navigation policy. Import-safe validators; Windows runtime launch.
Transport/clean-shutdown PASS is not navigation-objective PASS.
"""
import argparse
import hashlib
import json
import math
import os
import pathlib
import subprocess
import sys
import time
import traceback
import uuid

HERE = pathlib.Path(__file__).resolve().parent
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

from runtime_bridge_mouse_acceptance import ENV_CLEAR, Link, session_of, validate_png_bytes
from runtime_test_client import validate_events_reply

LANDMARKS = ("Landmark_North", "Landmark_South", "Landmark_East", "Landmark_West")
TARGETS = ("Player",) + LANDMARKS
ALLOWED_ACTIONS = frozenset({"controller", "capture", "quit", "events"})
ALLOWED_CONTROLLER_STATUS = frozenset({
    "queued", "preparing", "active", "releasing", "completed", "cancelled",
})
MAX_COMMAND_BYTES = 4096
KEEPALIVE_INTERVAL = 0.5  # bridge idle disconnect ~2s; keep status traffic bounded
DEFAULT_SCENE = "scene_runtime_bridge_player.json"


def _finite(value):
    return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)


def atomic_write_json(path, obj):
    path = pathlib.Path(path)
    tmp = path.with_name(path.name + ".tmp")
    tmp.write_text(json.dumps(obj, ensure_ascii=False, indent=2), encoding="utf-8")
    os.replace(tmp, path)


def validate_explore_args(args):
    if not isinstance(getattr(args, "exe", None), str) or not args.exe:
        return "exe path required"
    if not isinstance(getattr(args, "out_dir", None), str) or not args.out_dir:
        return "out-dir required"
    scene = getattr(args, "expected_scene", None)
    if not isinstance(scene, str) or not scene or len(scene) > 256:
        return "expected-scene invalid"
    bounds = {
        "lifetime": (0.0, 240.0),
        "idle_deadline": (0.0, 90.0),
        "ready_timeout": (0.0, 60.0),
        "shutdown_timeout": (0.0, 60.0),
        "poll_interval": (0.0, 1.0),
    }
    for name, (lo, hi) in bounds.items():
        value = getattr(args, name, None)
        if not _finite(value) or not (lo < float(value) <= hi):
            return "%s must be finite in (%g,%g]" % (name, lo, hi)
    max_actions = getattr(args, "max_actions", None)
    if type(max_actions) is not int or not 1 <= max_actions <= 12:
        return "max-actions must be an int in [1,12]"
    return None


def validate_command(obj, expected_sequence):
    """Strict command schema. Returns (ok, error, normalized_or_none). Never raises."""
    try:
        if not isinstance(obj, dict):
            return False, "command must be a JSON object", None
        if "sequence" not in obj or "action" not in obj:
            return False, "sequence and action required", None
        seq = obj.get("sequence")
        if type(seq) is not int or seq != expected_sequence:
            return False, "sequence must equal %d" % expected_sequence, None
        action = obj.get("action")
        if not isinstance(action, str) or action not in ALLOWED_ACTIONS:
            return False, "action must be controller|capture|quit|events", None
        keys = set(obj.keys())
        if action == "controller":
            if keys != {"sequence", "action", "lx", "ly", "updates"}:
                return False, "controller allows only sequence,action,lx,ly,updates", None
            lx, ly, updates = obj.get("lx"), obj.get("ly"), obj.get("updates")
            if not _finite(lx) or not -1.0 <= float(lx) <= 1.0:
                return False, "lx must be finite in [-1,1]", None
            if not _finite(ly) or not -1.0 <= float(ly) <= 1.0:
                return False, "ly must be finite in [-1,1]", None
            if type(updates) is not int or not 1 <= updates <= 60:
                return False, "updates must be int in [1,60]", None
            return True, None, {"sequence": seq, "action": action, "lx": float(lx), "ly": float(ly), "updates": updates}
        if keys != {"sequence", "action"}:
            return False, "%s allows only sequence,action" % action, None
        return True, None, {"sequence": seq, "action": action}
    except Exception as exc:
        return False, "invalid command: %s" % exc, None


def read_command_file(path):
    """Stat then bounded read. Returns (ok, error, bytes_or_none)."""
    path = pathlib.Path(path)
    try:
        size = path.stat().st_size
    except OSError as exc:
        return False, "command stat failed: %s" % exc, None
    if size > MAX_COMMAND_BYTES:
        return False, "command file exceeds %d bytes" % MAX_COMMAND_BYTES, None
    try:
        with path.open("rb") as fh:
            data = fh.read(MAX_COMMAND_BYTES + 1)
    except OSError as exc:
        return False, "command read failed: %s" % exc, None
    if len(data) > MAX_COMMAND_BYTES:
        return False, "command file exceeds %d bytes" % MAX_COMMAND_BYTES, None
    return True, None, data


def validate_observation_target(name, entry):
    if not isinstance(entry, dict):
        return False, "%s observation not a dict" % name
    if entry.get("found") is not True:
        return False, "%s found is not true" % name
    pos, vel = entry.get("position"), entry.get("velocity")
    if not isinstance(pos, dict) or not isinstance(vel, dict):
        return False, "%s missing position/velocity" % name
    for label, vector in (("position", pos), ("velocity", vel)):
        for axis in ("x", "y", "z"):
            if not _finite(vector.get(axis)):
                return False, "%s non-finite %s.%s" % (name, label, axis)
    return True, None


def validate_observations(observations):
    if not isinstance(observations, dict):
        return False, "observations not a dict"
    for name in TARGETS:
        if name not in observations:
            return False, "missing observation %s" % name
        ok, err = validate_observation_target(name, observations[name])
        if not ok:
            return False, err
    return True, None


def observe_all(link):
    out = {}
    for name in TARGETS:
        reply = link.observe(name)
        if not isinstance(reply, dict):
            raise RuntimeError("observe %s returned non-dict" % name)
        out[name] = {
            "found": reply.get("found") is True,
            "position": reply.get("position"),
            "velocity": reply.get("velocity"),
        }
    ok, err = validate_observations(out)
    if not ok:
        raise RuntimeError(err)
    return out


def validate_controller_status_payload(status_reply, operation_id):
    if not isinstance(status_reply, dict) or status_reply.get("ok") is not True:
        return False, "status ok is not true", None
    info = status_reply.get("controller")
    if not isinstance(info, dict):
        return False, "controller status missing", None
    rid = info.get("requestId")
    if type(rid) is not int or rid != operation_id:
        return False, "controller requestId mismatch", info
    st = info.get("status")
    if st not in ALLOWED_CONTROLLER_STATUS:
        return False, "malformed controller status", info
    rem = info.get("remainingUpdates")
    if type(rem) is not int or not 0 <= rem <= 60:
        return False, "malformed remainingUpdates", info
    if st == "cancelled":
        return False, "controller cancelled", info
    return True, None, info


def poll_controller(link, operation_id, deadline, poll_interval):
    while time.monotonic() < deadline:
        status = link.req("status")
        ok, err, info = validate_controller_status_payload(status, operation_id)
        if not ok:
            return False, err, info
        if info.get("status") == "completed" and info.get("remainingUpdates") == 0:
            return True, None, info
        time.sleep(poll_interval)
    return False, "controller completion deadline", None


def request_controller_once(link, lx, ly, updates):
    """Single-shot controller request. Never retries/replays on uncertain ACK."""
    request_id = link._next()
    try:
        return link.client.request("controller", request_id, {"lx": lx, "ly": ly, "updates": updates})
    except Exception:
        try:
            link.client.close()
        except Exception:
            pass
        raise


def wait_for_command(cmd_path, child, link, last_cmd_at, idle_deadline, lifetime_deadline,
                     poll_interval, monotonic=time.monotonic, sleep=time.sleep):
    """Wait for parent command with <=0.5s status keepalives.

    Keepalives do NOT reset last_cmd_at (parent idle). Status may use Link.req reconnect.
    Returns (event, detail): ready|idle|lifetime|child-exit|keepalive-fail.
    """
    last_keepalive = monotonic() - KEEPALIVE_INTERVAL
    while True:
        now = monotonic()
        if now >= lifetime_deadline:
            return "lifetime", None
        if now - last_cmd_at >= idle_deadline:
            return "idle", None
        if child.poll() is not None:
            return "child-exit", child.returncode
        if pathlib.Path(cmd_path).is_file():
            return "ready", None
        if now - last_keepalive >= KEEPALIVE_INTERVAL:
            try:
                st = link.req("status")
                if not isinstance(st, dict) or st.get("ok") is not True:
                    return "keepalive-fail", "status keepalive ok is not true: %s" % st
            except Exception as exc:
                return "keepalive-fail", "status keepalive failed: %s" % exc
            last_keepalive = monotonic()
        remain_idle = idle_deadline - (monotonic() - last_cmd_at)
        remain_life = lifetime_deadline - monotonic()
        remain_ka = KEEPALIVE_INTERVAL - (monotonic() - last_keepalive)
        sleep(max(0.0, min(poll_interval, remain_idle, remain_life, remain_ka, KEEPALIVE_INTERVAL)))


def wait_child_exit(child, shutdown_timeout):
    """Wait for child with pipe still open. Returns (exitCode, timedOut, forcedKill)."""
    if child is None:
        return None, False, False
    if child.poll() is not None:
        return child.returncode, False, False
    try:
        return child.wait(timeout=shutdown_timeout), False, False
    except subprocess.TimeoutExpired:
        child.kill()
        child.wait(timeout=5)
        return child.returncode, True, True


def close_link(link):
    if link is None:
        return None
    try:
        link.close()
    except Exception:
        pass
    return None


def finalize_quit(child, link, summary, started, shutdown_timeout, out_dir, reply, next_seq, cmd_path, receipt_ok, receipt_error):
    """Wait/kill child while pipe stays open; close link only after; then summary+response."""
    exit_code, timed_out, forced_kill = wait_child_exit(child, shutdown_timeout)
    link = close_link(link)
    cleaned = child is None or child.poll() is not None
    # Force-killed exit 1 is shutdown timeout, not a spontaneous runtime crash label.
    quit_ok = receipt_ok is True and exit_code == 0 and not forced_kill and cleaned
    summary["shutdown"] = {
        "exitCode": exit_code,
        "rawExitCode": exit_code,
        "pass": exit_code == 0 and not forced_kill,
        "receiptOk": receipt_ok is True,
        "timedOut": timed_out,
        "forcedKill": forced_kill,
    }
    summary["childCleanedUp"] = cleaned
    summary["reason"] = "quit"
    summary["status"] = "PASS" if quit_ok else "FAIL"
    summary["elapsedSeconds"] = round(time.monotonic() - started, 2)
    if quit_ok:
        reply["ok"] = True
        reply["error"] = None
    else:
        reply["ok"] = False
        if receipt_error:
            reply["error"] = receipt_error
        elif forced_kill:
            reply["error"] = "shutdown timed out; child force-killed rawExitCode=%s" % exit_code
        else:
            reply["error"] = "child exit rawExitCode=%s" % exit_code
    reply["shutdown"] = summary["shutdown"]
    reply["observations"] = None
    if summary["trace"] and summary["trace"][-1].get("sequence") == next_seq:
        summary["trace"][-1]["ok"] = reply["ok"]
        summary["trace"][-1]["error"] = reply["error"]
    else:
        summary["trace"].append({
            "sequence": next_seq,
            "commandFile": cmd_path.name if cmd_path else None,
            "action": "quit",
            "ok": reply["ok"],
            "error": reply["error"],
        })
    atomic_write_json(out_dir / "summary.json", summary)
    atomic_write_json(out_dir / ("response-%d.json" % next_seq), reply)
    return 0 if quit_ok else 1, child, link


def run(args):
    err = validate_explore_args(args)
    if err:
        raise SystemExit(err)
    exe = pathlib.Path(args.exe).resolve()
    if not exe.is_file():
        raise SystemExit("exe not found: %s" % exe)
    out_dir = pathlib.Path(args.out_dir).resolve()
    if out_dir.exists():
        raise SystemExit("out dir already exists: %s" % out_dir)
    out_dir.mkdir(parents=True)

    started = time.monotonic()
    lifetime_deadline = started + args.lifetime
    summary = {
        "schema": "ivy.runtime_bridge.explore.summary/1",
        "status": "FAIL",
        "reason": None,
        "handled": 0,
        "trace": [],
        "exe": str(exe),
        "exeSha256": hashlib.sha256(exe.read_bytes()).hexdigest(),
        "note": "transport/clean-shutdown PASS is not navigation-objective PASS",
    }
    child = None
    link = None
    exit_code = None
    try:
        if session_of(os.getpid()) == 0:
            raise AssertionError("launcher session must be interactive nonzero")
        run_id = uuid.uuid4().hex
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
            child = subprocess.Popen([str(exe)], cwd=str(pathlib.Path.home()), env=env, stdout=stdout, stderr=stderr)
        if session_of(child.pid) == 0:
            raise AssertionError("child session must be interactive nonzero")
        report = {"startedAt": started}
        link = Link(child.pid, run_id, report)

        ready_deadline = time.monotonic() + args.ready_timeout
        status = None
        while True:
            status = link.req("status")
            if status.get("ready") and status.get("playing"):
                break
            if time.monotonic() > ready_deadline or child.poll() is not None:
                raise AssertionError("runtime not ready")
            time.sleep(0.05)
        caps = status.get("capabilities") or []
        if "controller" not in caps:
            raise AssertionError("missing controller capability")
        scene_name = pathlib.PurePosixPath(str(status.get("scene", "")).replace("\\", "/")).name
        if scene_name != args.expected_scene:
            raise AssertionError("unexpected scene %r" % scene_name)

        observations = observe_all(link)
        ready = {
            "schema": "ivy.runtime_bridge.explore.ready/1",
            "pid": child.pid,
            "runId": run_id,
            "exeSha256": summary["exeSha256"],
            "scene": status.get("scene"),
            "capabilities": sorted(caps),
            "observations": observations,
            "exchange": {
                "commandPattern": "command-<N>.json",
                "responsePattern": "response-<N>.json",
                "maxActions": args.max_actions,
                "actions": sorted(ALLOWED_ACTIONS),
            },
        }
        atomic_write_json(out_dir / "ready.json", ready)
        summary["runId"] = run_id
        summary["pid"] = child.pid

        next_seq = 1
        last_cmd_at = time.monotonic()
        while True:
            if next_seq > args.max_actions:
                summary["reason"] = "action-limit"
                break

            cmd_path = out_dir / ("command-%d.json" % next_seq)
            # Keepalive status while waiting; does not reset last_cmd_at (parent idle).
            event, detail = wait_for_command(
                cmd_path, child, link, last_cmd_at, args.idle_deadline, lifetime_deadline,
                args.poll_interval)
            if event == "lifetime":
                summary["reason"] = "lifetime"
                break
            if event == "idle":
                summary["reason"] = "idle"
                break
            if event == "child-exit":
                summary["reason"] = "child-exit"
                exit_code = detail
                break
            if event == "keepalive-fail":
                raise RuntimeError("pipe keepalive unrecoverable: %s" % detail)

            last_cmd_at = time.monotonic()
            reply = {
                "schema": "ivy.runtime_bridge.explore.response/1",
                "sequence": next_seq,
                "ok": False,
                "error": None,
                "action": None,
                "observations": None,
            }
            quit_requested = False
            try:
                rok, rerr, raw_bytes = read_command_file(cmd_path)
                if not rok:
                    raise ValueError(rerr)
                obj = json.loads(raw_bytes.decode("utf-8"))
                ok, cerr, cmd = validate_command(obj, next_seq)
                if not ok:
                    raise ValueError(cerr)
                reply["action"] = cmd["action"]
                if cmd["action"] == "controller":
                    # No Link.req retry: uncertain ACK must not replay controller.
                    ack = request_controller_once(link, cmd["lx"], cmd["ly"], cmd["updates"])
                    if ack.get("ok") is not True or ack.get("status") != "queued":
                        raise RuntimeError("controller not queued: %s" % ack)
                    op_id = ack.get("requestId")
                    if type(op_id) is not int:
                        raise RuntimeError("missing controller requestId")
                    done, perr, info = poll_controller(
                        link, op_id, min(lifetime_deadline, time.monotonic() + 10.0), args.poll_interval)
                    reply["controller"] = info
                    if not done:
                        raise RuntimeError(perr)
                elif cmd["action"] == "capture":
                    cap_reply = link.capture()
                    cap = cap_reply.get("capture") or {}
                    name = pathlib.PurePath(str(cap.get("basename") or "")).name
                    path = out_dir / name
                    if not name or not path.is_file() or path.resolve().parent != out_dir.resolve():
                        raise RuntimeError("capture file missing or outside out-dir")
                    data = path.read_bytes()
                    png = validate_png_bytes(data, cap.get("width"), cap.get("height"))
                    if not png["pass"]:
                        raise RuntimeError("capture PNG invalid: %s" % png["error"])
                    reply["capture"] = {
                        "basename": name,
                        "width": png["width"],
                        "height": png["height"],
                        "bytes": png["bytes"],
                        "sha256": png["sha256"],
                    }
                elif cmd["action"] == "events":
                    ev_reply = link.req("events")
                    ok_ev, ev_err = validate_events_reply(ev_reply)
                    if not ok_ev:
                        raise RuntimeError(ev_err)
                    reply["events"] = {
                        "events": ev_reply.get("events"),
                        "latestSequence": ev_reply.get("latestSequence"),
                        "dropped": ev_reply.get("dropped"),
                        "requestId": ev_reply.get("requestId"),
                        "runId": ev_reply.get("runId"),
                    }
                elif cmd["action"] == "quit":
                    quit_requested = True

                if not quit_requested:
                    reply["observations"] = observe_all(link)
                    reply["ok"] = True
                    reply["error"] = None
            except Exception as exc:
                reply["ok"] = False
                reply["error"] = str(exc)
                if reply["observations"] is None and not quit_requested:
                    try:
                        reply["observations"] = observe_all(link)
                    except Exception:
                        reply["observations"] = None

            summary["trace"].append({
                "sequence": next_seq,
                "commandFile": cmd_path.name,
                "action": reply.get("action"),
                "ok": reply["ok"],
                "error": reply["error"],
            })
            summary["handled"] = next_seq

            if quit_requested:
                receipt_ok = False
                receipt_error = reply.get("error")
                try:
                    if reply.get("error") is None:
                        q = link.req("quit")
                        if q.get("ok") is True and q.get("requiresReceipt") is True:
                            receipt_ok = True
                        else:
                            receipt_error = "invalid quit receipt: %s" % q
                except Exception as exc:
                    receipt_ok = False
                    receipt_error = "quit exception: %s" % exc
                code, child, link = finalize_quit(
                    child, link, summary, started, args.shutdown_timeout, out_dir,
                    reply, next_seq, cmd_path, receipt_ok, receipt_error)
                return code

            atomic_write_json(out_dir / ("response-%d.json" % next_seq), reply)
            next_seq += 1

        # Idle/lifetime/action-limit: quit with pipe open until wait completes; close after.
        timed_out = False
        forced_kill = False
        if child.poll() is None:
            try:
                if link is not None:
                    link.req("quit")
            except Exception:
                pass
            exit_code, timed_out, forced_kill = wait_child_exit(child, args.shutdown_timeout)
            link = close_link(link)
        else:
            exit_code = child.returncode
            link = close_link(link)
        summary["shutdown"] = {
            "exitCode": exit_code,
            "rawExitCode": exit_code,
            "pass": exit_code == 0 and not forced_kill,
            "timedOut": timed_out,
            "forcedKill": forced_kill,
        }
        summary["status"] = "PASS" if (
            summary["reason"] in ("idle", "lifetime", "action-limit")
            and exit_code == 0 and not forced_kill) else "FAIL"
    except Exception:
        summary["error"] = traceback.format_exc()
        summary["reason"] = summary.get("reason") or "exception"
        summary["status"] = "FAIL"
    finally:
        if link is not None:
            try:
                link.close()
            except Exception:
                pass
        if child is not None and child.poll() is None:
            child.kill()
            child.wait(timeout=5)
        summary["childCleanedUp"] = child is None or child.poll() is not None
        summary["elapsedSeconds"] = round(time.monotonic() - started, 2)
        if "shutdown" not in summary:
            summary["shutdown"] = {"exitCode": child.returncode if child is not None else None, "pass": False}
        atomic_write_json(out_dir / "summary.json", summary)
    return 0 if summary.get("status") == "PASS" else 1


def main():
    p = argparse.ArgumentParser(description="Phase-6 runtime explore host (parent LLM file exchange)")
    p.add_argument("--exe", required=True)
    p.add_argument("--out-dir", required=True)
    p.add_argument("--expected-scene", default=DEFAULT_SCENE)
    p.add_argument("--lifetime", type=float, default=180.0)
    p.add_argument("--idle-deadline", type=float, default=60.0)
    p.add_argument("--max-actions", type=int, default=12)
    p.add_argument("--ready-timeout", type=float, default=25.0)
    p.add_argument("--shutdown-timeout", type=float, default=15.0)
    p.add_argument("--poll-interval", type=float, default=0.05)
    args = p.parse_args()
    err = validate_explore_args(args)
    if err:
        raise SystemExit(err)
    return run(args)


if __name__ == "__main__":
    raise SystemExit(main())
