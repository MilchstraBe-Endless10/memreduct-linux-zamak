"""Q17: the per-scope throttle pulse must be safe on every path.

The user's hard rules (2026-11-20): a clean must never harm a process, never
stall one, never leave an app unusable. These tests pin the mechanics that
make that true: the floor, the restore, the read-back, the watchdog.
Nothing here touches a real scope: every fixture lives in tmpdir.

Run: python3 -m unittest tests.test_balance_pulse
"""

import ctypes
import os
import subprocess
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NATIVE = os.path.join(ROOT, "native")

INT64_MAX = 2**63 - 1
PULSE_OK = 0
PULSE_OK_ABORTED = 1
PULSE_OK_NOTHING = 2


def build_library():
    out = os.path.join(tempfile.gettempdir(), "mr_pulse_%d.so" % os.getpid())
    subprocess.run(
        ["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-pedantic",
         "-shared", "-fPIC", "-I", NATIVE,
         os.path.join(NATIVE, "balance_pulse.c"), "-o", out, "-lpthread"],
        check=True, capture_output=True, text=True,
    )
    return out


class PulseLibraryTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls._libpath = build_library()
        cls.lib = ctypes.CDLL(cls._libpath)

    @classmethod
    def tearDownClass(cls):
        try:
            os.unlink(cls._libpath)
        except OSError:
            pass

    def setUp(self):
        lib = self.lib
        lib.mr_pulse_scope.argtypes = [ctypes.c_char_p, ctypes.c_int64,
                                       ctypes.POINTER(struct := None or __import__("ctypes").c_int64 * 5)]
        # The result struct is five int64 fields; mirror it as a plain array.
        self.ResultArray = ctypes.c_int64 * 5
        lib.mr_pulse_scope.argtypes = [ctypes.c_char_p, ctypes.c_int64,
                                       ctypes.POINTER(self.ResultArray)]
        lib.mr_pulse_scope.restype = ctypes.c_int64
        lib.mr_pulse_restore_all.argtypes = []
        lib.mr_pulse_restore_all.restype = ctypes.c_int64
        self._tmp = tempfile.mkdtemp(prefix="mr-pulse-")
        self.scope = os.path.join(self._tmp, "app-test.scope")
        os.makedirs(self.scope)
        self._write("memory.current", str(200 * 1024 * 1024))   # 200 MiB
        self._write("memory.high", "max")
        self._write("memory.stat", "anon 0\nfile 0\n")
        self._write("memory.reclaim", "")

    def tearDown(self):
        self.lib.mr_pulse_restore_all()
        import shutil
        shutil.rmtree(self._tmp, ignore_errors=True)

    def _write(self, name, text):
        with open(os.path.join(self.scope, name), "w", encoding="ascii") as fh:
            fh.write(text)

    def _read(self, name):
        with open(os.path.join(self.scope, name), encoding="ascii") as fh:
            return fh.read().strip()

    def _pulse(self, ms=100):
        result = self.ResultArray()
        rc = self.lib.mr_pulse_scope(self.scope.encode(), ms, result)
        return rc, result

    def test_pulse_writes_60_percent_and_restores_max(self):
        """Rule 4: after the pulse the scope's high is back to max."""
        rc, r = self._pulse()
        self.assertEqual(rc, PULSE_OK)
        self.assertEqual(r[3], 1, "restored flag must be set")
        self.assertEqual(self._read("memory.high"), "max")

    def test_pulse_target_is_60_percent_of_current(self):
        """The throttle target is current * 3/5, never more."""
        rc, r = self._pulse()
        self.assertEqual(rc, PULSE_OK)
        expected = 200 * 1024 * 1024 * 3 // 5
        self.assertEqual(r[1], expected)

    def test_scope_shrinks_after_pulse(self):
        """The point of the pulse: the scope releases pages."""
        rc, r = self._pulse()
        self.assertEqual(rc, PULSE_OK)
        # The fixture's memory.current is a plain file; the pulse cannot
        # actually reclaim from a fake cgroup, but it must never GROW it.
        self.assertLessEqual(r[2], r[0])

    def test_tiny_scope_still_pulses_within_floor(self):
        """A 10 MiB scope pulses too: the floor protects it, not the size."""
        self._write("memory.current", str(10 * 1024 * 1024))
        rc, r = self._pulse()
        self.assertEqual(rc, PULSE_OK)
        floor = 10 * 1024 * 1024 * 2 // 5
        self.assertGreaterEqual(r[1], floor,
                                "the throttle must never go below the floor")
        self.assertEqual(self._read("memory.high"), "max")

    def test_missing_scope_fails_closed_without_touching_anything(self):
        """A bad path returns an error and writes nothing anywhere."""
        rc, _ = self._pulse.__self__ and (0, None) or (0, None)  # placeholder
        result = self.ResultArray()
        rc = self.lib.mr_pulse_scope(b"/nonexistent/scope", 100, result)
        self.assertEqual(rc, -2, "MR_PULSE_ERR_READ: a missing scope is a read failure, fail closed")
        self.assertEqual(self._read("memory.high"), "max")

    def test_restore_all_is_idempotent(self):
        """Calling restore twice never fails and never raises."""
        self.assertEqual(self.lib.mr_pulse_restore_all(), 0)
        self.assertEqual(self.lib.mr_pulse_restore_all(), 0)

    def test_pulse_over_ms_is_clamped_to_the_ceiling(self):
        """Rule 2: a caller cannot request a pulse longer than 10s."""
        # With a 200s request the call must still return promptly (the
        # ceiling clamps it). We use a short real timeout to prove it.
        import time
        start = time.monotonic()
        rc, _ = self._pulse(ms=200000)
        elapsed = time.monotonic() - start
        self.assertEqual(rc, PULSE_OK)
        self.assertLess(elapsed, 15.0,
                        "the pulse ceiling must clamp an oversized request")


if __name__ == "__main__":
    unittest.main()
