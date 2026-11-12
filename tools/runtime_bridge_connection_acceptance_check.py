"""Offline unit checks for connection-acceptance constants and helpers."""
import os
import unittest
import importlib.util
from pathlib import Path

HERE = Path(__file__).resolve().parent


def _load(name):
    path = HERE / name
    spec = importlib.util.spec_from_file_location(name.replace(".py", ""), path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


class ConnectionAcceptanceChecks(unittest.TestCase):
    def test_min_ops_and_idle_gap(self):
        mod = _load("runtime_bridge_connection_acceptance.py")
        self.assertGreaterEqual(mod.MIN_OPS, 10)
        self.assertGreater(mod.IDLE_GAP_SEC, 2.0)

    def test_strict_link_has_no_reconnect_api(self):
        mod = _load("runtime_bridge_connection_acceptance.py")
        self.assertTrue(hasattr(mod, "StrictLink"))
        self.assertFalse(hasattr(mod.StrictLink, "recon"))
        self.assertFalse(hasattr(mod.StrictLink, "reconnect"))


if __name__ == "__main__":
    unittest.main()
