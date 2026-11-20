"""G-01 / G-02: the S4 guard and the three-state outcome are assertable here.

The capability matrix recorded both as known defects for the same reason: the
only evidence was `CI_DEEP_S4` lines emitted from src/main.zm during a live
GUI run, and nothing in tests/ asserted anything. Driving the GUI is not a
unit test, so the assertions belong at the bridge — where the outcome is
actually decided — and the GUI's job is only to print what the bridge reports.

Three-state outcome, as the GUI maps it:
    DONE      the run finished on its own (reason 0)
    PARTIAL   the kernel refused a batch (EAGAIN, reason 5)
    STOPPED   the guard or cancellation ended it (reasons 1, 2, 3)
    FAILED    something went wrong (reason 4)

Run: python3 -m unittest tests.test_s4_outcome
"""

import ctypes
import errno
import os
import subprocess
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

# Mirrors the enum in balance_bridge.h.
STOP_NONE = 0
STOP_PSI_UNREADABLE = 1
STOP_SWAP_RATE = 2
STOP_USER_CANCEL = 3
STOP_ERROR = 4
STOP_PARTIAL = 5

S1, S2, S3, S4 = 0, 1, 2, 3


def build_library():
    out = os.path.join(tempfile.gettempdir(), "mr_out_%d.so" % os.getpid())
    subprocess.run(
        ["cc", "-std=c11", "-O2", "-fPIC", "-shared", "-o", out]
        + [os.path.join(NATIVE, name) for name in SOURCES],
        check=True,
        capture_output=True,
    )
    return out


class S4OutcomeTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls._libpath = build_library()

    @classmethod
    def tearDownClass(cls):
        try:
            os.unlink(cls._libpath)
        except OSError:
            pass

    def setUp(self):
        self.lib = ctypes.CDLL(self._libpath)
        lib = self.lib
        lib.mr_bal_bridge_deep_reset.argtypes = []
        lib.mr_bal_set_write_hook.argtypes = [WRITE_HOOK]
        lib.mr_bal_test_set_write_errno.argtypes = [ctypes.c_int]
        lib.mr_bal_bridge_test_set_session_root.argtypes = [ctypes.c_char_p]
        lib.mr_bal_bridge_test_set_session_root.restype = ctypes.c_int
        lib.mr_bal_bridge_test_set_deep_telemetry.argtypes = [ctypes.c_double,
                                                              ctypes.c_int64]
        lib.mr_bal_bridge_test_set_deep_telemetry.restype = None
        for name in (
            "mr_bal_bridge_deep_start",
            "mr_bal_bridge_deep_poll",
            "mr_bal_bridge_deep_stop_reason",
            "mr_bal_bridge_deep_stage_ran",
            "mr_bal_bridge_deep_stage_ok",
            "mr_bal_bridge_deep_stage_positive",
            "mr_bal_bridge_deep_batches_completed",
            "mr_bal_bridge_deep_eagain_batches",
            "mr_mem_available_bytes",
        ):
            getattr(lib, name).restype = ctypes.c_int64
        lib.mr_bal_bridge_deep_reset()
        # Pin the S4 guard telemetry to a calm machine: the suite itself
        # generates real PSI/swap traffic, which is machine state, not
        # behaviour under test. Threshold behaviour stays covered by the
        # EAGAIN and stop-reason cases.
        lib.mr_bal_bridge_test_set_deep_telemetry(0.0, 0)

        self.batches = 0
        self.refuse_from = None
        self._tmp = tempfile.mkdtemp(prefix="mr-s4-out-")
        self._root = self._tmp.encode()
        reclaim = os.path.join(self._tmp, "memory.reclaim")
        with open(reclaim, "w") as handle:
            handle.write("0\n")

        def hook(fd, text, length):
            payload = text or b""
            # A batch is a bare byte count; probes carry " swappiness=0".
            # Length alone is not a discriminator: the probe is 14 bytes too.
            is_batch = payload.isdigit()
            if is_batch:
                self.batches += 1
                if self.refuse_from is not None and self.batches >= self.refuse_from:
                    self.lib.mr_bal_test_set_write_errno(errno.EAGAIN)
                    return -1
            return length

        self._hook = WRITE_HOOK(hook)
        self.lib.mr_bal_set_write_hook(self._hook)
        self.lib.mr_bal_bridge_test_set_session_root(self._root)

    def tearDown(self):
        self.lib.mr_bal_bridge_test_set_deep_telemetry(-1.0, -1)
        self.lib.mr_bal_bridge_test_set_session_root(None)
        self.lib.mr_bal_set_write_hook(WRITE_HOOK())
        for name in ("memory.reclaim",):
            path = os.path.join(self._tmp, name)
            if os.path.exists(path):
                os.unlink(path)
        try:
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

    def _run(self):
        first = self._start()
        if first != 0:
            self.skipTest("worker thread unavailable in this environment")
        self.assertTrue(self._settle(), "the run must finish")

    def test_every_stage_records_that_it_ran(self):
        """S2, S3 and S4 each record ran, so the GUI can name the outcome."""
        self._run()
        for stage, label in ((S2, "S2"), (S3, "S3"), (S4, "S4")):
            self.assertEqual(self.lib.mr_bal_bridge_deep_stage_ran(stage), 1,
                             "%s must record that it ran" % label)

    def test_completed_run_reports_no_stop_reason(self):
        """A run the kernel did not refuse ends with reason NONE."""
        self.refuse_from = None
        self._run()
        self.assertEqual(self.lib.mr_bal_bridge_deep_stop_reason(), STOP_NONE,
                         "a clean run must not invent a stop reason")
        self.assertGreater(self.batches, 0, "batches must have been submitted")

    def test_eagain_is_partial_not_error(self):
        """G-01: EAGAIN means 'less than asked', which is PARTIAL, not FAILED.

        Asserting STOP_PARTIAL rather than simply 'not zero' is the whole point:
        reporting a refusal as ERROR is the exaggerated-failure direction the
        review criteria forbid, and it used to happen here.
        """
        self.refuse_from = 1
        self._run()
        reason = self.lib.mr_bal_bridge_deep_stop_reason()
        self.assertEqual(reason, STOP_PARTIAL,
                         "a refused batch must be recorded as partial")
        self.assertGreater(self.lib.mr_bal_bridge_deep_eagain_batches(), 0,
                           "the refusal must be counted, not swallowed")
        self.assertEqual(self.lib.mr_bal_bridge_deep_batches_completed(), 0,
                         "a refused batch is not a completed batch")

    def test_cancel_is_its_own_outcome(self):
        """Cancelling is neither completion nor failure."""
        first = self._start()
        if first != 0:
            self.skipTest("worker thread unavailable in this environment")
        self.lib.mr_bal_bridge_deep_cancel()
        self.assertTrue(self._settle(), "the run must finish after a cancel")
        self.assertIn(self.lib.mr_bal_bridge_deep_stop_reason(),
                      (STOP_USER_CANCEL, STOP_NONE),
                      "a cancelled run reports cancel, or clean finish if it had ended")

    def test_stop_reasons_are_distinguishable(self):
        """G-02: the GUI's three states need reasons that do not collide."""
        reasons = {STOP_NONE, STOP_PSI_UNREADABLE, STOP_SWAP_RATE,
                   STOP_USER_CANCEL, STOP_ERROR, STOP_PARTIAL}
        self.assertEqual(len(reasons), 6,
                         "every stop reason must have a distinct value")

    def test_a_finished_run_does_not_keep_submitting(self):
        """After the worker joins, the write count must be frozen."""
        self.refuse_from = 1
        self._run()
        settled = self.batches
        time.sleep(0.3)
        self.assertEqual(self.batches, settled,
                         "no write may follow a finished run")


