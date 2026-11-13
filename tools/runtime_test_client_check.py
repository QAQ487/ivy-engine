"""Windows stdlib checks; runs only the renderer-independent bridge harness."""
import argparse
import json
import os
import subprocess
import sys
import time
import unittest
import uuid
from runtime_test_client import Client

class ClientChecks(unittest.TestCase):
    def test_real_keyboard_completion(self):
        run=uuid.uuid4().hex
        env=dict(os.environ, IVY_RUNTIME_TEST_BRIDGE="1", IVY_RUNTIME_TEST_RUN_ID=run)
        p=subprocess.Popen([os.environ["IVY_BRIDGE_HARNESS"],"--serve"],env=env,stdout=subprocess.PIPE,text=True)
        try:
            with Client(p.pid,run,timeout=2) as c:
                r=c.keyboard(["W","SPACE"],5,1)
                self.assertTrue(r["ok"])
                self.assertEqual(r["keyboard"]["status"],"completed")
                self.assertEqual(r["keyboard"]["requestId"],1)
                self.assertEqual(r["keyboard"]["remainingUpdates"],0)
                self.assertTrue(c.request("quit",r["requestId"]+1)["ok"])
            self.assertEqual(p.wait(timeout=3),0)
        finally:
            if p.poll() is None: p.kill();p.wait(timeout=3)
            p.stdout.close()

    def test_keyboard_completion_timeout_closes_connection(self):
        run=uuid.uuid4().hex
        env=dict(os.environ, IVY_RUNTIME_TEST_BRIDGE="1", IVY_RUNTIME_TEST_RUN_ID=run)
        p=subprocess.Popen([os.environ["IVY_BRIDGE_HARNESS"],"--stall"],env=env,stdout=subprocess.PIPE,text=True)
        try:
            with Client(p.pid,run) as c:
                start=time.monotonic()
                with self.assertRaises(TimeoutError): c.keyboard(["W"],1,1,completion_timeout=.2)
                self.assertLess(time.monotonic()-start,.6)
                self.assertIsNone(c.handle)
            with Client(p.pid,run) as c:
                self.assertEqual(c.request("status",1000)["keyboard"]["status"],"cancelled")
                self.assertTrue(c.request("quit",1001)["ok"])
            self.assertEqual(p.wait(timeout=3),0)
        finally:
            if p.poll() is None: p.kill();p.wait(timeout=3)
            p.stdout.close()

    def test_real_keyboard_cli_completion(self):
        run=uuid.uuid4().hex
        env=dict(os.environ, IVY_RUNTIME_TEST_BRIDGE="1", IVY_RUNTIME_TEST_RUN_ID=run)
        p=subprocess.Popen([os.environ["IVY_BRIDGE_HARNESS"],"--serve"],env=env,stdout=subprocess.PIPE,text=True)
        try:
            cli=subprocess.run([sys.executable,os.path.join(os.path.dirname(__file__),"runtime_test_client.py"),
                "--pid",str(p.pid),"--run-id",run,"--request-id","1","keyboard","--keys","W","SPACE","--updates","1"],
                capture_output=True,text=True,timeout=10)
            self.assertEqual(cli.returncode,0,cli.stderr)
            r=json.loads(cli.stdout)
            self.assertEqual(r["keyboard"]["status"],"completed")
            with Client(p.pid,run) as c: self.assertTrue(c.request("quit",r["requestId"]+1)["ok"])
            self.assertEqual(p.wait(timeout=3),0)
        finally:
            if p.poll() is None: p.kill();p.wait(timeout=3)
            p.stdout.close()

    def test_keyboard_client_rejects_invalid_payload_before_send(self):
        run=uuid.uuid4().hex
        env=dict(os.environ, IVY_RUNTIME_TEST_BRIDGE="1", IVY_RUNTIME_TEST_RUN_ID=run)
        p=subprocess.Popen([os.environ["IVY_BRIDGE_HARNESS"],"--serve"],env=env,stdout=subprocess.PIPE,text=True)
        try:
            with Client(p.pid,run) as c:
                for keys,updates in ((["W"],True),(["W"],1.0),(["W"],0),(["W"],121),(["W","W"],1),(["NOTAKEY"],1),(["LEFTWINDOWS"],1),(["LALT","F4"],1),([],1),("W",1),([True],1)):
                    with self.subTest(keys=keys,updates=updates):
                        with self.assertRaises(ValueError): c.keyboard(keys,updates,1)
                self.assertTrue(c.request("quit",1)["ok"])
            self.assertEqual(p.wait(timeout=3),0)
        finally:
            if p.poll() is None: p.kill();p.wait(timeout=3)
            p.stdout.close()

    def test_keyboard_client_accepts_expanded_keys_before_send(self):
        run=uuid.uuid4().hex
        env=dict(os.environ, IVY_RUNTIME_TEST_BRIDGE="1", IVY_RUNTIME_TEST_RUN_ID=run)
        p=subprocess.Popen([os.environ["IVY_BRIDGE_HARNESS"],"--serve"],env=env,stdout=subprocess.PIPE,text=True)
        try:
            with Client(p.pid,run,timeout=2) as c:
                r=c.keyboard(["ESCAPE","ENTER","LCTRL","E"],3,1)
                self.assertTrue(r["ok"])
                self.assertEqual(r["keyboard"]["status"],"completed")
                self.assertTrue(c.request("quit",r["requestId"]+1)["ok"])
            self.assertEqual(p.wait(timeout=3),0)
        finally:
            if p.poll() is None: p.kill();p.wait(timeout=3)
            p.stdout.close()

    def test_idle_gap_keeps_single_connection(self):
        """Quiet gaps >2s must not force ERROR_PIPE_NOT_CONNECTED on the next request."""
        run=uuid.uuid4().hex
        env=dict(os.environ, IVY_RUNTIME_TEST_BRIDGE="1", IVY_RUNTIME_TEST_RUN_ID=run)
        p=subprocess.Popen([os.environ["IVY_BRIDGE_HARNESS"],"--serve"],env=env,stdout=subprocess.PIPE,text=True)
        try:
            with Client(p.pid,run,timeout=5) as c:
                self.assertTrue(c.request("status",1)["ok"])
                time.sleep(2.5)
                self.assertIsNotNone(c.handle)
                self.assertTrue(c.request("status",2)["ok"])
                self.assertTrue(c.request("hello",3)["ok"])
                time.sleep(2.5)
                r=c.keyboard(["W"],1,4)
                self.assertEqual(r["keyboard"]["status"],"completed")
                self.assertIsNotNone(c.handle)
                self.assertTrue(c.request("status",r["requestId"]+1)["ok"])
                self.assertTrue(c.request("quit",r["requestId"]+2)["ok"])
            self.assertEqual(p.wait(timeout=3),0)
        finally:
            if p.poll() is None: p.kill();p.wait(timeout=3)
            p.stdout.close()

    def test_real_hello_status_quit(self):
        run=uuid.uuid4().hex
        env=dict(os.environ, IVY_RUNTIME_TEST_BRIDGE="1", IVY_RUNTIME_TEST_RUN_ID=run)
        p=subprocess.Popen([os.environ["IVY_BRIDGE_HARNESS"],"--serve"],env=env,stdout=subprocess.PIPE,text=True)
        try:
            with Client(p.pid,run,timeout=2) as c:
                r=c.request("hello",1)
                self.assertEqual(r["pid"],p.pid)
                self.assertEqual(r["runId"],run)
                self.assertTrue(c.request("status",2)["ready"])
                self.assertTrue(c.request("quit",3)["ok"])
            self.assertEqual(p.wait(timeout=3),0)
        finally:
            if p.poll() is None: p.kill();p.wait(timeout=3)
            p.stdout.close()

    def test_real_rejections_partial_disconnect_reconnect(self):
        run=uuid.uuid4().hex
        env=dict(os.environ, IVY_RUNTIME_TEST_BRIDGE="1", IVY_RUNTIME_TEST_RUN_ID=run)
        p=subprocess.Popen([os.environ["IVY_BRIDGE_HARNESS"],"--serve"],env=env,stdout=subprocess.PIPE,text=True)
        try:
            with Client(p.pid,run) as c:
                c.send_raw(b"{\n")
                self.assertEqual(c.read_response()["error"],"malformed_json")
                self.assertEqual(c.request("act",1)["error"],"unknown_verb")
                self.assertEqual(c.request("simulate",2)["error"],"unknown_verb")
                self.assertEqual(c.request("hello",2)["error"],"duplicate_request_id")
                c.send_raw(b'{"protocolVersion":99,"requestId":3,"runId":"'+run.encode()+b'","verb":"hello"}\n')
                self.assertEqual(c.read_response()["error"],"unsupported_version")
                c.send_raw(b'{"protocolVersion":1,')
                time.sleep(.02)
                c.send_raw(b'"requestId":4,"runId":"'+run.encode()+b'","verb":"status"}\n')
                self.assertTrue(c.read_response()["ok"])
                # A delivered quit response without receipt must NOT exit.
                c.send_raw(json.dumps(dict(protocolVersion=1,requestId=5,runId=run,verb="quit")).encode()+b"\n")
                self.assertTrue(c.read_response()["requiresReceipt"])
            time.sleep(.05)
            self.assertIsNone(p.poll())
            with Client(p.pid,run) as c:
                self.assertTrue(c.request("status",6)["ok"])
                c.send_raw(b"{")
            time.sleep(.05)
            with Client(p.pid,run) as c:
                self.assertTrue(c.request("quit",7)["ok"])
            self.assertEqual(p.wait(timeout=3),0)
        finally:
            if p.poll() is None: p.kill();p.wait(timeout=3)
            p.stdout.close()

    def test_partial_frame_deadline_and_recovery(self):
        run=uuid.uuid4().hex
        env=dict(os.environ, IVY_RUNTIME_TEST_BRIDGE="1", IVY_RUNTIME_TEST_RUN_ID=run)
        p=subprocess.Popen([os.environ["IVY_BRIDGE_HARNESS"],"--serve"],env=env,stdout=subprocess.PIPE,text=True)
        try:
            with Client(p.pid,run,timeout=.1) as c:
                c.send_raw(b"{")
                start=time.monotonic()
                with self.assertRaises(TimeoutError): c.read_response()
                self.assertLess(time.monotonic()-start,.5)
                time.sleep(2.1)
            with Client(p.pid,run) as c: self.assertTrue(c.request("quit",1)["ok"])
            self.assertEqual(p.wait(timeout=3),0)
        finally:
            if p.poll() is None: p.kill();p.wait(timeout=3)
            p.stdout.close()

    def test_oversize_disconnect_and_recovery(self):
        run=uuid.uuid4().hex
        env=dict(os.environ, IVY_RUNTIME_TEST_BRIDGE="1", IVY_RUNTIME_TEST_RUN_ID=run)
        p=subprocess.Popen([os.environ["IVY_BRIDGE_HARNESS"],"--serve"],env=env,stdout=subprocess.PIPE,text=True)
        try:
            with Client(p.pid,run) as c:
                c.send_raw(b"x"*4097)
                try: self.assertEqual(c.read_response()["error"],"message_too_large")
                except OSError: pass # Oversize terminates the connection; error is best-effort.
            with Client(p.pid,run) as c: self.assertTrue(c.request("quit",1)["ok"])
            self.assertEqual(p.wait(timeout=3),0)
        finally:
            if p.poll() is None: p.kill();p.wait(timeout=3)
            p.stdout.close()

    def test_join_cancels_idle_partial_backpressure_and_receipt(self):
        for mode in ("idle","partial","backpressure","receipt"):
            with self.subTest(mode=mode):
                run=uuid.uuid4().hex
                env=dict(os.environ, IVY_RUNTIME_TEST_BRIDGE="1", IVY_RUNTIME_TEST_RUN_ID=run)
                p=subprocess.Popen([os.environ["IVY_BRIDGE_HARNESS"],"--cancel"],env=env,stdout=subprocess.PIPE,text=True)
                try:
                    with Client(p.pid,run) as c:
                        if mode=="partial": c.send_raw(b"{")
                        if mode=="receipt":
                            c.send_raw(json.dumps(dict(protocolVersion=1,requestId=1,runId=run,verb="quit")).encode()+b"\n")
                            self.assertTrue(c.read_response()["requiresReceipt"])
                        if mode=="backpressure":
                            # Fill outgoing kernel buffers while intentionally never reading replies.
                            data=b"".join(json.dumps(dict(protocolVersion=1,requestId=i,runId=run,verb="status")).encode()+b"\n" for i in range(1,101))
                            try: c.send_raw(data)
                            except OSError: pass
                        self.assertEqual(p.wait(timeout=2),0)
                finally:
                    if p.poll() is None: p.kill();p.wait(timeout=3)
                    p.stdout.close()

    def test_missing_endpoint_deadline(self):
        start=time.monotonic()
        with self.assertRaises(TimeoutError):
            Client(12345, "missing-"+uuid.uuid4().hex, timeout=.15)
        self.assertLess(time.monotonic()-start, 1)

    def test_observe_client_validation(self):
        c=Client.__new__(Client)
        with self.assertRaises(ValueError): c.observe("", 1)
        with self.assertRaises(ValueError): c.observe("x"*65, 1)
        with self.assertRaises(ValueError): c.observe(123, 1)

    def test_world_manip_client_validation(self):
        c=Client.__new__(Client)
        with self.assertRaises(ValueError): c.reset_scene(1, scene="../rom/scene/x.json")
        with self.assertRaises(ValueError): c.reset_scene(1, scene="rom/scene/x.txt")
        with self.assertRaises(ValueError): c.set_transform("", 1, position=[0, 0, 0])
        with self.assertRaises(ValueError): c.set_transform("Box", 1)
        with self.assertRaises(ValueError): c.set_transform("Box", 1, position=[1, 2])
        with self.assertRaises(ValueError): c.set_transform("Box", 1, position=[1, 2, float("nan")])
        with self.assertRaises(ValueError): c.set_transform("Box", 1, position=[1, 2, 3], zero_velocity="yes")
        with self.assertRaises(ValueError): c.set_transform("Box", 1, rotation=[0, 0, 0, 0])
        with self.assertRaises(ValueError): c.set_velocity("Box", 1)
        with self.assertRaises(ValueError): c.set_velocity("Box!", 1, linear=[1, 0, 0])
        with self.assertRaises(ValueError): c.apply_impulse("Box", [1, 0], 1)
        with self.assertRaises(ValueError): c.apply_impulse("Box", [1, 0, 0], 1, point=[0, 1])
        with self.assertRaises(ValueError): c.apply_torque_impulse("Box", [0, 1], 1)
        with self.assertRaises(ValueError): c.apply_impulse("Box", [1, 0, 0], 2147483647)

    def test_request_surfaces_bridge_error_when_identity_echoed(self):
        """Rejected replies with echoed requestId must not raise identity mismatch."""
        class FakePipeClient(Client):
            def __init__(self):
                self.pid=1
                self.run_id="reject-echo"
                self.timeout=2
                self.handle=object()
                self._reply=None
            def send_raw(self,data,deadline=None):
                return len(data)
            def read_response(self,deadline=None):
                return dict(self._reply)
        c=FakePipeClient()
        c._reply={"protocolVersion":1,"requestId":7,"runId":"reject-echo","ok":False,"error":"invalid_request"}
        r=c.request("mouse",7,{"dx":0,"dy":0,"buttons":[],"updates":1,"durationMs":100})
        self.assertEqual(r["error"],"invalid_request")
        self.assertEqual(r["requestId"],7)
        self.assertIs(r["ok"],False)
        c._reply={"protocolVersion":1,"requestId":None,"runId":"reject-echo","ok":False,"error":"invalid_request"}
        with self.assertRaisesRegex(RuntimeError,r"response identity mismatch"):
            c.request("mouse",8,{"dx":0,"dy":0,"buttons":[],"durationMs":0})
        c._reply={"protocolVersion":1,"requestId":9,"runId":"other-run","ok":False,"error":"invalid_request"}
        with self.assertRaisesRegex(RuntimeError,r"response identity mismatch"):
            c.request("mouse",9,{"dx":0,"dy":0,"buttons":[],"durationMs":0})

    def test_resolve_capture_path_helper(self):
        from runtime_test_client import resolve_capture_path
        with self.assertRaises(ValueError):
            resolve_capture_path("")
        with self.assertRaises(ValueError):
            resolve_capture_path("../x.png")
        with self.assertRaises(ValueError):
            resolve_capture_path("a/b.png")
        with self.assertRaises(ValueError):
            resolve_capture_path("ok.png", output_dir="")
        resolved = resolve_capture_path("capture-run-1.png", output_dir=r"C:\out")
        self.assertEqual(resolved, os.path.join(r"C:\out", "capture-run-1.png"))
        prev = os.environ.get("IVY_RUNTIME_TEST_OUTPUT_DIR")
        try:
            os.environ["IVY_RUNTIME_TEST_OUTPUT_DIR"] = r"D:\caps"
            self.assertEqual(resolve_capture_path("capture-run-2.png"),
                             os.path.join(r"D:\caps", "capture-run-2.png"))
        finally:
            if prev is None:
                os.environ.pop("IVY_RUNTIME_TEST_OUTPUT_DIR", None)
            else:
                os.environ["IVY_RUNTIME_TEST_OUTPUT_DIR"] = prev
        status = {
            "basename": "capture-run-3.png",
            "path": r"E:\out\capture-run-3.png",
            "status": "completed",
        }
        self.assertEqual(Client.capture_output_path(status), r"E:\out\capture-run-3.png")
        self.assertEqual(
            Client.capture_output_path({"basename": "capture-run-4.png", "status": "completed"},
                                       output_dir=r"F:\caps"),
            os.path.join(r"F:\caps", "capture-run-4.png"))

    def test_sequential_completion_helpers_keep_one_connection(self):
        """Completion-waiting helpers must not close the handle on success."""
        class FakeSequentialClient(Client):
            def __init__(self):
                self.pid = 1
                self.run_id = "seq-conn"
                self.timeout = 2
                self.handle = object()
                self.calls = []
                self._world_polls = 0
                self._cap_polls = 0
                self._kb_polls = 0
            def request(self, verb, request_id, payload=None, deadline=None):
                self.calls.append((verb, request_id))
                base = {"protocolVersion": 1, "runId": self.run_id, "requestId": request_id, "ok": True}
                if verb == "set_transform":
                    return dict(base, status="queued")
                if verb == "capture":
                    return dict(base, status="queued")
                if verb == "keyboard":
                    return dict(base, status="queued")
                if verb == "status":
                    if self._world_polls < 1 and any(c[0] == "set_transform" for c in self.calls):
                        self._world_polls += 1
                        return dict(base, worldOp={"status": "queued", "requestId": 1})
                    if self._world_polls == 1:
                        self._world_polls += 1
                        return dict(base, worldOp={"status": "completed", "requestId": 1, "verb": "set_transform"})
                    if self._cap_polls < 1:
                        self._cap_polls += 1
                        return dict(base, capture={"status": "queued", "requestId": 10})
                    if self._cap_polls == 1:
                        self._cap_polls += 1
                        return dict(base, capture={
                            "status": "completed", "requestId": 10,
                            "basename": "capture-seq-conn-10.png",
                            "path": r"C:\out\capture-seq-conn-10.png",
                            "width": 8, "height": 8, "error": "",
                        })
                    if self._kb_polls < 1:
                        self._kb_polls += 1
                        return dict(base, keyboard={"status": "active", "requestId": 20, "remainingUpdates": 1})
                    return dict(base, keyboard={"status": "completed", "requestId": 20, "remainingUpdates": 0})
                if verb in ("hello", "perf", "list_objects"):
                    return dict(base, objects=[], frames=1)
                return base
            def close(self):
                self.handle = None
        c = FakeSequentialClient()
        self.assertTrue(c.request("hello", 100)["ok"])
        self.assertIsNotNone(c.handle)
        tf = c.set_transform("Box", 1, position=[1, 2, 3], completion_timeout=2.0)
        self.assertEqual(tf["worldOp"]["status"], "completed")
        self.assertIsNotNone(c.handle)
        self.assertTrue(c.request("perf", 200, {"frames": 1})["ok"])
        cap = c.capture(10, completion_timeout=2.0)
        self.assertEqual(cap["capture"]["status"], "completed")
        self.assertEqual(Client.capture_output_path(cap["capture"]), r"C:\out\capture-seq-conn-10.png")
        self.assertIsNotNone(c.handle)
        kb = c.keyboard(["W"], 1, 20, completion_timeout=2.0)
        self.assertEqual(kb["keyboard"]["status"], "completed")
        self.assertIsNotNone(c.handle)
        self.assertTrue(c.request("list_objects", 300)["ok"])
        self.assertIsNotNone(c.handle)
        verbs = [v for v, _ in c.calls]
        self.assertIn("set_transform", verbs)
        self.assertIn("capture", verbs)
        self.assertIn("keyboard", verbs)
        self.assertGreaterEqual(len(c.calls), 8)

    def test_mouse_client_validation(self):
        from runtime_test_client import build_mouse_payload
        c=Client.__new__(Client)
        for bad_dx in (-601, 601, "0", 1.5, True):
            with self.subTest(bad_dx=bad_dx):
                with self.assertRaises(ValueError): c.mouse(bad_dx, 0, [], 5, 1)
        for bad_dy in (-601, 601, "0", 1.5, True):
            with self.subTest(bad_dy=bad_dy):
                with self.assertRaises(ValueError): c.mouse(0, bad_dy, [], 5, 1)
        for bad_buttons in (["X"], ["LEFT", "LEFT"], ["LEFT", "RIGHT", "MIDDLE", "LEFT"], "LEFT", [1]):
            with self.subTest(bad_buttons=bad_buttons):
                with self.assertRaises(ValueError): c.mouse(0, 0, bad_buttons, 5, 1)
        for bad_wheel in (-121, 121, "0", 1.5):
            with self.subTest(bad_wheel=bad_wheel):
                with self.assertRaises(ValueError): c.mouse(0, 0, [], 5, 1, wheel=bad_wheel)
        for bad_updates in (0, 121, True, 1.5, "5"):
            with self.subTest(bad_updates=bad_updates):
                with self.assertRaises(ValueError): c.mouse(0, 0, [], bad_updates, 1)
        with self.assertRaises(ValueError): build_mouse_payload(0, 0, [], 5, x=10)
        with self.assertRaises(ValueError): build_mouse_payload(0, 0, [], 5, y=10)
        for bad_xy in ((-1, 0), (0, -1), (1.5, 0), (True, 0), ("0", 0), (0, True)):
            with self.subTest(bad_xy=bad_xy):
                with self.assertRaises(ValueError):
                    build_mouse_payload(0, 0, [], 5, x=bad_xy[0], y=bad_xy[1])
        payload=build_mouse_payload(1, -2, ["LEFT"], 3, wheel=4, x=100, y=200)
        self.assertEqual(payload, {"dx":1,"dy":-2,"buttons":["LEFT"],"wheel":4,"updates":3,"x":100,"y":200})
        self.assertNotIn("x", build_mouse_payload(0, 0, [], 1))
        with self.assertRaises(ValueError):
            build_mouse_payload(0, 0, ["LEFT"], 5, duration_ms=100)
        with self.assertRaises(ValueError):
            build_mouse_payload(0, 0, ["LEFT"])
        for bad_duration in (0, 30001, True, 1.5, "100", -1):
            with self.subTest(bad_duration=bad_duration):
                with self.assertRaises(ValueError):
                    build_mouse_payload(0, 0, ["LEFT"], duration_ms=bad_duration)
        duration_payload=build_mouse_payload(0, 0, ["LEFT"], duration_ms=3500, x=189, y=527)
        self.assertEqual(duration_payload, {
            "dx":0,"dy":0,"buttons":["LEFT"],"wheel":0,"durationMs":3500,"x":189,"y":527})
        self.assertNotIn("updates", duration_payload)

    def test_editor_play_client_validation(self):
        from runtime_test_client import (
            build_play_enter_payload, build_load_scene_payload, build_set_setting_payload, SETTING_KEYS)
        self.assertEqual(build_play_enter_payload(), {})
        self.assertEqual(build_play_enter_payload("viewport"), {"mode": "viewport"})
        self.assertEqual(build_play_enter_payload("fullscreen"), {"mode": "fullscreen"})
        with self.assertRaises(ValueError):
            build_play_enter_payload("bogus")
        self.assertEqual(build_load_scene_payload("rom/scene/a.json"), {"scene": "rom/scene/a.json"})
        for bad in ("scene.json", "rom/../x.json", "rom/scene/a.txt", "../rom/a.json", ""):
            with self.subTest(bad_scene=bad):
                with self.assertRaises(ValueError):
                    build_load_scene_payload(bad)
        for key in SETTING_KEYS:
            self.assertEqual(build_set_setting_payload(key, True), {"key": key, "value": True})
            self.assertEqual(build_set_setting_payload(key, False), {"key": key, "value": False})
        with self.assertRaises(ValueError):
            build_set_setting_payload("fogEnabled", True)
        with self.assertRaises(ValueError):
            build_set_setting_payload("frustumCullingEnabled", 1)
        with self.assertRaises(ValueError):
            build_set_setting_payload("frustumCullingEnabled", "true")

    def test_events_client_validation(self):
        from runtime_test_client import validate_events_reply
        c=Client.__new__(Client)
        c.pid=1
        c.run_id="events-test"
        c.timeout=2
        c.handle=None
        for bad in (0, True, 1.5, "1", -1):
            with self.subTest(bad=bad):
                with self.assertRaises(ValueError):
                    c.events(bad)
        # Shared body validator (Client.events raises RuntimeError on invalid body after identity).
        self.assertFalse(validate_events_reply({
            "ok": True, "protocolVersion": 1,
            "events": [{"sequence": 999, "type": "checkpoint_reached",
                        "checkpoint": "Checkpoint_North", "actor": "Player", "score": -1}],
            "latestSequence": 0, "dropped": 0,
        })[0])
        self.assertTrue(validate_events_reply({
            "ok": True, "protocolVersion": 1, "events": [], "latestSequence": 0, "dropped": 0,
        })[0])
        class FakeClient(Client):
            def request(self, verb, request_id, payload=None, deadline=None):
                if getattr(self, "_mode") == "identity":
                    return {"ok": True, "protocolVersion": 1, "runId": "other", "requestId": request_id,
                            "events": [], "latestSequence": 0, "dropped": 0}
                return {"ok": True, "protocolVersion": 1, "runId": self.run_id, "requestId": request_id,
                        "events": [{"sequence": 999, "type": "checkpoint_reached",
                                    "checkpoint": "Checkpoint_North", "actor": "Player", "score": -1}],
                        "latestSequence": 0, "dropped": 0}
        fc=FakeClient.__new__(FakeClient)
        fc.pid=1; fc.run_id="events-test"; fc.timeout=2; fc.handle=object()
        fc._mode="identity"
        with self.assertRaises(RuntimeError):
            fc.events(1)
        fc._mode="body"
        with self.assertRaises(RuntimeError):
            fc.events(1)

    def test_controller_client_validation(self):
        c=Client.__new__(Client)
        for bad_lx in (-1.01, 1.01, float("nan"), float("inf"), True, False, "0", None):
            with self.subTest(bad_lx=bad_lx):
                with self.assertRaises(ValueError): c.controller(bad_lx, 0.0, 5, 1)
        for bad_ly in (-1.01, 1.01, float("nan"), float("-inf"), True, "1", None):
            with self.subTest(bad_ly=bad_ly):
                with self.assertRaises(ValueError): c.controller(0.0, bad_ly, 5, 1)
        for bad_updates in (0, 121, True, 1.5, "5"):
            with self.subTest(bad_updates=bad_updates):
                with self.assertRaises(ValueError): c.controller(0.0, 1.0, bad_updates, 1)
        for bad_rid in (0, True, 1.5, 2147483647):
            with self.subTest(bad_rid=bad_rid):
                with self.assertRaises(ValueError): c.controller(0.0, 1.0, 5, bad_rid)
        for bad_timeout in (0, 10.1, float("nan"), float("inf"), -1, True, False, "7", None):
            with self.subTest(bad_timeout=bad_timeout):
                with self.assertRaises(ValueError): c.controller(0.0, 1.0, 5, 1, completion_timeout=bad_timeout)

    def test_real_mouse_completion(self):
        run=uuid.uuid4().hex
        env=dict(os.environ, IVY_RUNTIME_TEST_BRIDGE="1", IVY_RUNTIME_TEST_RUN_ID=run)
        p=subprocess.Popen([os.environ["IVY_BRIDGE_HARNESS"],"--serve"],env=env,stdout=subprocess.PIPE,text=True)
        try:
            with Client(p.pid,run,timeout=2) as c:
                r=c.mouse(15,-25,["LEFT"],5,1,wheel=10)
                self.assertTrue(r["ok"])
                self.assertEqual(r["mouse"]["status"],"completed")
                self.assertEqual(r["mouse"]["requestId"],1)
                self.assertEqual(r["mouse"]["remainingUpdates"],0)
                self.assertTrue(c.request("quit",r["requestId"]+1)["ok"])
            self.assertEqual(p.wait(timeout=3),0)
        finally:
            if p.poll() is None: p.kill();p.wait(timeout=3)
            p.stdout.close()

    def test_real_controller_completion(self):
        run=uuid.uuid4().hex
        env=dict(os.environ, IVY_RUNTIME_TEST_BRIDGE="1", IVY_RUNTIME_TEST_RUN_ID=run)
        p=subprocess.Popen([os.environ["IVY_BRIDGE_HARNESS"],"--serve"],env=env,stdout=subprocess.PIPE,text=True)
        try:
            with Client(p.pid,run,timeout=2) as c:
                r=c.controller(0.0,1.0,5,1)
                self.assertTrue(r["ok"])
                self.assertEqual(r["controller"]["status"],"completed")
                self.assertEqual(r["controller"]["requestId"],1)
                self.assertEqual(r["controller"]["remainingUpdates"],0)
                self.assertTrue(c.request("quit",r["requestId"]+1)["ok"])
            self.assertEqual(p.wait(timeout=3),0)
        finally:
            if p.poll() is None: p.kill();p.wait(timeout=3)
            p.stdout.close()

    def test_real_controller_cli_completion(self):
        run=uuid.uuid4().hex
        env=dict(os.environ, IVY_RUNTIME_TEST_BRIDGE="1", IVY_RUNTIME_TEST_RUN_ID=run)
        p=subprocess.Popen([os.environ["IVY_BRIDGE_HARNESS"],"--serve"],env=env,stdout=subprocess.PIPE,text=True)
        try:
            cli=subprocess.run([sys.executable,os.path.join(os.path.dirname(__file__),"runtime_test_client.py"),
                "--pid",str(p.pid),"--run-id",run,"--request-id","1","controller","--lx","0","--ly","1","--updates","1"],
                capture_output=True,text=True,timeout=10)
            self.assertEqual(cli.returncode,0,cli.stderr)
            r=json.loads(cli.stdout)
            self.assertEqual(r["controller"]["status"],"completed")
            with Client(p.pid,run) as c: self.assertTrue(c.request("quit",r["requestId"]+1)["ok"])
            self.assertEqual(p.wait(timeout=3),0)
        finally:
            if p.poll() is None: p.kill();p.wait(timeout=3)
            p.stdout.close()

if __name__ == "__main__":
    unittest.main()
