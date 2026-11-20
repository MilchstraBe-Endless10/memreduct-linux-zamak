"""G-04: the S4 pressure path must be reachable without the live cgroup.

The capability matrix recorded this as a known defect: balance_bridge.c opened
``<session>/memory.reclaim`` directly, so no test could drive the loop at all
and the guard behaviour only ever existed as a manual observation. The fix is
a session-root override on the bridge; these tests use it to stand a temporary
directory in for the cgroup.

Nothing here writes to /sys. The substitute root is a tmpfs directory, and the
only file the loop can touch is the one this test creates.

Run: python3 -m unittest tests.test_s4_path
"""

import ctypes
import errno
import os
import subprocess
import shutil
import tempfile
import time
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NATIVE = os.path.join(ROOT, "native")
SOURCES = [
    "balance_bridge.c",
    "balance_gui.c",
    "balance_exec.c",
    "balance_snapshot.c",
    "balance_plan.c",
    "balance_reclaim.c",
    "balance_policy.c",
]

WRITE_HOOK = ctypes.CFUNCTYPE(
    ctypes.c_long, ctypes.c_int, ctypes.c_char_p, ctypes.c_ulong
)


def build_library():
    out = os.path.join(tempfile.gettempdir(), "mr_s4_%d.so" % os.getpid())
    subprocess.run(
        ["cc", "-std=c11", "-O2", "-fPIC", "-shared", "-o", out]
        + [os.path.join(NATIVE, name) for name in SOURCES],
        check=True,
        capture_output=True,
    )
    return out


