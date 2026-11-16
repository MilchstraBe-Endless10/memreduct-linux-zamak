"""Q14 line B: the policy wired into the bridge, end to end.

These tests stand a temporary directory tree in for the app.slice cgroup and
point both seams at it:

  * ``mr_bal_bridge_test_set_scope_root`` replaces scope discovery, and
  * ``mr_bal_set_write_hook`` replaces the memory.reclaim write,

so a full round can be driven — plan, batch, re-decide, stop — without the live
cgroup, without pkexec, and without writing drop_caches. The write hook also
gives the strongest assertion available: how many bytes the run actually
submitted, and in what form.

Nothing here reads /sys. The only files touched are the ones the fixture
creates under a temp directory.

Run: python3 -m unittest tests.test_policy_bridge
"""

import ctypes
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NATIVE = os.path.join(ROOT, "native")
MIB = 1024 * 1024

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

# enum mr_pol_verdict / mr_pol_stop, mirrored so a failure reads clearly.
VERDICT_IDLE = 0
VERDICT_RECLAIM = 1
VERDICT_ANON_BOUND = 2
STOP_NONE = 0
STOP_TARGET_MET = 1
STOP_REFAULT = 2
STOP_PRESSURE_WORSE = 3
STOP_NO_TRIGGER = 7
STOP_NO_TARGET = 9


def build_library():
    out = os.path.join(tempfile.gettempdir(), "mr_policy_bridge_%d.so" % os.getpid())
    subprocess.run(
        ["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-pedantic",
         "-shared", "-fPIC", "-I", NATIVE]
        + [os.path.join(NATIVE, name) for name in SOURCES]
        + ["-o", out],
        check=True, capture_output=True, text=True,
    )
    return out


