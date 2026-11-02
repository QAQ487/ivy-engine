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

    def test_mouse_client_validation(self):
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
