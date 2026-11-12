"""Live acceptance: one Client connection survives N mixed ops with idle gaps.

Fails on any reconnect. Reuses the world-manip fixture/assets already staged by
other acceptance scripts (no extra content beyond existing checks).
"""
import argparse
import json
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

from runtime_bridge_mouse_acceptance import ENV_CLEAR, session_of
from runtime_bridge_world_manip_acceptance import (
    DEFAULT_FIXTURE,
    OBJECT_NAME,
    SCENE_ARG,
    stage_fixture_package,
    wait_ready,
)
from runtime_test_client import Client

MIN_OPS = 10
IDLE_GAP_SEC = 2.5


class StrictLink:
    """Single Client; any OSError / reconnect is a hard failure."""

    def __init__(self, pid, run_id):
        self.pid = pid
        self.run_id = run_id
        self.client = Client(pid, run_id, timeout=5)
        self.counter = 0
        self.ops = 0

    def _next(self):
        self.counter += 1
        return self.counter

    def req(self, verb, payload=None):
        request_id = self._next()
        reply = self.client.request(verb, request_id, payload)
        self.ops += 1
        return reply

    def set_transform(self, object_name, position, completion_timeout=5.0):
        rid = self._next()
        # Helper consumes additional status request ids; sync counter afterward.
        reply = self.client.set_transform(
            object_name, rid, position=position, completion_timeout=completion_timeout)
        self.counter = max(self.counter, int(reply.get("requestId", rid)))
        self.ops += 1
        return reply

    def capture(self, completion_timeout=8.0):
        rid = self._next()
        reply = self.client.capture(rid, completion_timeout=completion_timeout)
        self.counter = max(self.counter, int(reply.get("requestId", rid)))
        self.ops += 1
        return reply

    def keyboard(self, keys, updates, completion_timeout=7.0):
        rid = self._next()
        reply = self.client.keyboard(keys, updates, rid, completion_timeout=completion_timeout)
        self.counter = max(self.counter, int(reply.get("requestId", rid)))
        self.ops += 1
        return reply

    def close(self):
        self.client.close()