class S4PathTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls._libpath = build_library()
        cls.harness = ctypes.CDLL(cls._libpath)

    @classmethod
    def tearDownClass(cls):
        try:
            os.unlink(cls._libpath)
        except OSError:
            pass

    def setUp(self):
        self.lib = ctypes.CDLL(self._libpath)
        self.lib.mr_bal_bridge_deep_reset.argtypes = []
        self.lib.mr_bal_bridge_test_set_deep_telemetry.argtypes = [ctypes.c_double,
                                                                   ctypes.c_int64]
        self.lib.mr_bal_bridge_test_set_deep_telemetry.restype = None
        self.lib.mr_bal_bridge_deep_reset()
        # Pin the S4 guard telemetry to a calm machine: the suite itself
        # generates real PSI/swap traffic, which is machine state, not
        # behaviour under test.
        self.lib.mr_bal_bridge_test_set_deep_telemetry(0.0, 0)
        self.lib.mr_bal_set_write_hook.argtypes = [WRITE_HOOK]
        self.lib.mr_bal_test_set_write_errno.argtypes = [ctypes.c_int]
        self.lib.mr_bal_bridge_test_set_session_root.argtypes = [ctypes.c_char_p]
        self.lib.mr_bal_bridge_test_set_session_root.restype = ctypes.c_int
        for name in (
            "mr_bal_bridge_deep_start",
            "mr_bal_bridge_deep_poll",
            "mr_bal_bridge_deep_stop_reason",
            "mr_bal_bridge_deep_stage_ran",
            "mr_bal_bridge_deep_batches_completed",
            "mr_mem_available_bytes",
        ):
            getattr(self.lib, name).restype = ctypes.c_int64

        self.calls = []
        self.refuse_from_batch = None
        self.seen_batches = 0
        self._tmp = tempfile.mkdtemp(prefix="mr-s4-root-")
        self._root = self._tmp.encode()
        # The substitute root must look like a cgroup: the loop opens
        # <root>/memory.reclaim, so that file has to exist.
        self._reclaim = os.path.join(self._tmp, "memory.reclaim")
        with open(self._reclaim, "w") as handle:
            handle.write("0\n")

        def hook(fd, text, length):
            payload = text or b""
            self.calls.append(payload.decode(errors="replace"))
            is_batch = payload.isdigit()    # a batch is a bare byte count
            if is_batch:
                self.seen_batches += 1
                if (self.refuse_from_batch is not None
                        and self.seen_batches >= self.refuse_from_batch):
                    self.lib.mr_bal_test_set_write_errno(errno.EAGAIN)
                    return -1
            return length

        self._hook = WRITE_HOOK(hook)
        self.lib.mr_bal_set_write_hook(self._hook)

    def tearDown(self):
        self.lib.mr_bal_bridge_test_set_deep_telemetry(-1.0, -1)
        self.lib.mr_bal_bridge_test_set_session_root(None)
        self.lib.mr_bal_set_write_hook(WRITE_HOOK())
        try:
            os.unlink(self._reclaim)
            os.rmdir(self._tmp)
        except OSError:
            pass

    def _settle(self, limit=1500):
        for _ in range(limit):
            if self.lib.mr_bal_bridge_deep_poll() == 1:
                return True
            time.sleep(0.01)
        return False

    def _start(self):
        baseline = self.lib.mr_mem_available_bytes()
        return self.lib.mr_bal_bridge_deep_start(baseline if baseline > 0 else 1)

    def test_override_replaces_the_discovered_root(self):
        """The seame exists and reports what was set, then restores."""
        self.lib.mr_bal_bridge_test_set_session_root(self._root)
        self.lib.mr_bal_bridge_test_session_root.restype = ctypes.c_char_p
        current = self.lib.mr_bal_bridge_test_session_root()
        self.assertEqual(current, self._root,
                         "the override must be what the loop would use")
        self.lib.mr_bal_bridge_test_set_session_root(None)
        restored = self.lib.mr_bal_bridge_test_session_root()
        self.assertNotEqual(restored, self._root,
                            "clearing the override must restore discovery")

    def test_s4_path_is_reached_through_the_substitute_root(self):
        """With a real file behind the override, S4 runs and writes batches."""
        self.lib.mr_bal_bridge_test_set_session_root(self._root)
        first = self._start()
        if first != 0:
            self.skipTest("worker thread unavailable in this environment")
        self.assertTrue(self._settle(), "the run must finish")
        self.assertEqual(self.lib.mr_bal_bridge_deep_stage_ran(3), 1,
                         "S4 must record that it ran")
        self.assertGreater(self.seen_batches, 0,
                           "S4 must submit at least one batch")

    def test_missing_reclaim_file_stops_instead_of_writing(self):
        """If the root has no memory.reclaim, the run stops, it does not spin.

        One root, one reclaim target: S2/S3's session sweep and S4's batch
        loop both write <root>/memory.reclaim, so a missing target fails the
        sweep first and the worker fail-closes there — reaching S4 would mean
        sweeping succeeded without its target, which this product cannot do.
        The assertable safety property is: no batches completed and a stop
        reason is left behind, on every host, with no reliance on a real
        session cgroup existing.
        """
        empty = tempfile.mkdtemp(prefix="mr-s4-empty-")
        try:
            with open(os.path.join(empty, "memory.stat"), "w",
                      encoding="utf-8") as handle:
                handle.write("file 268435456\nanon 0\n")
            self.lib.mr_bal_bridge_test_set_session_root(empty.encode())
            first = self._start()
            if first != 0:
                self.skipTest("worker thread unavailable in this environment")
            self.assertTrue(self._settle(), "the run must finish")
            self.assertEqual(self.lib.mr_bal_bridge_deep_stage_ran(3), 0,
                             "a sweep without its write target must not reach S4")
            self.assertEqual(self.lib.mr_bal_bridge_deep_batches_completed(), 0,
                             "a missing target must complete no batches")
            self.assertNotEqual(self.lib.mr_bal_bridge_deep_stop_reason(), 0,
                                "a missing target must record a stop reason")
        finally:
            shutil.rmtree(empty, ignore_errors=True)


if __name__ == "__main__":
    unittest.main()
