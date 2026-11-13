"""Scalar bridge: the surface the Zamak controller actually calls.

Exercises the real scope root derivation and listing against this machine.
"""

import ctypes
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NATIVE = os.path.join(ROOT, "native")

class BridgeTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="memreduct-bridge-")
        cls.lib_path = os.path.join(cls.tmp, "libbridge.so")
        subprocess.run(
            ["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-pedantic",
             "-shared", "-fPIC", "-I", NATIVE,
             os.path.join(NATIVE, "balance_bridge.c"),
             os.path.join(NATIVE, "balance_gui.c"),
             os.path.join(NATIVE, "balance_exec.c"),
             os.path.join(NATIVE, "balance_snapshot.c"),
             os.path.join(NATIVE, "balance_plan.c"),
             "-o", cls.lib_path],
            check=True, capture_output=True, text=True,
        )
        cls.lib = ctypes.CDLL(cls.lib_path)
        cls.lib.mr_bal_bridge_scope_root.restype = ctypes.c_char_p
        cls.lib.mr_bal_bridge_scan_scopes.restype = ctypes.c_int64
        cls.lib.mr_bal_bridge_scope_name.argtypes = [ctypes.c_int64]
        cls.lib.mr_bal_bridge_scope_name.restype = ctypes.c_char_p
        cls.lib.mr_bal_bridge_scope_label.argtypes = [ctypes.c_int64]
        cls.lib.mr_bal_bridge_scope_label.restype = ctypes.c_char_p
        cls.lib.mr_bal_bridge_run.argtypes = [ctypes.c_int64, ctypes.c_uint64,
                                              ctypes.c_uint64, ctypes.c_uint32,
                                              ctypes.c_uint32]
        cls.lib.mr_bal_bridge_run.restype = ctypes.c_int64

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def test_scope_root_is_absolute_and_under_the_cgroup_mount(self):
        root = self.lib.mr_bal_bridge_scope_root().decode()
        # /proc/self/cgroup reports a mount-relative path; the filesystem calls
        # need it rooted, and returning the relative form silently listed zero
        # scopes while looking correct.
        self.assertTrue(root.startswith("/sys/fs/cgroup/"), root)
        self.assertTrue(root.endswith("/app.slice"), root)
        self.assertTrue(os.path.isdir(root), root)

    def test_scan_reports_only_app_scopes(self):
        count = self.lib.mr_bal_bridge_scan_scopes()
        self.assertGreaterEqual(count, 0)
        for i in range(count):
            name = self.lib.mr_bal_bridge_scope_name(i).decode()
            self.assertTrue(name.startswith("app-"), name)
            self.assertTrue(name.endswith(".scope"), name)
            self.assertNotIn("\\x2d", self.lib.mr_bal_bridge_scope_label(i).decode(),
                             "labels must be readable, not systemd escapes")

    def test_out_of_range_index_is_empty(self):
        self.assertEqual(self.lib.mr_bal_bridge_scope_name(-1).decode(), "")
        self.assertEqual(self.lib.mr_bal_bridge_scope_label(10_000).decode(), "")

    def test_run_refuses_out_of_range_index_without_touching_anything(self):
        self.lib.mr_bal_bridge_scan_scopes()
        self.assertEqual(self.lib.mr_bal_bridge_run(-1, 1024, 1024, 1, 0), 0)
        self.assertEqual(self.lib.mr_bal_bridge_run(10_000, 1024, 1024, 1, 0), 0)

    def test_scope_file_bytes_is_readable_or_minus_one(self):
        """The scope's own file counter is the only honest budget ceiling."""
        count = self.lib.mr_bal_bridge_scan_scopes()
        if count == 0:
            self.skipTest("no app scopes on this host")
        self.lib.mr_bal_bridge_scope_file_bytes.argtypes = [ctypes.c_int64]
        self.lib.mr_bal_bridge_scope_file_bytes.restype = ctypes.c_int64
        value = self.lib.mr_bal_bridge_scope_file_bytes(0)
        self.assertGreaterEqual(value, -1)
        self.assertEqual(self.lib.mr_bal_bridge_scope_file_bytes(-1), -1)
        self.assertEqual(self.lib.mr_bal_bridge_scope_file_bytes(10_000), -1)

    def test_scope_view_reports_the_three_counters(self):
        """The picker shows file, anon and current; summing file+anon would
        describe a quantity no action here can move."""
        count = self.lib.mr_bal_bridge_scan_scopes()
        if count == 0:
            self.skipTest("no app scopes on this host")
        self.lib.mr_bal_bridge_scope_refresh.argtypes = [ctypes.c_int64]
        self.lib.mr_bal_bridge_scope_refresh.restype = ctypes.c_int64
        for name in ("mr_bal_bridge_scope_eligible",
                     "mr_bal_bridge_scope_anon_bytes",
                     "mr_bal_bridge_scope_current_bytes"):
            getattr(self.lib, name).argtypes = []
            getattr(self.lib, name).restype = ctypes.c_int64
        self.assertEqual(self.lib.mr_bal_bridge_scope_refresh(0), 1)
        anon = self.lib.mr_bal_bridge_scope_anon_bytes()
        cur = self.lib.mr_bal_bridge_scope_current_bytes()
        self.assertGreaterEqual(anon, -1)
        self.assertGreaterEqual(cur, -1)
        if cur >= 0 and anon >= 0:
            self.assertLessEqual(anon, cur, "anon cannot exceed the scope total")
        # An out-of-range index leaves the view invalid rather than stale.
        self.assertEqual(self.lib.mr_bal_bridge_scope_refresh(-1), 0)
        self.assertEqual(self.lib.mr_bal_bridge_scope_eligible(), 0)

    def test_run_with_zero_budget_refuses(self):
        count = self.lib.mr_bal_bridge_scan_scopes()
        if count == 0:
            self.skipTest("no app scopes on this host")
        self.assertEqual(self.lib.mr_bal_bridge_run(0, 0, 0, 1, 0), 0)


if __name__ == "__main__":
    unittest.main()