def run(args):
    exe = pathlib.Path(args.exe).resolve()
    if not exe.is_file():
        raise SystemExit("exe not found: %s" % exe)
    out_dir = pathlib.Path(args.out_dir).resolve()
    if out_dir.exists():
        raise SystemExit("out dir already exists: %s" % out_dir)
    out_dir.mkdir(parents=True)

    staged_exe, package = stage_fixture_package(exe, out_dir, args.fixture_scene)
    report = {
        "schema": "ivy.runtime_bridge.connection_acceptance/1",
        "startedAt": time.monotonic(),
        "exe": str(exe),
        "stagedExe": str(staged_exe),
        "sceneArg": SCENE_ARG,
        "launcherSession": session_of(os.getpid()),
        "minOps": MIN_OPS,
        "idleGapSec": IDLE_GAP_SEC,
        "reconnects": [],
        "checks": {},
        "status": "FAIL",
    }
    child = None
    link = None
    try:
        run_id = uuid.uuid4().hex
        report["runId"] = run_id
        env = dict(os.environ)
        for key in ENV_CLEAR:
            env.pop(key, None)
        env["IVY_RUNTIME_TEST_BRIDGE"] = "1"
        env["IVY_RUNTIME_TEST_RUN_ID"] = run_id
        env["IVY_RUNTIME_TEST_OUTPUT_DIR"] = str(out_dir)
        env["IVY_RUNTIME_TEST_FIXED_CAMERA"] = "1"
        env["IVY_SKIP_CLOSE_CONFIRM"] = "1"
        child = subprocess.Popen(
            [str(staged_exe), "--scene", SCENE_ARG],
            cwd=str(package),
            env=env,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
        )
        report["pid"] = child.pid
        # Connect once; do not wrap with reconnect helpers.
        deadline = time.monotonic() + 30.0
        last_err = None
        while time.monotonic() < deadline:
            try:
                link = StrictLink(child.pid, run_id)
                break
            except Exception as exc:
                last_err = exc
                time.sleep(0.2)
        if link is None:
            raise TimeoutError("initial Client connect failed: %s" % last_err)

        # wait_ready uses link.req — still one connection.
        class _ReadyAdapter:
            def __init__(self, strict):
                self._strict = strict
            def req(self, verb, payload=None):
                return self._strict.req(verb, payload)
        wait_ready(_ReadyAdapter(link), timeout=25.0)

        hello = link.req("hello")
        report["checks"]["hello"] = {"pass": hello.get("ok") is True}

        time.sleep(IDLE_GAP_SEC)
        st = link.req("status")
        report["checks"]["status_after_idle"] = {"pass": st.get("ok") is True, "ready": st.get("ready")}

        listed = link.req("list_objects", {"limit": 32})
        names = [o.get("name") for o in listed.get("objects", [])]
        report["checks"]["list_objects"] = {"pass": OBJECT_NAME in names, "names": names}
        if OBJECT_NAME not in names:
            raise RuntimeError("DynamicBox missing from list_objects")

        perf = link.req("perf", {"frames": 5})
        report["checks"]["perf"] = {"pass": perf.get("ok") is True}

        time.sleep(IDLE_GAP_SEC)
        tf = link.set_transform(OBJECT_NAME, position=[1.0, 2.0, 0.0])
        report["checks"]["set_transform"] = {
            "pass": (tf.get("worldOp") or {}).get("status") == "completed",
            "worldOp": tf.get("worldOp"),
        }
        if not report["checks"]["set_transform"]["pass"]:
            raise RuntimeError("set_transform did not complete: %s" % tf)

        time.sleep(IDLE_GAP_SEC)
        kb = link.keyboard(["W"], 2)
        report["checks"]["keyboard"] = {
            "pass": (kb.get("keyboard") or {}).get("status") == "completed",
            "keyboard": kb.get("keyboard"),
        }
        if not report["checks"]["keyboard"]["pass"]:
            raise RuntimeError("keyboard did not complete: %s" % kb)

        cap = link.capture(completion_timeout=10.0)
        cap_body = cap.get("capture") or {}
        path = Client.capture_output_path(cap_body, output_dir=str(out_dir))
        path_ok = isinstance(path, str) and path.endswith(cap_body.get("basename") or "missing")
        report["checks"]["capture"] = {
            "pass": cap_body.get("status") == "completed" and path_ok and bool(cap_body.get("path")),
            "basename": cap_body.get("basename"),
            "path": cap_body.get("path"),
            "resolved": path,
        }
        if not report["checks"]["capture"]["pass"]:
            raise RuntimeError("capture incomplete or missing path: %s" % cap_body)

        # More status/perf ops to clear MIN_OPS with the same handle.
        for i in range(3):
            time.sleep(IDLE_GAP_SEC if i == 1 else 0.05)
            link.req("status")
            link.req("perf", {"frames": 3})

        report["ops"] = link.ops
        report["checks"]["op_count"] = {"pass": link.ops >= MIN_OPS, "ops": link.ops}
        if link.ops < MIN_OPS:
            raise RuntimeError("expected >= %d ops on one connection, got %d" % (MIN_OPS, link.ops))
        report["checks"]["no_reconnect"] = {"pass": True, "handleAlive": link.client.handle is not None}
        if link.client.handle is None:
            raise RuntimeError("client handle closed unexpectedly")

        quit_reply = link.req("quit")
        quit_ok = quit_reply.get("ok") is True and quit_reply.get("requiresReceipt") is True
        report["checks"]["quit"] = {"pass": quit_ok}
        if not quit_ok:
            raise RuntimeError("invalid quit reply: %s" % quit_reply)
        link.close()
        link = None
        exit_code = child.wait(timeout=15)
        report["exitCode"] = exit_code
        report["checks"]["exit"] = {"pass": exit_code == 0, "exitCode": exit_code}
        if exit_code != 0:
            raise RuntimeError("runtime exit code %s" % exit_code)

        report["status"] = "PASS"
        return 0
    except Exception as exc:
        report["error"] = str(exc)
        report["traceback"] = traceback.format_exc()
        if "reconnects" in report and not report["reconnects"]:
            # Surface pipe errors explicitly for the Windows verification plan.
            report["reconnects"].append({"error": str(exc)})
        if child is not None and child.poll() is None:
            child.kill()
            try:
                child.wait(timeout=5)
            except Exception:
                pass
        return 1
    finally:
        if link is not None:
            link.close()
        if child is not None and child.stderr:
            err = child.stderr.read()
            if err:
                (out_dir / "runtime.stderr.txt").write_bytes(err)
            child.stderr.close()
        report["elapsedSec"] = round(time.monotonic() - report["startedAt"], 3)
        (out_dir / "result.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
        print(json.dumps({"status": report["status"], "outDir": str(out_dir)}, ensure_ascii=False))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", required=True, help="path to Debug IvyEngineRuntime.exe")
    parser.add_argument("--out-dir", required=True)
    parser.add_argument("--fixture-scene", default=str(DEFAULT_FIXTURE),
                        help="path to scene_runtime_bridge_world_manip.json")
    return run(parser.parse_args())


if __name__ == "__main__":
    raise SystemExit(main())
