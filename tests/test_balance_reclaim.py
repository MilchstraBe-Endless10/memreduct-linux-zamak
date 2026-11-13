"""File-first reclaim backend: swappiness=0 only, sticky stop, honest counting.

The fixture tests drive the real C against a fake cgroup directory. The
optional integration test drives it against a throwaway real cgroup and is
skipped unless explicitly enabled, because it does real reclaim.
"""

import ctypes
import errno
import sys
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NATIVE = os.path.join(ROOT, "native")
MIB = 1024 * 1024

CAPS = {"unknown": 0, "file_only": 1, "no_swappiness_parameter": 2, "unavailable": 3}
STATUS = {"ok": 0, "partial": 1, "stopped": 2, "unsupported": 3,
          "no_scope": 4, "bad_request": 5}
STOP = {"none": 0, "no_cold_pages": 1, "stall_rising": 2, "unsupported": 3, "error": 4}


class RunConfig(ctypes.Structure):
    _fields_ = [
        ("budget_bytes", ctypes.c_uint64),
        ("batch_bytes", ctypes.c_uint64),
        ("max_batches", ctypes.c_uint32),
        ("psi_stop_centi", ctypes.c_uint32),
        ("stop_on_budget", ctypes.c_int32),
    ]


class RunResult(ctypes.Structure):
    _fields_ = [
        ("status", ctypes.c_int64),
        ("caps", ctypes.c_int64),
        ("requested_bytes", ctypes.c_uint64),
        ("accepted_bytes", ctypes.c_uint64),
        ("batches_attempted", ctypes.c_uint32),
        ("batches_accepted", ctypes.c_uint32),
        ("batches_rejected", ctypes.c_uint32),
        ("error_code", ctypes.c_int32),
        ("stopped_early", ctypes.c_int32),
        ("stop_reason", ctypes.c_int64),
    ]


class ReclaimTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="memreduct-reclaim-")
        cls.lib_path = os.path.join(cls.tmp, "libreclaim.so")
        subprocess.run(
            ["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-pedantic",
             "-shared", "-fPIC", "-DMEMREDUCT_TESTING", "-I", NATIVE,
             os.path.join(NATIVE, "balance_reclaim.c"), "-o", cls.lib_path],
            check=True, capture_output=True, text=True,
        )
        cls.lib = ctypes.CDLL(cls.lib_path)
        cls.lib.mr_bal_caps_probe.argtypes = [ctypes.c_int, ctypes.POINTER(ctypes.c_int64)]
        cls.lib.mr_bal_caps_probe.restype = ctypes.c_int64
        cls.lib.mr_bal_run_config_for_budget.argtypes = [ctypes.c_uint64,
                                                         ctypes.POINTER(RunConfig)]
        cls.lib.mr_bal_run_config_for_budget.restype = None
        cls.lib.mr_bal_reclaim_run.argtypes = [ctypes.c_int, ctypes.POINTER(RunConfig),
                                               ctypes.POINTER(RunResult)]
        cls.lib.mr_bal_reclaim_run.restype = ctypes.c_int
        cls.lib.mr_bal_set_write_hook.argtypes = [ctypes.c_void_p]
        cls.lib.mr_bal_set_write_hook.restype = None
        cls.lib.mr_bal_test_set_write_errno.argtypes = [ctypes.c_int]
        cls.lib.mr_bal_test_set_write_errno.restype = None
        for name in ("mr_bal_caps_name", "mr_bal_run_status_name", "mr_bal_stop_name"):
            getattr(cls.lib, name).argtypes = [ctypes.c_int64]
            getattr(cls.lib, name).restype = ctypes.c_char_p

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def setUp(self):
        self.scope = tempfile.mkdtemp(prefix="scope-", dir=self.tmp)
        self.written = []
        self.write_control_fd = None

    def tearDown(self):
        if self.write_control_fd is not None:
            os.close(self.write_control_fd)
        shutil.rmtree(self.scope, ignore_errors=True)

    # -- fake cgroup helpers ---------------------------------------------
    def install_scope(self, file_bytes=4 * MIB):
        """A directory that looks enough like a cgroup for openat() to work."""
        with open(os.path.join(self.scope, "memory.stat"), "w", encoding="ascii") as fh:
            fh.write("anon 0\nfile %d\n" % file_bytes)
        self.control = os.path.join(self.scope, "memory.reclaim")
        open(self.control, "w").close()

    def run_harness(self, behaviour, budget, batch=None, max_batches=32, psi_stop=0):
        """Drive the real C through a C harness so errno crosses correctly."""
        if not hasattr(self.__class__, "harness"):
            self.__class__.harness = os.path.join(self.tmp, "harness")
            subprocess.run(
                ["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-pedantic",
                 "-DMEMREDUCT_TESTING", "-I", NATIVE,
                 os.path.join(ROOT, "tests", "balance_reclaim_harness.c"),
                 os.path.join(NATIVE, "balance_reclaim.c"),
                 "-o", self.__class__.harness],
                check=True, capture_output=True, text=True,
            )
        batch = budget if batch is None else batch
        proc = subprocess.run(
            [self.__class__.harness, self.scope, behaviour, str(budget), str(batch),
             str(max_batches), str(psi_stop)],
            capture_output=True, text=True,
        )
        self.assertEqual(proc.returncode, 0, proc.stderr)
        out = {}
        for line in proc.stdout.splitlines():
            key, _, value = line.partition("=")
            out[key] = value
        return out

    # -- behaviour --------------------------------------------------------
    def test_every_write_carries_swappiness_zero(self):
        """The bare byte count was measured to reclaim anonymous pages on this
        kernel. No code path may emit it, not even after a rejection."""
        self.install_scope()
        out = self.run_harness("accept", budget=96 * MIB, batch=32 * MIB, max_batches=3)
        self.assertEqual(out["status"], "ok")
        self.assertEqual(out["write_calls"], "3")
        self.assertRegex(out["last_write"], r"^\d+ swappiness=0$")

    def test_parameter_order_is_bytes_first(self):
        """'swappiness=0 128M' is EINVAL on this kernel; the order is load-bearing."""
        self.install_scope()
        out = self.run_harness("accept", budget=32 * MIB)
        self.assertRegex(out["last_write"], r"^33554432 swappiness=0$")

    def test_eagain_is_no_cold_pages_and_stops_the_loop(self):
        self.install_scope()
        out = self.run_harness("eagain", budget=256 * MIB, batch=32 * MIB, max_batches=8)
        self.assertEqual(out["status"], "stopped")
        self.assertEqual(out["stop"], "no_cold_pages")
        self.assertEqual(out["attempted"], "1", "EAGAIN must stop the loop, not be retried")
        self.assertEqual(out["accepted"], "0")
        self.assertEqual(out["error_code"], str(errno.EAGAIN))

    def test_eagain_never_falls_back_to_a_bare_byte_count(self):
        self.install_scope()
        out = self.run_harness("eagain", budget=64 * MIB, batch=32 * MIB, max_batches=4)
        self.assertIn("swappiness=0", out["last_write"])

    def test_partial_run_reports_exactly_what_the_kernel_took(self):
        self.install_scope()
        out = self.run_harness("accept-2-then-eagain", budget=256 * MIB,
                               batch=32 * MIB, max_batches=8)
        self.assertEqual(out["status"], "partial")
        self.assertEqual(out["accepted"], str(64 * MIB))
        self.assertEqual(out["requested"], str(96 * MIB),
                         "the rejected third batch was requested but not accepted")
        self.assertEqual(out["accepted_batches"], "2")
        self.assertEqual(out["rejected_batches"], "1")
        self.assertEqual(out["stopped_early"], "1")

    def test_hard_error_stops_immediately(self):
        self.install_scope()
        out = self.run_harness("eperm", budget=256 * MIB, batch=32 * MIB, max_batches=8)
        self.assertEqual(out["status"], "stopped")
        self.assertEqual(out["stop"], "stall_rising")
        self.assertEqual(out["attempted"], "1")
        self.assertEqual(out["error_code"], str(errno.EPERM))

    def test_short_write_is_uncertain_not_success(self):
        self.install_scope()
        out = self.run_harness("short", budget=64 * MIB, batch=32 * MIB, max_batches=4)
        self.assertEqual(out["accepted"], "0")
        self.assertEqual(out["error_code"], str(errno.EIO))

    def test_no_swappiness_support_refuses_instead_of_degrading(self):
        self.install_scope()
        out = self.run_harness("einval", budget=256 * MIB, batch=32 * MIB, max_batches=8)
        self.assertEqual(out["status"], "unsupported")
        self.assertEqual(out["caps"], "no_swappiness_parameter")
        self.assertEqual(out["accepted"], "0")
        self.assertEqual(out["attempted"], "0",
                         "no reclaim write may follow once swappiness is unsupported")

    def test_stop_on_stall_prevents_the_next_batch(self):
        self.install_scope()
        # psi-stop of 1 hundredth of a percent: the host's live PSI exceeds it
        # almost always, so the loop must stop before asking for anything.
        out = self.run_harness("accept", budget=256 * MIB, batch=32 * MIB,
                               max_batches=8, psi_stop=1)
        self.assertEqual(out["attempted"], "0")
        self.assertEqual(out["stop"], "stall_rising")

    def test_empty_file_bucket_stops_before_asking(self):
        self.install_scope(file_bytes=0)
        out = self.run_harness("accept", budget=256 * MIB, batch=32 * MIB, max_batches=8)
        self.assertEqual(out["attempted"], "0")
        self.assertEqual(out["stop"], "no_cold_pages")

    def test_budget_is_never_exceeded(self):
        self.install_scope()
        out = self.run_harness("accept", budget=100 * MIB, batch=32 * MIB, max_batches=32)
        self.assertEqual(out["accepted"], str(100 * MIB),
                         "batch sizes are clamped to the remaining budget")
        self.assertEqual(out["status"], "ok")

    def test_max_batches_bounds_the_loop(self):
        self.install_scope()
        out = self.run_harness("accept", budget=1024 * MIB, batch=1 * MIB, max_batches=3)
        self.assertEqual(out["attempted"], "3")
        self.assertEqual(out["accepted"], str(3 * MIB))

    def test_names_are_stable(self):
        for name, value in CAPS.items():
            self.assertEqual(self.lib.mr_bal_caps_name(value).decode(), name)
        for name, value in STATUS.items():
            self.assertEqual(self.lib.mr_bal_run_status_name(value).decode(), name)
        for name, value in STOP.items():
            self.assertEqual(self.lib.mr_bal_stop_name(value).decode(), name)