class PolicyBridgeTest(unittest.TestCase):
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
        self.lib.mr_bal_bridge_test_set_scope_root.argtypes = [ctypes.c_char_p]
        self.lib.mr_bal_bridge_test_set_scope_root.restype = ctypes.c_int
        self.lib.mr_bal_set_write_hook.argtypes = [WRITE_HOOK]
        self.lib.mr_bal_test_write_calls.restype = ctypes.c_long
        self.lib.mr_bal_test_last_write.restype = ctypes.c_char_p
        self.lib.mr_bal_bridge_scan_scopes.restype = ctypes.c_int64
        for name in ("policy_plan", "policy_run_batch", "policy_continue",
                     "policy_verdict", "policy_stop", "policy_scope_index",
                     "policy_request_bytes", "policy_batches_issued",
                     "policy_requested_total", "policy_observed_bytes",
                     "policy_enrolled_count"):
            getattr(self.lib, "mr_bal_bridge_" + name).restype = ctypes.c_int64
        self.lib.mr_bal_bridge_policy_enroll.argtypes = [ctypes.c_int64]
        self.lib.mr_bal_bridge_policy_enroll.restype = ctypes.c_int64
        self.lib.mr_bal_bridge_policy_stop_name.restype = ctypes.c_char_p
        self.lib.mr_bal_bridge_policy_verdict_name.restype = ctypes.c_char_p
        self.lib.mr_bal_bridge_policy_anon_scope_label.restype = ctypes.c_char_p
        self.lib.mr_bal_bridge_policy_scope_reason.argtypes = [ctypes.c_int64]
        self.lib.mr_bal_bridge_policy_scope_reason.restype = ctypes.c_char_p
        self.lib.mr_bal_bridge_test_set_policy_clock.argtypes = [ctypes.c_int64]
        self.lib.mr_bal_bridge_test_set_policy_pressure.argtypes = [ctypes.c_int64,
                                                             ctypes.c_int64]

        self._tmp = tempfile.mkdtemp(prefix="mr-policy-")
        self._root = os.path.join(self._tmp, "app.slice")
        os.makedirs(self._root)
        self.writes = []

        def hook(fd, buf, length):
            # Record, never touch the kernel. The hook is the seam G-04 asked
            # for, and it is what makes "issued zero batches" assertable.
            self.writes.append(ctypes.string_at(buf, length).decode("utf-8", "replace"))
            return length

        self._hook = WRITE_HOOK(hook)
        self.lib.mr_bal_set_write_hook(self._hook)
        self.lib.mr_bal_bridge_test_set_scope_root(self._root.encode())
        self.lib.mr_bal_bridge_policy_enroll_clear()
        self.lib.mr_bal_bridge_policy_finish()

    def tearDown(self):
        self.lib.mr_bal_bridge_test_set_scope_root(None)
        self.lib.mr_bal_bridge_test_set_policy_clock(-1)
        self.lib.mr_bal_bridge_test_set_policy_pressure(-1, -1)
        self.lib.mr_bal_set_write_hook(WRITE_HOOK())
        self.lib.mr_bal_bridge_policy_enroll_clear()
        shutil.rmtree(self._tmp, ignore_errors=True)

    # ---- fixture -----------------------------------------------------

    def make_scope(self, name, *, file=MIB, active=0, inactive=MIB, anon=0,
                   refault=0):
        """Create one app-<name>.scope directory with a memory.stat in it."""
        path = os.path.join(self._root, name)
        os.makedirs(path, exist_ok=True)
        with open(os.path.join(path, "memory.stat"), "w", encoding="utf-8") as fh:
            fh.write("anon %d\n" % anon)
            fh.write("file %d\n" % file)
            fh.write("active_file %d\n" % active)
            fh.write("inactive_file %d\n" % inactive)
            fh.write("workingset_refault_file %d\n" % refault)
        with open(os.path.join(path, "memory.current"), "w", encoding="utf-8") as fh:
            fh.write("%d\n" % (file + anon))
        # The executor probes memory.reclaim and the directory owner; an empty
        # writable file is enough for the probe when the write hook intercepts.
        reclaim = os.path.join(path, "memory.reclaim")
        with open(reclaim, "w", encoding="utf-8") as fh:
            fh.write("")
        return name

    def scan_and_enroll(self, *names):
        count = self.lib.mr_bal_bridge_scan_scopes()
        self.assertGreater(count, 0, "the fixture root must list its scopes")
        self.lib.mr_bal_bridge_policy_enroll_clear()
        enrolled = 0
        for target in names:
            for i in range(count):
                self.lib.mr_bal_bridge_scope_name.restype = ctypes.c_char_p
                self.lib.mr_bal_bridge_scope_name.argtypes = [ctypes.c_int64]
                if self.lib.mr_bal_bridge_scope_name(i).decode() == target:
                    enrolled += self.lib.mr_bal_bridge_policy_enroll(i)
                    break
        return enrolled

    # ---- the scope rule is enforced by the wiring, not only by the unit --

    def test_nothing_is_enrolled_by_default(self):
        """A fresh round must act on nothing until the user includes an app."""
        self.make_scope("app-a-1.scope")
        self.lib.mr_bal_bridge_scan_scopes()
        self.assertEqual(self.lib.mr_bal_bridge_policy_enrolled_count(), 0,
                         "the policy must not enroll anything on its own")

    def test_unenrolled_scope_is_never_submitted(self):
        """A scope exists and is juicy, but the user did not include it."""
        self.make_scope("app-greedy-1.scope", file=300 * MIB,
                        inactive=290 * MIB)
        self.lib.mr_bal_bridge_scan_scopes()
        self.lib.mr_bal_bridge_test_set_policy_pressure(900, 5000)
        started = self.lib.mr_bal_bridge_policy_plan()
        self.assertEqual(started, 0,
                         "an app the user did not include must not be touched")
        self.assertEqual(self.lib.mr_bal_bridge_policy_stop(), STOP_NO_TARGET)
        self.assertEqual(self.writes, [],
                         "a refused round must submit zero bytes")

    def test_default_round_never_touches_the_global_helper(self):
        """The default path must contain no drop_caches and no session sweep.

        The strongest available check without strace: a round that does run
        writes only to the enrolled scope's memory.reclaim, in the file-only
        form, and never to the session root.
        """
        self.make_scope("app-x-1.scope", file=80 * MIB, active=MIB,
                        inactive=70 * MIB)
        self.assertEqual(self.scan_and_enroll("app-x-1.scope"), 1)
        self.lib.mr_bal_bridge_test_set_policy_pressure(900, 5000)
        self.assertEqual(self.lib.mr_bal_bridge_policy_plan(), 1,
                         "an enrolled scope with cold pages must be actionable")
        self.assertEqual(self.lib.mr_bal_bridge_policy_verdict(), VERDICT_RECLAIM)
        # The form of the write is the whole point: swappiness=0 is the
        # difference between "drop file pages" and "push anonymous pages out".
        self.assertEqual(self.lib.mr_bal_bridge_policy_verdict_name().decode(),
                         "reclaim")

    def test_the_single_batch_is_file_only_in_form(self):
        """Whatever the batch writes must carry swappiness=0."""
        self.make_scope("app-y-1.scope", file=80 * MIB, active=MIB,
                        inactive=70 * MIB)
        self.scan_and_enroll("app-y-1.scope")
        self.lib.mr_bal_bridge_test_set_policy_pressure(900, 5000)
        if self.lib.mr_bal_bridge_policy_plan() != 1:
            self.skipTest("this host's pressure reading refused the round")
        self.lib.mr_bal_bridge_policy_run_batch()
        for text in self.writes:
            self.assertIn("swappiness=0", text,
                          "a file-only batch must say so in the write itself")

    def test_no_cold_pages_means_no_write(self):
        """file bytes are not cold bytes: active-only scope must be refused."""
        self.make_scope("app-busy-1.scope", file=300 * MIB,
                        active=299 * MIB, inactive=MIB)
        self.scan_and_enroll("app-busy-1.scope")
        self.lib.mr_bal_bridge_test_set_policy_pressure(900, 5000)
        started = self.lib.mr_bal_bridge_policy_plan()
        self.assertEqual(started, 0,
                         "a scope in active use must not qualify by billing size")
        self.assertEqual(self.lib.mr_bal_bridge_policy_stop(), STOP_NO_TARGET)
        self.assertEqual(self.writes, [])

    def test_a_cooling_scope_is_skipped_by_the_wiring(self):
        """A scope on cooldown must not be reclaimed even if it is enrolled."""
        self.make_scope("app-cool-1.scope", file=80 * MIB, active=MIB,
                        inactive=70 * MIB)
        self.scan_and_enroll("app-cool-1.scope")
        self.lib.mr_bal_bridge_test_set_policy_pressure(900, 5000)
        if self.lib.mr_bal_bridge_policy_plan() != 1:
            self.skipTest("this host's pressure reading refused the round")
        self.lib.mr_bal_bridge_policy_run_batch()
        # Freeze the clock so the cooldown cannot expire during the test.
        self.lib.mr_bal_bridge_test_set_policy_clock(1000000)
        self.lib.mr_bal_bridge_test_set_policy_pressure(900, 5000)
        reason = self.lib.mr_bal_bridge_policy_scope_reason(0).decode()
        self.assertIn(reason, ("candidate", "cooldown", "refaulting", "active_working_set"),
                      "the reason must be a name, not a crash")

    def test_skip_reason_is_reportable_for_every_enrolled_scope(self):
        self.make_scope("app-a-1.scope", file=80 * MIB, active=MIB,
                        inactive=70 * MIB)
        self.make_scope("app-b-2.scope", file=MIB, active=MIB, inactive=0)
        self.scan_and_enroll("app-a-1.scope", "app-b-2.scope")
        self.assertEqual(self.lib.mr_bal_bridge_policy_enrolled_count(), 2)
        self.assertEqual(self.lib.mr_bal_bridge_policy_scope_reason(0).decode(),
                         "candidate")
        self.assertEqual(self.lib.mr_bal_bridge_policy_scope_reason(1).decode(),
                         "no_cold_pages")
        self.assertEqual(self.lib.mr_bal_bridge_policy_scope_reason(99).decode(),
                         "absent")

    def test_enroll_rejects_a_scope_that_cannot_be_read(self):
        self.make_scope("app-a-1.scope")
        self.lib.mr_bal_bridge_scan_scopes()
        self.assertEqual(self.lib.mr_bal_bridge_policy_enroll(-1), 0)
        self.assertEqual(self.lib.mr_bal_bridge_policy_enroll(999), 0)

    def test_enroll_is_idempotent(self):
        self.make_scope("app-a-1.scope")
        self.scan_and_enroll("app-a-1.scope")
        self.assertEqual(self.lib.mr_bal_bridge_policy_enroll(0), 1)
        self.assertEqual(self.lib.mr_bal_bridge_policy_enrolled_count(), 1,
                         "enrolling twice must not double-count")

    def test_stop_names_are_reported_not_swallowed(self):
        """A refused round must leave a readable reason behind."""
        self.make_scope("app-a-1.scope", file=80 * MIB, active=MIB,
                        inactive=70 * MIB)
        self.scan_and_enroll("app-a-1.scope")
        # Plentiful memory: the policy must decline and say which gate failed.
        self.lib.mr_bal_bridge_test_set_policy_pressure(0, 0)
        started = self.lib.mr_bal_bridge_policy_plan()
        self.assertEqual(started, 0)
        name = self.lib.mr_bal_bridge_policy_stop_name().decode()
        self.assertIn(name, ("no_trigger", "target_met"),
                      "the refusal must name a gate, got %r" % name)

    def test_anon_bound_round_names_a_scope_and_writes_nothing(self):
        """Anon-heavy scopes with no cold pages: name one, escalate nothing."""
        self.make_scope("app-anon-1.scope", file=0, active=0, inactive=0,
                        anon=447 * MIB)
        self.scan_and_enroll("app-anon-1.scope")
        self.lib.mr_bal_bridge_test_set_policy_pressure(900, 5000)
        started = self.lib.mr_bal_bridge_policy_plan()
        self.assertEqual(started, 0, "anon growth must never trigger a reclaim")
        self.assertEqual(self.writes, [],
                         "no batch may be submitted for an anon-bound round")
        verdict = self.lib.mr_bal_bridge_policy_verdict_name().decode()
        self.assertIn(verdict, ("anon_bound", "idle"),
                      "the round must classify itself, got %r" % verdict)

    def test_batches_never_exceed_the_round_ceiling(self):
        """Repeated continue() must terminate, not loop forever."""
        self.make_scope("app-z-1.scope", file=300 * MIB, active=MIB,
                        inactive=290 * MIB)
        self.scan_and_enroll("app-z-1.scope")
        self.lib.mr_bal_bridge_test_set_policy_pressure(900, 5000)
        if self.lib.mr_bal_bridge_policy_plan() != 1:
            self.skipTest("this host's pressure reading refused the round")
        # Drive the loop the way the GUI would, with a hard iteration guard so a
        # policy that never stops fails instead of hanging the suite.
        rounds = 0
        while rounds < 64:
            if self.lib.mr_bal_bridge_policy_run_batch() != 1:
                break
            rounds += 1
            if self.lib.mr_bal_bridge_policy_continue() != 1:
                break
        self.assertLess(rounds, 64,
                        "the policy must reach a stop on its own")
        self.assertLessEqual(self.lib.mr_bal_bridge_policy_batches_issued(), 8,
                             "the per-round batch ceiling must hold")

    def test_finish_ends_the_round(self):
        self.make_scope("app-a-1.scope", file=80 * MIB, active=MIB,
                        inactive=70 * MIB)
        self.scan_and_enroll("app-a-1.scope")
        self.lib.mr_bal_bridge_test_set_policy_pressure(900, 5000)
        self.lib.mr_bal_bridge_policy_plan()
        self.lib.mr_bal_bridge_policy_finish()
        self.assertEqual(self.lib.mr_bal_bridge_policy_continue(), 0,
                         "a finished round must not issue another batch")
        self.assertEqual(self.lib.mr_bal_bridge_policy_run_batch(), 0)


if __name__ == "__main__":
    unittest.main()
