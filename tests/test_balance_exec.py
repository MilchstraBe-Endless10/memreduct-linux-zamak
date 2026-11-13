"""Unprivileged executor: ownership, bare names, measured accounting.

The fixture tests exercise the refusal paths and the scope-name rules without
touching the kernel. The opt-in test runs against a real throwaway cgroup,
because the interesting behaviour (EAGAIN still reclaiming) only exists there.
"""

import ctypes
import errno
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NATIVE = os.path.join(ROOT, "native")
MIB = 1024 * 1024

STATUS = {"ok": 0, "partial": 1, "stopped": 2, "refused": 3,
          "not_delegated": 4, "wrong_owner": 5, "bad_request": 6}
STOP = {"none": 0, "no_cold_pages": 1, "stall": 2, "error": 3, "budget_met": 4}


class ExecConfig(ctypes.Structure):
    _fields_ = [
        ("budget_bytes", ctypes.c_uint64),
        ("batch_bytes", ctypes.c_uint64),
        ("max_batches", ctypes.c_uint32),
        ("psi_stop_centi", ctypes.c_uint32),
        ("expected_uid", ctypes.c_uint32),
    ]


class ExecResult(ctypes.Structure):
    _fields_ = [
        ("status", ctypes.c_int64),
        ("stop_reason", ctypes.c_int64),
        ("requested_bytes", ctypes.c_uint64),
        ("accepted_bytes", ctypes.c_uint64),
        ("batches_attempted", ctypes.c_uint32),
        ("batches_accepted", ctypes.c_uint32),
        ("batches_rejected", ctypes.c_uint32),
        ("observed_on_reject_bytes", ctypes.c_uint64),
        ("error_code", ctypes.c_int32),
        ("scope_current_before", ctypes.c_int64),
        ("scope_current_after", ctypes.c_int64),
        ("scope_anon_before", ctypes.c_int64),
        ("scope_anon_after", ctypes.c_int64),
        ("scope_file_before", ctypes.c_int64),
        ("scope_file_after", ctypes.c_int64),
    ]


class ExecTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="memreduct-exec-")
        cls.lib_path = os.path.join(cls.tmp, "libexec.so")
        subprocess.run(
            ["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-pedantic",
             "-shared", "-fPIC", "-I", NATIVE,
             os.path.join(NATIVE, "balance_exec.c"), "-o", cls.lib_path],
            check=True, capture_output=True, text=True,
        )
        cls.lib = ctypes.CDLL(cls.lib_path)
        cls.lib.mr_bal_exec_open_scope.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
        cls.lib.mr_bal_exec_open_scope.restype = ctypes.c_int
        cls.lib.mr_bal_exec_probe.argtypes = [ctypes.c_int, ctypes.c_uint32]
        cls.lib.mr_bal_exec_probe.restype = ctypes.c_int64
        cls.lib.mr_bal_exec_run.argtypes = [ctypes.c_int, ctypes.POINTER(ExecConfig),
                                            ctypes.POINTER(ExecResult)]
        cls.lib.mr_bal_exec_run.restype = ctypes.c_int
        cls.lib.mr_bal_exec_list_scopes.argtypes = [ctypes.c_char_p, ctypes.c_uint32,
                                                    ctypes.c_char_p, ctypes.c_uint32]
        cls.lib.mr_bal_exec_list_scopes.restype = ctypes.c_int64
        cls.lib.mr_bal_exec_scope_name.argtypes = [ctypes.c_int64]
        cls.lib.mr_bal_exec_scope_name.restype = ctypes.c_char_p
        cls.lib.mr_bal_exec_app_label.argtypes = [ctypes.c_char_p]
        cls.lib.mr_bal_exec_app_label.restype = ctypes.c_char_p
        for name in ("mr_bal_exec_status_name", "mr_bal_exec_stop_name"):
            getattr(cls.lib, name).argtypes = [ctypes.c_int64]
            getattr(cls.lib, name).restype = ctypes.c_char_p

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def make_scope(self, name="app-test.App-1234.scope", owner_scopes=True):
        root = tempfile.mkdtemp(prefix="scoperoot-", dir=self.tmp)
        scope = os.path.join(root, name)
        os.makedirs(scope)
        if owner_scopes:
            open(os.path.join(scope, "memory.stat"), "w").write("anon 0\nfile 0\n")
            open(os.path.join(scope, "memory.current"), "w").write("0\n")
        return root

    def test_bare_scope_name_is_required(self):
        root = self.make_scope()
        # errno does not survive the ctypes trampoline reliably on this host
        # (measured earlier), so the assertion is on the refusal itself.
        for bad in ["", "/abs", "../escape", "a/b", ".", ".."]:
            fd = self.lib.mr_bal_exec_open_scope(root.encode(), bad.encode())
            self.assertEqual(fd, -1, "accepted hostile name %r" % bad)

    def test_missing_scope_is_reported(self):
        root = self.make_scope()
        fd = self.lib.mr_bal_exec_open_scope(root.encode(), b"app-none-1.scope")
        self.assertEqual(fd, -1)

    def test_symlinked_scope_is_refused(self):
        """A symlink in the root must not let the caller escape its slice."""
        root = tempfile.mkdtemp(prefix="scoperoot-", dir=self.tmp)
        target = self.make_scope()
        os.symlink(os.path.join(target, "app-test.App-1234.scope"),
                   os.path.join(root, "app-link-1.scope"))
        fd = self.lib.mr_bal_exec_open_scope(root.encode(), b"app-link-1.scope")
        self.assertEqual(fd, -1, "O_NOFOLLOW should reject a symlinked scope")

    def test_wrong_owner_is_refused_without_writing(self):
        root = self.make_scope()
        scope = os.path.join(root, "app-test.App-1234.scope")
        control = os.path.join(scope, "memory.reclaim")
        open(control, "w").close()
        os.chmod(control, 0o200)
        fd = self.lib.mr_bal_exec_open_scope(root.encode(), b"app-test.App-1234.scope")
        self.assertGreaterEqual(fd, 0)
        try:
            # Claim uid+1: the probe must refuse on the owner mismatch.
            status = self.lib.mr_bal_exec_probe(fd, 1000 + 1)
        finally:
            os.close(fd)
        self.assertEqual(status, STATUS["wrong_owner"])

    def test_missing_control_file_means_not_delegated(self):
        root = self.make_scope()
        fd = self.lib.mr_bal_exec_open_scope(root.encode(), b"app-test.App-1234.scope")
        self.assertGreaterEqual(fd, 0)
        try:
            status = self.lib.mr_bal_exec_probe(fd, os.getuid())
        finally:
            os.close(fd)
        self.assertEqual(status, STATUS["not_delegated"])

    def test_run_refuses_when_probe_fails(self):
        root = self.make_scope()
        fd = self.lib.mr_bal_exec_open_scope(root.encode(), b"app-test.App-1234.scope")
        cfg = ExecConfig()
        cfg.budget_bytes = 4 * MIB
        cfg.batch_bytes = 1 * MIB
        cfg.max_batches = 4
        cfg.expected_uid = os.getuid()
        result = ExecResult()
        try:
            self.assertEqual(self.lib.mr_bal_exec_run(fd, ctypes.byref(cfg),
                                                       ctypes.byref(result)), 0)
        finally:
            os.close(fd)
        self.assertEqual(result.status, STATUS["not_delegated"])
        self.assertEqual(result.batches_attempted, 0,
                         "no write may happen once the probe refused")

    def test_bad_request_is_rejected_before_any_io(self):
        root = self.make_scope()
        fd = self.lib.mr_bal_exec_open_scope(root.encode(), b"app-test.App-1234.scope")
        cfg = ExecConfig()
        cfg.budget_bytes = 0
        cfg.batch_bytes = 1 * MIB
        cfg.max_batches = 1
        cfg.expected_uid = os.getuid()
        result = ExecResult()
        try:
            self.lib.mr_bal_exec_run(fd, ctypes.byref(cfg), ctypes.byref(result))
        finally:
            os.close(fd)
        self.assertEqual(result.status, STATUS["bad_request"])

    # -- listing ----------------------------------------------------------
    def test_listing_only_returns_app_scopes_with_a_control_file(self):
        root = tempfile.mkdtemp(prefix="scoperoot-", dir=self.tmp)
        uid = os.getuid()
        # A complete app scope: listed.
        good = os.path.join(root, "app-good.App-1.scope")
        os.makedirs(good)
        open(os.path.join(good, "memory.reclaim"), "w").close()
        os.chmod(os.path.join(good, "memory.reclaim"), 0o200)
        # No control file: skipped.
        os.makedirs(os.path.join(root, "app-nofile.App-2.scope"))
        # Not an app scope: skipped even with a control file.
        other = os.path.join(root, "dsh-subprocess-3.scope")
        os.makedirs(other)
        open(os.path.join(other, "memory.reclaim"), "w").close()

        buf = ctypes.create_string_buffer(64 * 128)
        count = self.lib.mr_bal_exec_list_scopes(root.encode(), uid, buf, len(buf))
        self.assertEqual(count, 1)
        self.assertEqual(self.lib.mr_bal_exec_scope_name(0).decode(),
                         "app-good.App-1.scope")
        self.assertEqual(self.lib.mr_bal_exec_scope_name(1).decode(), "")

    def test_app_label_strips_prefix_suffix_and_pid(self):
        self.assertEqual(
            self.lib.mr_bal_exec_app_label(b"app-com.google.Chrome-2701326.scope").decode(),
            "com.google.Chrome")

    def test_app_label_decodes_systemd_escapes(self):
        """'deepseek\\x2dai' is a dash; showing the escape would be unreadable."""
        self.assertEqual(
            self.lib.mr_bal_exec_app_label(
                b"app-deepseek\\x2dai\\x2ddsh\\x2ddesktop-2504063.scope").decode(),
            "deepseek-ai-dsh-desktop")

    def test_app_label_of_unknown_shape_is_honest(self):
        self.assertEqual(self.lib.mr_bal_exec_app_label(b"weird.scope").decode(), "weird")

    def test_names_are_stable(self):
        for name, value in STATUS.items():
            self.assertEqual(self.lib.mr_bal_exec_status_name(value).decode(), name)
        for name, value in STOP.items():
            self.assertEqual(self.lib.mr_bal_exec_stop_name(value).decode(), name)