@unittest.skipUnless(os.environ.get("MEMREDUCT_REAL_CGROUP_TEST") == "1",
                     "set MEMREDUCT_REAL_CGROUP_TEST=1 to run real reclaim")
class RealCgroupTest(unittest.TestCase):
    """Real control file, throwaway cgroup, real kernel answers.

    Opt-in on purpose: it performs an actual reclaim inside a cgroup that holds
    only this test's own child process. It is the test that proves the measured
    swappiness=0 behaviour rather than assuming it.
    """

    BASE = "/sys/fs/cgroup/user.slice/user-1000.slice/user@1000.service/app.slice"
    CHILD = (
        "import os,sys,time\n"
        "mib=int(sys.argv[1])\n"
        "buf=bytearray(mib*1024*1024)\n"
        "for i in range(0,len(buf),4096): buf[i]=1\n"
        "print('ready',flush=True)\n"
        "time.sleep(60)\n"
    )

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="memreduct-real-")
        cls.lib_path = os.path.join(cls.tmp, "libreclaim-real.so")
        subprocess.run(
            ["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-pedantic",
             "-shared", "-fPIC", "-I", NATIVE,
             os.path.join(NATIVE, "balance_reclaim.c"), "-o", cls.lib_path],
            check=True, capture_output=True, text=True,
        )
        cls.lib = ctypes.CDLL(cls.lib_path)
        cls.lib.mr_bal_caps_probe.argtypes = [ctypes.c_int, ctypes.POINTER(ctypes.c_int64)]
        cls.lib.mr_bal_caps_probe.restype = ctypes.c_int64

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def setUp(self):
        self.cg = os.path.join(self.BASE, "memreduct-test.scope")
        if os.path.isdir(self.cg):
            os.rmdir(self.cg)
        os.makedirs(self.cg)
        self.child = subprocess.Popen([sys.executable, "-c", self.CHILD, "200"],
                                      stdout=subprocess.PIPE, text=True)

    def tearDown(self):
        try:
            self.child.kill()
            self.child.wait(timeout=5)
        except Exception:
            pass
        try:
            os.rmdir(self.cg)
        except OSError:
            pass

    def load_child(self):
        with open(os.path.join(self.cg, "cgroup.procs"), "w", encoding="ascii") as fh:
            fh.write(str(self.child.pid))
        self.assertIn("ready", self.child.stdout.readline())

    def stat(self):
        out = {}
        with open(os.path.join(self.cg, "memory.stat"), encoding="ascii") as fh:
            for line in fh:
                parts = line.split()
                if len(parts) == 2:
                    out[parts[0]] = int(parts[1])
        return out

    def test_caps_reports_file_only_on_this_host(self):
        scope_fd = os.open(self.cg, os.O_RDONLY | os.O_DIRECTORY)
        try:
            caps = ctypes.c_int64()
            self.assertEqual(self.lib.mr_bal_caps_probe(scope_fd, ctypes.byref(caps)), 0)
        finally:
            os.close(scope_fd)
        self.assertEqual(caps.value, 1, "host should accept 'N swappiness=0'")

    def test_swappiness_zero_takes_no_anonymous_page(self):
        """The measured invariant: with anon pages present and no file cache,
        a swappiness=0 request reclaims nothing rather than pushing anon out."""
        self.load_child()
        before = self.stat()
        scope_fd = os.open(self.cg, os.O_RDONLY | os.O_DIRECTORY)
        try:
            cfg = RunConfig()
            # A tiny batch: the request is small enough that no plausible file
            # page can absorb it, so EAGAIN is the expected kernel answer.
            self.lib.mr_bal_run_config_for_budget(8 * MIB, ctypes.byref(cfg))
            cfg.batch_bytes = 8 * MIB
            cfg.max_batches = 1
            result = RunResult()
            self.assertEqual(self.lib.mr_bal_reclaim_run(scope_fd, ctypes.byref(cfg),
                                                         ctypes.byref(result)), 0)
        finally:
            os.close(scope_fd)
        after = self.stat()
        self.assertLessEqual(after["anon"], before["anon"])
        self.assertGreaterEqual(after["anon"], before["anon"] - 2 * MIB,
                                "anonymous memory must not be pushed out by a "
                                "file-only request")
        self.assertEqual(result.accepted_bytes, 0,
                         "no file pages exist here, so nothing may be accepted")


if __name__ == "__main__":
    unittest.main()
