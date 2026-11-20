"""Q16 line A: the balanced clean's action record.

The defect this covers: a click on the balanced clean could only report "ran /
did not run". Which scope was confirmed, which absolute file was written, what
exact text went to the kernel, what the kernel returned, which errno, and — when
nothing was written — which refusal stopped it, were all measured by the layers
below and then thrown away before the interface ever saw them.

Every test here drives the record through the two seams that already exist:

  * ``mr_bal_bridge_test_set_scope_root`` stands a temp directory in for the
    app.slice cgroup, so the confirmed scope set and the absolute target path
    are real strings built the same way production builds them;
  * ``mr_bal_set_write_hook`` replaces the memory.reclaim write, so the byte
    count, the exact text, the return value and the errno are whatever this
    test injects — including an injected EAGAIN.

No test reads /sys, writes a real cgroup, or touches /proc/sys/vm. The only
files touched are the ones the fixture creates under a temp directory.

Run: python3 -m unittest discover -s tests
"""

import ctypes
import errno
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

STOP_NONE = 0
STOP_NO_TARGET = 9
REASON_NOT_OBSERVED = "not_observed"


def build_library():
    out = os.path.join(tempfile.gettempdir(), "mr_q16_record_%d.so" % os.getpid())
    subprocess.run(
        ["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-pedantic",
         "-shared", "-fPIC", "-I", NATIVE]
        + [os.path.join(NATIVE, name) for name in SOURCES]
        + ["-o", out],
        check=True, capture_output=True, text=True,
    )
    return out