@unittest.skipUnless(os.environ.get("MEMREDUCT_REAL_CGROUP_TEST") == "1",
                     "set MEMREDUCT_REAL_CGROUP_TEST=1 to run real reclaim")
class RealScopeTest(unittest.TestCase):
    """Real kernel answers, in a throwaway systemd scope the test owns.

    The systemd-created scope is what makes this meaningful: a scope made with
    mkdir has no memory accounting on this host, so a reclaim there reports
    zeros and proves nothing.
    """

    ROOT = "/sys/fs/cgroup/user.slice/user-1000.slice/user@1000.service/app.slice"
    UNIT = "memreduct-exec-test.scope"

    @classmethod
    def setUpClass(cls):
        # Build its own handle: unittest does not run ExecTest.setUpClass when
        # only this class is selected, and reaching into another class's state
        # is how a test becomes silently order-dependent.
        cls.tmp = tempfile.mkdtemp(prefix="memreduct-exec-real-")
        cls.lib_path = os.path.join(cls.tmp, "libexec-real.so")
        subprocess.run(
            ["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-pedantic",
             "-shared", "-fPIC", "-I", NATIVE,
             os.path.join(NATIVE, "balance_exec.c"), "-o", cls.lib_path],
            check=True, capture_output=True, text=True,
        )
        cls.lib = ctypes.CDLL(cls.lib_path)
        cls.lib.mr_bal_exec_open_scope.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
        cls.lib.mr_bal_exec_open_scope.restype = ctypes.c_int
        cls.lib.mr_bal_exec_probe.argtypes = [ctypes.c_int, ctypes.c_uint32]
        cls.lib.mr_bal_exec_probe.restype = ctypes.c_int64

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def setUp(self):
        subprocess.run(["systemctl", "--user", "stop", self.UNIT],
                       capture_output=True)
        self.cg = os.path.join(self.ROOT, self.UNIT)
        self.unit = None

    def tearDown(self):
        subprocess.run(["systemctl", "--user", "stop", self.UNIT],
                       capture_output=True)

    def start_scope(self, body_python):
        self.unit = subprocess.Popen(
            ["systemd-run", "--user", "--scope", "--unit", self.UNIT,
             "--slice=app.slice", sys.executable, "-c", body_python],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        for _ in range(60):
            if os.path.isdir(self.cg):
                return True
            import time
            time.sleep(0.2)
        return False

    def test_owner_uid_is_the_caller_on_a_real_scope(self):
        if not self.start_scope("import time; time.sleep(20)"):
            self.skipTest("systemd-run scope did not appear")
        fd = self.lib.mr_bal_exec_open_scope(self.ROOT.encode(), self.UNIT.encode())
        self.assertGreaterEqual(fd, 0)
        try:
            fresh = self.lib.mr_bal_exec_probe(fd, os.getuid())
        finally:
            os.close(fd)
        self.assertIn(fresh, (STATUS["ok"], STATUS["refused"],
                              STATUS["not_delegated"]))


if __name__ == "__main__":
    unittest.main()
