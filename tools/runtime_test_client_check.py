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
                self.assertEqual(c.request("capture",2)["error"],"unknown_verb")
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

if __name__ == "__main__":
    unittest.main()