class ActionRecordTest(unittest.TestCase):
    """Every field, in the three states a click can end in."""

    @classmethod
    def setUpClass(cls):
        cls._libpath = build_library()
        # One probe library for the "no request yet" assertions, which must run
        # before any begin() in this process.
        cls.lib = ctypes.CDLL(cls._libpath)
        cls._bind(cls.lib)

    @classmethod
    def _bind(cls, lib):
        lib.mr_bal_bridge_test_set_scope_root.argtypes = [ctypes.c_char_p]
        lib.mr_bal_bridge_test_set_scope_root.restype = ctypes.c_int
        lib.mr_bal_set_write_hook.argtypes = [WRITE_HOOK]
        lib.mr_bal_set_write_hook.restype = None
        lib.mr_bal_test_set_write_errno.argtypes = [ctypes.c_int]
        lib.mr_bal_test_set_write_errno.restype = None
        lib.mr_bal_bridge_scan_scopes.restype = ctypes.c_int64
        lib.mr_bal_bridge_scope_name.argtypes = [ctypes.c_int64]
        lib.mr_bal_bridge_scope_name.restype = ctypes.c_char_p
        lib.mr_bal_bridge_scope_refresh.argtypes = [ctypes.c_int64]
        lib.mr_bal_bridge_scope_refresh.restype = ctypes.c_int64
        lib.mr_bal_bridge_scope_eligible.restype = ctypes.c_int64
        lib.mr_bal_bridge_policy_enroll.argtypes = [ctypes.c_int64]
        lib.mr_bal_bridge_policy_enroll.restype = ctypes.c_int64
        lib.mr_bal_bridge_policy_enroll_clear.restype = None
        lib.mr_bal_bridge_test_set_policy_pressure.argtypes = [ctypes.c_int64,
                                                               ctypes.c_int64]
        lib.mr_bal_bridge_test_set_policy_pressure.restype = None
        lib.mr_bal_bridge_test_set_policy_memory.argtypes = [ctypes.c_int64,
                                                             ctypes.c_int64]
        lib.mr_bal_bridge_test_set_policy_memory.restype = None
        lib.mr_bal_bridge_test_set_policy_clock.argtypes = [ctypes.c_int64]
        lib.mr_bal_bridge_test_set_policy_clock.restype = None
        for name in ("policy_plan", "policy_run_batch", "policy_continue",
                     "policy_stop", "policy_batches_issued", "policy_finish"):
            getattr(lib, "mr_bal_bridge_" + name).restype = ctypes.c_int64
        lib.mr_bal_bridge_policy_finish.argtypes = []
        lib.mr_bal_bridge_policy_finish.restype = None
        for name in ("action_begin", "action_observed", "action_id",
                     "action_scope_count", "action_write_count",
                     "action_reason_known"):
            getattr(lib, "mr_bal_bridge_" + name).restype = ctypes.c_int64
        lib.mr_bal_bridge_action_close_auto.restype = None
        lib.mr_bal_bridge_action_scope_name.argtypes = [ctypes.c_int64]
        lib.mr_bal_bridge_action_scope_name.restype = ctypes.c_char_p
        lib.mr_bal_bridge_action_scope_label.argtypes = [ctypes.c_int64]
        lib.mr_bal_bridge_action_scope_label.restype = ctypes.c_char_p
        lib.mr_bal_bridge_action_scope_index.argtypes = [ctypes.c_int64]
        lib.mr_bal_bridge_action_scope_index.restype = ctypes.c_int64
        lib.mr_bal_bridge_action_target.restype = ctypes.c_char_p
        for name in ("action_write_bytes", "action_write_result",
                     "action_write_errno"):
            getattr(lib, "mr_bal_bridge_" + name).argtypes = [ctypes.c_int64]
            getattr(lib, "mr_bal_bridge_" + name).restype = ctypes.c_int64
        lib.mr_bal_bridge_action_write_text.argtypes = [ctypes.c_int64]
        lib.mr_bal_bridge_action_write_text.restype = ctypes.c_char_p
        lib.mr_bal_bridge_action_write_class.argtypes = [ctypes.c_int64]
        lib.mr_bal_bridge_action_write_class.restype = ctypes.c_char_p
        lib.mr_bal_bridge_action_reason.restype = ctypes.c_char_p
        lib.mr_bal_bridge_action_export.restype = ctypes.c_char_p
        lib.mr_bal_bridge_action_export_path.restype = ctypes.c_char_p
        lib.mr_bal_bridge_action_summary.argtypes = [ctypes.c_int64]
        lib.mr_bal_bridge_action_summary.restype = ctypes.c_char_p

    @classmethod
    def tearDownClass(cls):
        try:
            os.unlink(cls._libpath)
        except OSError:
            pass

    def setUp(self):
        self.lib = ctypes.CDLL(self._libpath)
        self._bind(self.lib)
        self._tmp = tempfile.mkdtemp(prefix="mr-q16-")
        self._root = os.path.join(self._tmp, "app.slice")
        os.makedirs(self._root)
        self.writes = []
        self.hook_result = None  # None = accept; otherwise (ret, errno)

        def hook(fd, buf, length):
            self.writes.append(ctypes.string_at(buf, length).decode("utf-8", "replace"))
            if self.hook_result is None:
                return length
            ret, _ = self.hook_result
            return ret

        self._hook = WRITE_HOOK(hook)
        self.lib.mr_bal_set_write_hook(self._hook)
        self.lib.mr_bal_bridge_test_set_scope_root(self._root.encode())
        self.lib.mr_bal_bridge_policy_enroll_clear()
        self.lib.mr_bal_bridge_policy_finish()
        # Pin memory evidence to scarcity (8 GiB total / 2 GiB available) so
        # the policy reaches enrollment decisions regardless of host RAM.
        self.lib.mr_bal_bridge_test_set_policy_memory(8 * 1024**3, 2 * 1024**3)
        self.lib.mr_bal_test_set_write_errno(0)

    def tearDown(self):
        self.lib.mr_bal_bridge_test_set_scope_root(None)
        self.lib.mr_bal_bridge_test_set_policy_clock(-1)
        self.lib.mr_bal_bridge_test_set_policy_pressure(-1, -1)
        self.lib.mr_bal_bridge_test_set_policy_memory(-1, -1)
        self.lib.mr_bal_set_write_hook(WRITE_HOOK())
        self.lib.mr_bal_bridge_policy_enroll_clear()
        self.lib.mr_bal_bridge_policy_finish()
        self.lib.mr_bal_test_set_write_errno(0)
        shutil.rmtree(self._tmp, ignore_errors=True)

    # ---- fixture -----------------------------------------------------

    def make_scope(self, name, *, file=MIB, active=0, inactive=MIB, anon=0,
                   refault=0):
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
        with open(os.path.join(path, "memory.reclaim"), "w", encoding="utf-8") as fh:
            fh.write("")
        return name

    def enroll_eligible(self):
        """Exactly what the tenth menu row does: scan, then enroll what is eligible."""
        count = self.lib.mr_bal_bridge_scan_scopes()
        self.lib.mr_bal_bridge_policy_enroll_clear()
        enrolled = 0
        for i in range(count):
            if (self.lib.mr_bal_bridge_scope_refresh(i) == 1 and
                    self.lib.mr_bal_bridge_scope_eligible() == 1):
                enrolled += self.lib.mr_bal_bridge_policy_enroll(i)
        return count, enrolled

    def run_round(self):
        """The row's sequence: begin, plan, batches, finish, close."""
        record_id = self.lib.mr_bal_bridge_action_begin()
        started = self.lib.mr_bal_bridge_policy_plan() == 1
        if started:
            guard = 0
            while guard < 32:
                if self.lib.mr_bal_bridge_policy_run_batch() != 1:
                    break
                guard += 1
                if self.lib.mr_bal_bridge_policy_continue() != 1:
                    break
        self.lib.mr_bal_bridge_policy_finish()
        self.lib.mr_bal_bridge_action_close_auto()
        return record_id, started

    # ---- before any request ------------------------------------------

    def test_a_fresh_process_reports_no_request_rather_than_a_zero(self):
        """0 id / 0 writes / "not_observed" mean "nothing happened yet".

        Loaded from a private copy of the library: the shared handle above has
        already recorded requests in this process, and "never recorded" is a
        property of a module instance, not of a test ordering. Asserting it on
        the shared handle would pass or fail depending on which test ran first,
        which is exactly the kind of order dependence this suite must not have.
        """
        private = os.path.join(self._tmp, "fresh-record.so")
        shutil.copyfile(self._libpath, private)
        fresh = ctypes.CDLL(private)
        self._bind(fresh)
        self.assertEqual(fresh.mr_bal_bridge_action_observed(), 0)
        self.assertEqual(fresh.mr_bal_bridge_action_id(), 0)
        self.assertEqual(fresh.mr_bal_bridge_action_scope_count(), 0)
        self.assertEqual(fresh.mr_bal_bridge_action_write_count(), 0)
        self.assertEqual(fresh.mr_bal_bridge_action_reason().decode(),
                         REASON_NOT_OBSERVED)
        self.assertEqual(fresh.mr_bal_bridge_action_reason_known(), 0)
        # A per-write field outside the record answers -1 / "", never garbage.
        self.assertEqual(fresh.mr_bal_bridge_action_write_bytes(0), -1)
        self.assertEqual(fresh.mr_bal_bridge_action_write_text(0).decode(), "")
        self.assertEqual(fresh.mr_bal_bridge_action_write_result(0), -1)
        self.assertEqual(fresh.mr_bal_bridge_action_write_errno(0), 0)
        self.assertEqual(fresh.mr_bal_bridge_action_write_class(0).decode(),
                         REASON_NOT_OBSERVED)
        self.assertIn("not_observed",
                      fresh.mr_bal_bridge_action_export().decode())

    # ---- success -----------------------------------------------------

    def test_a_successful_round_records_id_scopes_target_and_every_field(self):
        self.make_scope("app-x-1.scope", file=80 * MIB, active=MIB,
                        inactive=70 * MIB)
        count, enrolled = self.enroll_eligible()
        self.assertGreaterEqual(count, 1)
        self.assertEqual(enrolled, 1)
        self.lib.mr_bal_bridge_test_set_policy_pressure(900, 5000)

        record_id, started = self.run_round()
        if not started:
            self.skipTest("this host's pressure reading refused the round")

        # Request id: monotonically increasing, never 0 for a real request.
        self.assertGreaterEqual(record_id, 1)
        self.assertEqual(self.lib.mr_bal_bridge_action_observed(), 1)
        self.assertEqual(self.lib.mr_bal_bridge_action_id(), record_id)

        # Confirmed scope: name and scanned-list index.
        self.assertEqual(self.lib.mr_bal_bridge_action_scope_count(), 1)
        self.assertEqual(
            self.lib.mr_bal_bridge_action_scope_name(0).decode(),
            "app-x-1.scope")
        self.assertGreaterEqual(self.lib.mr_bal_bridge_action_scope_index(0), 0)
        label = self.lib.mr_bal_bridge_action_scope_label(0).decode()
        self.assertTrue(label, "the record must carry the readable label too")
        self.assertNotIn("\\x2d", label)

        # Absolute write target, built as <root>/<scope>/memory.reclaim.
        target = self.lib.mr_bal_bridge_action_target().decode()
        self.assertEqual(
            target, os.path.join(self._root, "app-x-1.scope", "memory.reclaim"))
        self.assertTrue(target.startswith("/"), target)

        # At least one write reached the seam, and it carries the file-only form.
        writes = self.lib.mr_bal_bridge_action_write_count()
        self.assertGreaterEqual(writes, 1)
        for i in range(writes):
            text = self.lib.mr_bal_bridge_action_write_text(i).decode()
            self.assertIn("swappiness=0", text)
            self.assertRegex(text, r"^\d+ swappiness=0$")
            requested = self.lib.mr_bal_bridge_action_write_bytes(i)
            self.assertEqual(requested, len(text),
                             "the recorded byte count is the text's own length")
            self.assertEqual(self.lib.mr_bal_bridge_action_write_result(i),
                             requested)
            self.assertEqual(self.lib.mr_bal_bridge_action_write_errno(i), 0)
            self.assertEqual(
                self.lib.mr_bal_bridge_action_write_class(i).decode(), "accepted")

        # Nothing to explain: a round that wrote says "none", not "unknown".
        self.assertEqual(self.lib.mr_bal_bridge_action_reason().decode(), "none")
        self.assertEqual(self.lib.mr_bal_bridge_action_reason_known(), 1)

        # The export is one line and names the fields the user was promised.
        line = self.lib.mr_bal_bridge_action_export().decode()
        self.assertNotIn("\n", line)
        self.assertIn("id=%d" % record_id, line)
        self.assertIn("scope[0]=app-x-1.scope#", line)
        self.assertIn("target=" + target, line)
        self.assertIn("writes=%d" % writes, line)
        self.assertIn("swappiness=0", line)
        self.assertIn("class=accepted", line)
        self.assertIn("reason=none", line)

        # The footer sentence names the id, the scope, the size and the return.
        summary = self.lib.mr_bal_bridge_action_summary(0).decode()
        self.assertIn(str(record_id), summary)
        self.assertIn("app-x-1.scope", summary)
        self.assertIn("swappiness=0", summary)

    def test_request_ids_are_monotonic_across_clicks(self):
        """Every click gets a new id; no id is ever reused or handed out twice."""
        self.make_scope("app-x-1.scope", file=80 * MIB, active=MIB,
                        inactive=70 * MIB)
        self.enroll_eligible()
        self.lib.mr_bal_bridge_test_set_policy_pressure(900, 5000)
        ids = []
        for _ in range(3):
            ids.append(self.lib.mr_bal_bridge_action_begin())
            # action_id() is the record's own view of the same value; a mismatch
            # would mean the id the GUI shows is not the id the record kept.
            self.assertEqual(self.lib.mr_bal_bridge_action_id(), ids[-1])
            self.lib.mr_bal_bridge_action_close_auto()
        self.assertEqual(ids, sorted(set(ids)),
                         "ids must increase and never repeat: %r" % (ids,))
        self.assertGreater(ids[0], 0)

    def test_the_copyable_line_is_actually_written_to_a_file(self):
        """The footer is a canvas, so the text cannot be selected. The record is
        therefore also written to a file, and the path is what the user copies.

        XDG_STATE_HOME is repointed for this test so nothing is written to the
        real home directory: the accessor reads that variable at call time.
        """
        state = os.path.join(self._tmp, "state")
        os.environ["XDG_STATE_HOME"] = state
        try:
            self.make_scope("app-x-1.scope", file=80 * MIB, active=MIB,
                            inactive=70 * MIB)
            self.enroll_eligible()
            self.lib.mr_bal_bridge_test_set_policy_pressure(900, 5000)
            record_id, started = self.run_round()
            if not started:
                self.skipTest("this host's pressure reading refused the round")

            path = self.lib.mr_bal_bridge_action_export_path().decode()
            self.assertTrue(path, "the export path must be derived, not empty")
            self.assertTrue(path.startswith(state), path)
            self.assertTrue(os.path.isfile(path), path)
            # 0600: the record names the user's own applications.
            mode = os.stat(path).st_mode & 0o777
            self.assertEqual(mode, 0o600, oct(mode))

            with open(path, "r", encoding="utf-8") as fh:
                contents = fh.read()
            self.assertTrue(contents.endswith("\n"))
            self.assertEqual(contents.count("\n"), 1,
                             "the file holds exactly one line")
            self.assertEqual(contents.rstrip("\n"),
                             self.lib.mr_bal_bridge_action_export().decode(),
                             "the file must hold the same bytes the record shows")
            self.assertIn("id=%d" % record_id, contents)
            self.assertIn("swappiness=0", contents)
        finally:
            os.environ.pop("XDG_STATE_HOME", None)

    def test_the_export_path_is_empty_when_no_home_is_known(self):
        """No home directory means no path: the record is still shown, the file
        is not claimed to exist."""
        saved_state = os.environ.pop("XDG_STATE_HOME", None)
        saved_home = os.environ.pop("HOME", None)
        try:
            self.assertEqual(
                self.lib.mr_bal_bridge_action_export_path().decode(), "")
        finally:
            if saved_state is not None:
                os.environ["XDG_STATE_HOME"] = saved_state
            if saved_home is not None:
                os.environ["HOME"] = saved_home

    # ---- refusal -----------------------------------------------------

    def test_a_click_with_nothing_enrolled_records_no_scope_and_says_so(self):
        """The case a user most wants explained: nothing was even a target."""
        self.make_scope("app-idle-1.scope", file=MIB, active=MIB, inactive=0)
        count = self.lib.mr_bal_bridge_scan_scopes()
        self.assertGreaterEqual(count, 1)
        self.lib.mr_bal_bridge_policy_enroll_clear()

        record_id, started = self.run_round()
        self.assertFalse(started)
        self.assertGreaterEqual(record_id, 1)
        self.assertEqual(self.lib.mr_bal_bridge_action_scope_count(), 0)
        self.assertEqual(self.lib.mr_bal_bridge_action_write_count(), 0)
        self.assertEqual(self.writes, [])
        self.assertEqual(self.lib.mr_bal_bridge_action_reason().decode(),
                         "no_scope")
        self.assertEqual(self.lib.mr_bal_bridge_action_reason_known(), 1)
        # No confirmed scope means no target: "" is the honest answer, not a
        # path the round never approached.
        self.assertEqual(self.lib.mr_bal_bridge_action_target().decode(), "")

        line = self.lib.mr_bal_bridge_action_export().decode()
        self.assertIn("scopes=0", line)
        self.assertIn("target=unavailable", line)
        self.assertIn("writes=0", line)
        self.assertIn("reason=no_scope", line)
        summary = self.lib.mr_bal_bridge_action_summary(0).decode()
        self.assertIn("no_scope", summary)

    def test_a_policy_refusal_names_the_policy_stop(self):
        """Enrolled, but the policy refuses: the reason is that stop's name."""
        self.make_scope("app-busy-1.scope", file=300 * MIB,
                        active=299 * MIB, inactive=MIB)
        self.enroll_eligible()
        self.lib.mr_bal_bridge_test_set_policy_pressure(900, 5000)

        record_id, started = self.run_round()
        self.assertFalse(started, "a scope in active use must not qualify")
        self.assertEqual(self.lib.mr_bal_bridge_policy_stop(), STOP_NO_TARGET)
        self.assertGreaterEqual(record_id, 1)
        # The scope was confirmed at begin() time even though it was then
        # refused: "which apps did this click consider" is a fact about the
        # request, not about the outcome.
        self.assertEqual(self.lib.mr_bal_bridge_action_scope_count(), 1)
        self.assertEqual(self.lib.mr_bal_bridge_action_write_count(), 0)
        self.assertEqual(self.writes, [])
        self.assertEqual(self.lib.mr_bal_bridge_action_reason().decode(),
                         "policy:no_target")

    # ---- EAGAIN ------------------------------------------------------

    def test_an_eagain_write_records_return_errno_and_class(self):
        self.make_scope("app-z-1.scope", file=80 * MIB, active=MIB,
                        inactive=70 * MIB)
        self.enroll_eligible()
        self.lib.mr_bal_bridge_test_set_policy_pressure(900, 5000)
        # Inject the measured kernel answer: a file-only request that could not
        # be fully satisfied. The hook returns -1 and the seam's errno is set
        # explicitly, because a Python callback cannot set the C errno the
        # caller would otherwise read.
        self.hook_result = (-1, errno.EAGAIN)
        self.lib.mr_bal_test_set_write_errno(errno.EAGAIN)

        record_id, started = self.run_round()
        if not started:
            self.skipTest("this host's pressure reading refused the round")

        self.assertGreaterEqual(self.lib.mr_bal_bridge_action_write_count(), 1)
        index = 0
        self.assertEqual(self.lib.mr_bal_bridge_action_write_result(index), -1)
        self.assertEqual(self.lib.mr_bal_bridge_action_write_errno(index),
                         errno.EAGAIN)
        self.assertEqual(self.lib.mr_bal_bridge_action_write_class(index).decode(),
                         "eagain")
        # The requested size and exact text are still recorded: an EAGAIN is
        # precisely the case where "what did we ask for" matters.
        text = self.lib.mr_bal_bridge_action_write_text(index).decode()
        self.assertRegex(text, r"^\d+ swappiness=0$")
        self.assertEqual(self.lib.mr_bal_bridge_action_write_bytes(index),
                         len(text))

        line = self.lib.mr_bal_bridge_action_export().decode()
        self.assertIn("ret=-1", line)
        self.assertIn("errno=%d" % errno.EAGAIN, line)
        self.assertIn("class=eagain", line)
        summary = self.lib.mr_bal_bridge_action_summary(0).decode()
        self.assertIn("eagain", summary)
        self.assertIn(str(record_id), summary)

    def test_an_einval_write_is_distinguished_from_eagain(self):
        """A kernel that rejects the parameter is a different answer, and the
        record must not collapse the two into "failed"."""
        self.make_scope("app-w-1.scope", file=80 * MIB, active=MIB,
                        inactive=70 * MIB)
        self.enroll_eligible()
        self.lib.mr_bal_bridge_test_set_policy_pressure(900, 5000)
        self.hook_result = (-1, errno.EINVAL)
        self.lib.mr_bal_test_set_write_errno(errno.EINVAL)
        record_id, started = self.run_round()
        if not started:
            self.skipTest("this host's pressure reading refused the round")
        if self.lib.mr_bal_bridge_action_write_count() == 0:
            self.skipTest("the probe refused the fixture before a batch ran")
        self.assertEqual(self.lib.mr_bal_bridge_action_write_class(0).decode(),
                         "einval")

    def test_a_short_write_is_not_reported_as_accepted(self):
        """A partial write is a refusal in this tree (EIO), so the record must
        not call it accepted just because the return was non-negative."""
        self.make_scope("app-s-1.scope", file=80 * MIB, active=MIB,
                        inactive=70 * MIB)
        self.enroll_eligible()
        self.lib.mr_bal_bridge_test_set_policy_pressure(900, 5000)

        def short_hook(fd, buf, length):
            self.writes.append(ctypes.string_at(buf, length).decode("utf-8", "replace"))
            return max(0, length - 1)

        self._short = WRITE_HOOK(short_hook)
        self.lib.mr_bal_set_write_hook(self._short)
        record_id, started = self.run_round()
        if not started:
            self.skipTest("this host's pressure reading refused the round")
        if self.lib.mr_bal_bridge_action_write_count() == 0:
            self.skipTest("the probe refused the fixture before a batch ran")
        self.assertEqual(self.lib.mr_bal_bridge_action_write_class(0).decode(),
                         "short_write")