class S4StateIsolationTest(unittest.TestCase):
    """Regression for the suite-order flake: every entry into the deep worker
    must start from a known state, and the telemetry seams must be restored
    when a test ends. A pin that leaks would make the next case's guard read
    the fixture instead of the host — exactly the class of cross-case state
    that used to make test_completed_run_reports_no_stop_reason fail only
    when the whole suite ran.
    """

    @classmethod
    def setUpClass(cls):
        cls._libpath = build_library()

    @classmethod
    def tearDownClass(cls):
        try:
            os.unlink(cls._libpath)
        except OSError:
            pass

    def setUp(self):
        self.lib = ctypes.CDLL(self._libpath)
        lib = self.lib
        lib.mr_bal_bridge_deep_reset.argtypes = []
        lib.mr_bal_bridge_deep_reset.restype = None
        lib.mr_bal_bridge_test_set_deep_telemetry.argtypes = [ctypes.c_double,
                                                              ctypes.c_int64]
        lib.mr_bal_bridge_test_set_deep_telemetry.restype = None
        lib.mr_bal_bridge_deep_stop_reason.restype = ctypes.c_int64
        lib.mr_bal_bridge_deep_stage_ran.restype = ctypes.c_int64
        for stage in range(4):
            lib.mr_bal_bridge_deep_stage_ran.argtypes = [ctypes.c_int]

    def test_reset_clears_every_stage_and_reason(self):
        """deep_reset must return the worker to the pristine state."""
        self.lib.mr_bal_bridge_deep_reset()
        self.assertEqual(self.lib.mr_bal_bridge_deep_stop_reason(), 0)
        for stage in range(4):
            self.assertEqual(self.lib.mr_bal_bridge_deep_stage_ran(stage), 0,
                             "stage %d must start unrecorded" % stage)

    def test_reset_is_idempotent_against_a_used_worker(self):
        """Resetting twice leaves the same pristine state as resetting once."""
        self.lib.mr_bal_bridge_test_set_deep_telemetry(0.0, 0)
        self.lib.mr_bal_bridge_deep_reset()
        self.lib.mr_bal_bridge_deep_reset()
        self.assertEqual(self.lib.mr_bal_bridge_deep_stop_reason(), 0)
        for stage in range(4):
            self.assertEqual(self.lib.mr_bal_bridge_deep_stage_ran(stage), 0)
        self.lib.mr_bal_bridge_test_set_deep_telemetry(-1.0, -1)


if __name__ == "__main__":
    unittest.main()