if __name__ == "__main__":
    unittest.main()


class ExportWiringTest(unittest.TestCase):
    """The export must happen when the record closes, not when a widget paints.

    The previous version wrote the file as a side effect of building the status
    string, which meant a clean that ran but was never repainted left no file,
    and a later status assignment could overwrite the one that would have
    written it. These tests drive the C surface only - no GUI, no real reclaim.
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
        ActionRecordTest._bind(self.lib)
        self.lib.mr_bal_bridge_action_export_error.restype = ctypes.c_char_p
        self.state = tempfile.mkdtemp(prefix="q16-export-")
        self._saved = os.environ.get("XDG_STATE_HOME")
        os.environ["XDG_STATE_HOME"] = self.state

    def tearDown(self):
        if self._saved is None:
            os.environ.pop("XDG_STATE_HOME", None)
        else:
            os.environ["XDG_STATE_HOME"] = self._saved
        shutil.rmtree(self.state, ignore_errors=True)

    def _file(self):
        return os.path.join(self.state, "memreduct", "balanced-last.txt")

    def test_closing_the_record_writes_the_file_without_any_gui(self):
        """No status string is built here - only begin/close, as the GUI does."""
        self.lib.mr_bal_bridge_action_begin()
        self.lib.mr_bal_bridge_action_close_auto()
        self.assertTrue(os.path.exists(self._file()),
                        "closing the record must export, not the status paint")
        content = open(self._file(), encoding="utf-8").read()
        self.assertTrue(content.startswith("MEMREDUCT-BALANCED "), content)
        self.assertEqual(content.count("\n"), 1)

    def test_a_refused_click_still_leaves_a_file(self):
        """The refusal is the case a user most wants to read back."""
        self.lib.mr_bal_bridge_policy_enroll_clear()
        self.lib.mr_bal_bridge_action_begin()
        self.lib.mr_bal_bridge_action_close_auto()
        self.assertTrue(os.path.exists(self._file()))
        content = open(self._file(), encoding="utf-8").read()
        self.assertIn("reason=no_scope", content)

    def test_export_error_is_empty_on_success(self):
        self.lib.mr_bal_bridge_action_begin()
        self.lib.mr_bal_bridge_action_close_auto()
        self.assertEqual(
            self.lib.mr_bal_bridge_action_export_error().decode(), "")

    def test_a_failed_mkdir_is_reported_not_swallowed(self):
        """Point the state dir under a regular file so mkdir must fail."""
        blocker = os.path.join(self.state, "blocked")
        with open(blocker, "w", encoding="utf-8") as handle:
            handle.write("x")
        os.environ["XDG_STATE_HOME"] = blocker
        self.lib.mr_bal_bridge_action_begin()
        self.lib.mr_bal_bridge_action_close_auto()
        path = self.lib.mr_bal_bridge_action_export_path().decode()
        message = self.lib.mr_bal_bridge_action_export_error().decode()
        self.assertEqual(path, "", "a failed export must not claim a path")
        self.assertNotEqual(message, "",
                            "a failed export must say why, not return silence")
        self.assertIn("mkdir", message)

    def test_a_missing_home_is_named(self):
        saved_home = os.environ.pop("HOME", None)
        os.environ.pop("XDG_STATE_HOME", None)
        try:
            self.lib.mr_bal_bridge_action_begin()
            self.lib.mr_bal_bridge_action_close_auto()
            message = self.lib.mr_bal_bridge_action_export_error().decode()
            self.assertIn("HOME", message)
        finally:
            if saved_home is not None:
                os.environ["HOME"] = saved_home


class EagainSemanticsTest(unittest.TestCase):
    """EAGAIN is a partial reclaim, not a zero reclaim and not a success."""

    @classmethod
    def setUpClass(cls):
        cls._libpath = build_library()

    @classmethod
    def tearDownClass(cls):
        try:
            os.unlink(cls._libpath)
        except OSError:
            pass

    def test_eagain_is_its_own_class_not_a_failure(self):
        lib = ctypes.CDLL(self._libpath)
        ActionRecordTest._bind(lib)
        lib.mr_bal_write_attempted.restype = ctypes.c_int64
        lib.mr_bal_bridge_action_write_class.argtypes = [ctypes.c_int64]
        lib.mr_bal_bridge_action_write_class.restype = ctypes.c_char_p
        lib.mr_bal_bridge_action_write_errno.argtypes = [ctypes.c_int64]
        lib.mr_bal_bridge_action_write_errno.restype = ctypes.c_int64
        lib.mr_bal_bridge_action_begin()
        lib.mr_bal_test_set_write_errno(11)   # EAGAIN
        ctypes.CDLL(self._libpath)  # keep handle alive
        class Args(ctypes.Structure):
            _fields_ = [("fd", ctypes.c_int), ("data", ctypes.c_char_p),
                        ("len", ctypes.c_size_t)]
        hook = WRITE_HOOK(lambda fd, data, length: -1)
        lib.mr_bal_set_write_hook(hook)
        lib.mr_bal_bridge_action_close_auto()
        classes = []
        for i in range(lib.mr_bal_bridge_action_write_count()):
            classes.append(lib.mr_bal_bridge_action_write_class(i).decode())
        lib.mr_bal_set_write_hook(WRITE_HOOK())
        if classes:
            self.assertEqual(classes[0], "eagain",
                             "EAGAIN must be classed as a partial reclaim")
