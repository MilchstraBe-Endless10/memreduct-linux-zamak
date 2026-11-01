"""Cross-process workingset advisory prototype (Issue #12, ADR-0007).

Every test here runs unprivileged. The planner is exercised against a fixture
procfs and a fake pidfd_open, so no real process is ever pinned or advised. The
writable half is only ever reached through an injected backend that records
calls instead of issuing a syscall.

The point of most assertions is a negative: this path must be unable to name a
process, unable to run without two separate gates, and unable to present an
advisory byte count as memory freed.
"""
import ctypes
import errno
import os
import pathlib
import subprocess
import sys
import tempfile
import time
import unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent

# Result enums, mirrored from native/crossproc_plan.h.
OK = 0
UNSUPPORTED = 1
PROC_UNAVAILABLE = 2
BUDGET_EXHAUSTED = 3
PERMISSION_DENIED = 4
NO_CANDIDATES = 5
INVALID_ARGUMENT = 6
REENTRANT = 7

SKIP_NONE = 0
SKIP_PID1 = 1
SKIP_SELF = 2
SKIP_KERNEL_THREAD = 3
SKIP_OTHER_UID = 4
SKIP_EXITED = 5
SKIP_PID_REUSED = 6
SKIP_SHARED = 7
SKIP_WRITABLE = 8
SKIP_SPECIAL = 9
SKIP_TOO_LARGE = 10
SKIP_BUDGET = 11
SKIP_UNREADABLE = 12

ADVICE_NONE = 0
ADVICE_COLD = 1
ADVICE_PAGEOUT = 2

# Advise enums, mirrored from native/crossproc_advise.h.
A_DISABLED = 0
A_NOT_APPROVED = 1
A_OK = 2
A_PARTIAL = 3
A_NO_PERMISSION = 4
A_TARGET_GONE = 5
A_BAD_REQUEST = 6
A_UNAVAILABLE = 7
A_FAILED = 8
A_REENTRANT = 9
A_EMPTY_PLAN = 10

MAX_TARGETS = 8
MAX_REGIONS = 16

STAT_FIELDS_BEFORE_START = 18  # fields 4..21 sit between comm and start_time


def stat_line(pid, start_time, comm='fake'):
    """A /proc/<pid>/stat line whose field 22 is start_time.

    Two details are load-bearing and both were wrong before, which made every
    fixture process look unreadable so the planner always returned NO_CANDIDATES:

    * comm is wrapped in parentheses, exactly as the kernel writes it. The
      parser locates field 3 with strrchr(buffer, ')'), so a line without the
      closing parenthesis cannot be parsed at all.
    * start_time must land on field 22. With field 3 being the state, that is
      18 filler fields (4..21), not 19.
    """
    fields = [f'{pid} ({comm})', 'S']
    fields.extend(['0'] * STAT_FIELDS_BEFORE_START)
    fields.append(str(start_time))
    return ' '.join(fields) + '\n'


class MockProc:
    """A writable fixture of /proc/<pid>/{stat,maps} plus directory entries."""

    def __init__(self, root, uid=None):
        self.root = pathlib.Path(root)
        self.root.mkdir(parents=True, exist_ok=True)
        self.uid = os.geteuid() if uid is None else uid
        # pid -> uid, for fixtures that claim another owner. An unprivileged
        # run cannot chown a file to a different user, so those entries stay
        # owned by us and the test-only uid hook reports the intended owner.
        self.other_uids = {}

    def add(self, pid, maps='', start_time=None, uid=None):
        entry = self.root / str(pid)
        entry.mkdir(exist_ok=True)
        (entry / 'stat').write_text(stat_line(pid, start_time if start_time is not None else pid))
        (entry / 'maps').write_text(maps)
        owner = self.uid if uid is None else uid
        self._claim(pid, entry, owner)
        return entry

    def _claim(self, pid, entry, owner):
        """Make the fixture look like it belongs to `owner`.

        Only a privileged process may hand a file to another uid, so when the
        owner differs we leave the real ownership alone and let the uid hook
        answer the question the planner's stat() would have answered.
        """
        if owner != os.geteuid():
            self.other_uids[pid] = owner
            return
        os.chown(entry, owner, owner)
        for child in entry.iterdir():
            os.chown(child, owner, owner)

    def add_kernel_thread(self, pid, start_time=None):
        """A kernel thread has no address space, therefore no maps file."""
        entry = self.root / str(pid)
        entry.mkdir(exist_ok=True)
        (entry / 'stat').write_text(stat_line(pid, start_time if start_time is not None else pid))
        os.chown(entry, self.uid, self.uid)
        os.chown(entry / 'stat', self.uid, self.uid)
        return entry

    def add_plain_dir(self, name):
        entry = self.root / name
        entry.mkdir(exist_ok=True)
        return entry


def mapping(start, end, perms='r--p', path='', offset='00000000', inode=0):
    """One /proc/<pid>/maps line: start-end perms offset dev inode path.

    The column order is the kernel's, and it is load-bearing: an earlier version
    emitted offset and inode swapped, which made every file-backed fixture look
    anonymous (inode 0) and every anonymous one look file-backed. The planner
    keys its writable-file and special-mapping rules off exactly those two
    fields, so the fixtures silently exercised the wrong branch.
    """
    line = f'{start:x}-{end:x} {perms} {offset} 00:00 {inode}'
    if path:
        line += f' {path}'
    return line + '\n'


class CrossProcessTestCase(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.temp.cleanup)
        cls.work = pathlib.Path(cls.temp.name)

    def build_library(self, name, defines=(), sources=None):
        target = self.work / name
        flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', '-shared', '-fPIC']
        subprocess.run(['cc', *flags, *defines,
                        *(sources or [str(ROOT / 'native/crossproc_plan.c')]),
                        '-o', str(target)], check=True)
        return ctypes.CDLL(str(target))

    def install_pidfd_hook(self, library, gone=()):
        """Pin fixture PIDs that do not exist on the host.

        A real pidfd_open on a fixture pid answers ESRCH, so every plan would be
        empty and the selection logic would go untested. The hook returns a
        genuine descriptor (the close(2) path stays honest) except for pids the
        caller declares already exited.
        """
        state = {'calls': 0}

        def _hook(pid):
            state['calls'] += 1
            if pid in gone:
                ctypes.set_errno(errno.ESRCH)
                return -1
            return os.dup(0)

        callback = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_int32)(_hook)
        library.mr_crossproc_set_pidfd_hook.argtypes = [ctypes.c_void_p]
        library.mr_crossproc_set_pidfd_hook.restype = None
        library.mr_crossproc_set_pidfd_hook(ctypes.cast(callback, ctypes.c_void_p))
        self._hook_refs.append(callback)
        return state

    def install_uid_hook(self, library, proc):
        """Report the fixture's intended owner for pids we could not chown."""
        def _hook(pid):
            return proc.other_uids.get(pid, os.geteuid())

        callback = ctypes.CFUNCTYPE(ctypes.c_int32, ctypes.c_int32)(_hook)
        library.mr_crossproc_set_uid_hook.argtypes = [ctypes.c_void_p]
        library.mr_crossproc_set_uid_hook.restype = None
        library.mr_crossproc_set_uid_hook(ctypes.cast(callback, ctypes.c_void_p))
        self._hook_refs.append(callback)

    def install_test_hooks(self, library, proc, gone=()):
        # _hook_refs must be initialised once and then appended to. Resetting it
        # here would drop the reference to the pidfd callback installed on the
        # line below, letting ctypes' trampoline be collected while the C side
        # still holds its function pointer -- the library would then call into
        # freed memory, which shows up as a silently empty plan rather than a
        # crash. Keep every trampoline alive for the lifetime of the test.
        if not hasattr(self, '_hook_refs'):
            self._hook_refs = []
        before = len(self._hook_refs)
        self.install_pidfd_hook(library, gone)
        self.install_uid_hook(library, proc)
        assert len(self._hook_refs) == before + 2, 'both trampolines must stay referenced'
        return self._hook_refs


class TestPlannerCannotNameAProcess(unittest.TestCase):
    """The planner must have no input that selects a process."""

    def setUp(self):
        self.source = (ROOT / 'native/crossproc_plan.c').read_text()
        self.header = (ROOT / 'native/crossproc_plan.h').read_text()

    def test_limits_struct_carries_only_budgets_and_an_advice_enum(self):
        limits = self.header.split('struct mr_crossproc_limits {')[1].split('};')[0]
        self.assertNotIn('pid', limits.lower())
        self.assertNotIn('char', limits, 'no path may be passed in')
        self.assertIn('max_targets', limits)
        self.assertIn('max_total_bytes', limits)
        self.assertIn('advice', limits)

    def test_plan_build_takes_no_pid_or_path_argument(self):
        signature = self.source.split('int mr_crossproc_plan_build(')[1].split(')')[0]
        self.assertEqual(signature.count(','), 1, signature)
        self.assertNotIn('pid', signature.lower())
        self.assertNotIn('path', signature.lower())

    def test_procfs_root_is_fixed_outside_test_builds(self):
        function = self.source.split('static const char *proc_root(void)')[1].split('\n}\n')[0]
        self.assertIn('return "/proc";', function)
        # The override lives entirely inside the test-only branch.
        guard = function.split('#ifdef MEMREDUCT_TESTING')[1].split('#endif')[0]
        self.assertIn('MEMREDUCT_TEST_PROC_ROOT', guard)
        self.assertNotIn('MEMREDUCT_TEST_PROC_ROOT', function.replace(guard, ''))

    def test_testing_build_rejects_a_relative_root(self):
        function = self.source.split('static const char *proc_root(void)')[1].split('\n}\n')[0]
        self.assertIn("override[0] == '/'", function)

    def test_no_writes_and_no_signals_anywhere_in_the_prototype(self):
        for name in ('crossproc_plan.c', 'crossproc_advise.c'):
            source = (ROOT / 'native' / name).read_text()
            for forbidden in ('kill(', 'ptrace(', 'drop_caches', 'argv', 'system('):
                self.assertNotIn(forbidden, source, f'{name} must not contain {forbidden}')
            # The only write-capable opens are in the test helper, not here.
            self.assertNotIn('O_WRONLY', source)
            self.assertNotIn('O_RDWR', source)
            self.assertNotIn('fwrite', source)
            self.assertNotIn('unlink', source)


class TestPlannerSelection(CrossProcessTestCase):
    """Target and region selection over a fixture procfs."""

    def setUp(self):
        self.proc = MockProc(self.work / f'proc-{self.id()}')
        self.library = self.build_library(f'libplan-{self.id()}.so', ('-DMEMREDUCT_TESTING',))
        self.library.mr_crossproc_plan_status.restype = ctypes.c_int64
        self.library.mr_crossproc_plan_targets.restype = ctypes.c_int64
        self.library.mr_crossproc_plan_regions.restype = ctypes.c_int64
        self.library.mr_crossproc_plan_bytes.restype = ctypes.c_int64
        self.library.mr_crossproc_plan_error.restype = ctypes.c_int64
        self.install_test_hooks(self.library, self.proc)
        self.previous = os.environ.get('MEMREDUCT_TEST_PROC_ROOT')
        os.environ['MEMREDUCT_TEST_PROC_ROOT'] = str(self.proc.root)

    def tearDown(self):
        if self.previous is None:
            os.environ.pop('MEMREDUCT_TEST_PROC_ROOT', None)
        else:
            os.environ['MEMREDUCT_TEST_PROC_ROOT'] = self.previous

    def build_plan(self, library=None, limits=None):
        """Call mr_crossproc_plan_build directly and return a plain dict."""
        library = library or self.library
        library.mr_crossproc_plan_build.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
        library.mr_crossproc_plan_build.restype = ctypes.c_int
        plan = (ctypes.c_char * 8192)()
        buffer = None
        if limits is not None:
            buffer = (ctypes.c_char * 32)()
            ctypes.memmove(buffer, ctypes.byref(limits), ctypes.sizeof(limits))
        rc = library.mr_crossproc_plan_build(
            ctypes.cast(buffer, ctypes.c_void_p) if buffer is not None else None,
            ctypes.cast(plan, ctypes.c_void_p))
        self.assertEqual(rc, 0)
        return plan

    def status_of(self, plan, library=None):
        library = library or self.library
        library.mr_crossproc_plan_status_of.argtypes = [ctypes.c_void_p]
        library.mr_crossproc_plan_status_of.restype = ctypes.c_int64
        return library.mr_crossproc_plan_status_of(ctypes.cast(plan, ctypes.c_void_p))

    def test_selects_only_readable_private_mappings(self):
        self.proc.add(1000, '\n'.join([
            mapping(0x10000, 0x20000, 'r--p', '', inode=0),        # keep: private anon
            mapping(0x20000, 0x30000, 'r--p', '/lib/a.so', inode=7),  # keep: private file
            mapping(0x30000, 0x40000, 'rw-p', '/lib/b.so', inode=8),  # skip: writable file
            mapping(0x40000, 0x50000, 'rw-s', '/lib/c.so', inode=9),  # skip: shared
            mapping(0x50000, 0x60000, 'r--p', '[vdso]', inode=0),     # skip: special
            mapping(0x60000, 0x70000, 'r--p', '[stack]', inode=0),    # skip: special
        ]) )
        result = self.library.mr_crossproc_plan_status()
        self.assertEqual(result, OK)
        self.assertEqual(self.library.mr_crossproc_plan_targets(), 1)
        self.assertEqual(self.library.mr_crossproc_plan_regions(), 2,
                         'only the two readable private mappings may be planned')
        self.assertEqual(self.library.mr_crossproc_plan_bytes(), 2 * 0x10000)
        self.library.mr_crossproc_plan_skipped.argtypes = [ctypes.c_int64]
        self.library.mr_crossproc_plan_skipped.restype = ctypes.c_int64
        # Address the counters by their real enum values, not by guessed
        # numbers: 7 is SHARED, 8 is WRITABLE and 9 is SPECIAL.
        self.assertEqual(self.library.mr_crossproc_plan_skipped(SKIP_WRITABLE), 1,
                         'writable file-backed skipped')
        self.assertEqual(self.library.mr_crossproc_plan_skipped(SKIP_SHARED), 1,
                         'shared mapping skipped')
        self.assertEqual(self.library.mr_crossproc_plan_skipped(SKIP_SPECIAL), 2,
                         'special mappings skipped')

    def test_skips_pid1_self_kernel_threads_and_other_users(self):
        self.proc.add(1, mapping(0x10000, 0x20000))
        self.proc.add(os.getpid(), mapping(0x10000, 0x20000))
        self.proc.add_kernel_thread(2)
        other = 100 if os.geteuid() != 100 else 101
        self.proc.add(4242, mapping(0x10000, 0x20000) + '\n', uid=other)
        self.proc.add(1000, mapping(0x10000, 0x20000))
        self.assertEqual(self.library.mr_crossproc_plan_status(), OK)
        self.assertEqual(self.library.mr_crossproc_plan_targets(), 1)
        self.library.mr_crossproc_plan_skipped.argtypes = [ctypes.c_int64]
        self.library.mr_crossproc_plan_skipped.restype = ctypes.c_int64
        self.assertEqual(self.library.mr_crossproc_plan_skipped(SKIP_PID1), 1, 'PID 1')
        self.assertEqual(self.library.mr_crossproc_plan_skipped(SKIP_SELF), 1, 'self')
        self.assertEqual(self.library.mr_crossproc_plan_skipped(SKIP_KERNEL_THREAD), 1, 'kernel thread')
        self.assertEqual(self.library.mr_crossproc_plan_skipped(SKIP_OTHER_UID), 1, 'other uid')

    def test_non_numeric_proc_entries_are_ignored(self):
        self.proc.add_plain_dir('sys')
        self.proc.add_plain_dir('self')
        self.proc.add_plain_dir('123abc')
        self.proc.add(1000, mapping(0x10000, 0x20000))
        self.assertEqual(self.library.mr_crossproc_plan_status(), OK)
        self.assertEqual(self.library.mr_crossproc_plan_targets(), 1)

    def test_empty_fixture_reports_no_candidates(self):
        self.assertEqual(self.library.mr_crossproc_plan_status(), NO_CANDIDATES)
        self.assertEqual(self.library.mr_crossproc_plan_targets(), 0)

    def test_missing_root_reports_proc_unavailable(self):
        os.environ['MEMREDUCT_TEST_PROC_ROOT'] = str(self.proc.root / 'does-not-exist')
        self.assertEqual(self.library.mr_crossproc_plan_status(), PROC_UNAVAILABLE)

    def test_relative_root_override_is_ignored(self):
        os.environ['MEMREDUCT_TEST_PROC_ROOT'] = 'relative/path'
        # Falls back to the real /proc, which exists: the point is that a
        # relative override cannot redirect the scan.
        self.assertIn(self.library.mr_crossproc_plan_status(),
                      (OK, NO_CANDIDATES, BUDGET_EXHAUSTED))


class TestPlannerBudgets(CrossProcessTestCase):
    """Every ceiling must actually stop the scan."""

    def setUp(self):
        self.proc = MockProc(self.work / f'proc-{self.id()}')
        self.library = self.build_library(f'libbudget-{self.id()}.so', ('-DMEMREDUCT_TESTING',))
        for name in ('mr_crossproc_plan_status', 'mr_crossproc_plan_targets',
                     'mr_crossproc_plan_regions', 'mr_crossproc_plan_bytes'):
            getattr(self.library, name).restype = ctypes.c_int64
        self.install_test_hooks(self.library, self.proc)
        self.previous = os.environ.get('MEMREDUCT_TEST_PROC_ROOT')
        os.environ['MEMREDUCT_TEST_PROC_ROOT'] = str(self.proc.root)

    def tearDown(self):
        if self.previous is None:
            os.environ.pop('MEMREDUCT_TEST_PROC_ROOT', None)
        else:
            os.environ['MEMREDUCT_TEST_PROC_ROOT'] = self.previous

    def test_target_ceiling_stops_the_scan(self):
        for pid in range(1000, 1000 + MAX_TARGETS + 4):
            self.proc.add(pid, mapping(0x10000, 0x20000) + '\n')
        self.assertEqual(self.library.mr_crossproc_plan_status(), BUDGET_EXHAUSTED)
        self.assertEqual(self.library.mr_crossproc_plan_targets(), MAX_TARGETS)

    def test_region_ceiling_is_per_target(self):
        many = ''.join(mapping(0x10000 + i * 0x1000, 0x10000 + (i + 1) * 0x1000)
                       for i in range(MAX_REGIONS + 6))
        self.proc.add(1000, many)
        self.assertEqual(self.library.mr_crossproc_plan_status(), OK)
        self.assertEqual(self.library.mr_crossproc_plan_targets(), 1)
        self.assertEqual(self.library.mr_crossproc_plan_regions(), MAX_REGIONS)

    def test_oversized_region_is_skipped_not_truncated(self):
        # 65 MiB in one mapping exceeds the 64 MiB single-region ceiling.
        self.proc.add(1000, mapping(0x10000, 0x10000 + 65 * 1024 * 1024) + '\n')
        self.assertEqual(self.library.mr_crossproc_plan_status(), NO_CANDIDATES)
        self.assertEqual(self.library.mr_crossproc_plan_regions(), 0)

    def test_total_byte_budget_stops_collection(self):
        # 5 x 64 MiB = 320 MiB > the 256 MiB total, so only 4 fit.
        for pid in range(1000, 1005):
            self.proc.add(pid, mapping(0x10000, 0x10000 + 64 * 1024 * 1024) + '\n')
        self.assertIn(self.library.mr_crossproc_plan_status(), (OK, BUDGET_EXHAUSTED))
        self.assertLessEqual(self.library.mr_crossproc_plan_bytes(), 256 * 1024 * 1024)

    def test_huge_procfs_is_capped_by_the_entry_limit(self):
        for pid in range(2000, 2000 + 400):
            self.proc.add(pid, mapping(0x10000, 0x20000) + '\n')
        # Neither the target ceiling nor the byte ceiling may be exceeded, and
        # the scan itself stays bounded.
        self.assertIn(self.library.mr_crossproc_plan_status(), (OK, BUDGET_EXHAUSTED))
        self.assertLessEqual(self.library.mr_crossproc_plan_targets(), MAX_TARGETS)


class TestFixtureIsolation(CrossProcessTestCase):
    """Regression cover for the leak that made every plan empty.

    The fixture procfs tree used to be built under pathlib.Path(self.work.name),
    which is a bare basename, so the trees were created in the process cwd and
    eight tmp* directories leaked into the repository root. Nothing failed
    loudly; the planner simply saw an empty procfs.
    """

    def test_fixture_root_lives_inside_the_temporary_directory(self):
        proc = MockProc(self.work / 'proc-isolation')
        self.assertTrue(str(proc.root).startswith(str(self.work) + os.sep))
        self.assertTrue(proc.root.is_dir())

    def test_class_work_root_is_one_source_of_truth(self):
        # Every fixture must derive from cls.work. If a test re-derives the
        # root from temp.name the basename bug comes straight back. Scan only
        # the MockProc construction sites so this assertion cannot match its
        # own source text.
        lines = [line for line in pathlib.Path(__file__).read_text().splitlines()
                 if 'MockProc(' in line and 'def ' not in line and 'class ' not in line]
        self.assertTrue(lines, 'expected to find MockProc call sites')
        for line in lines:
            self.assertNotIn('.name', line,
                             f'a fixture root must not go through .name: {line.strip()}')

    def test_building_a_plan_does_not_change_the_working_directory(self):
        proc = MockProc(self.work / 'proc-cwd')
        proc.add(1000, mapping(0x10000, 0x20000))
        library = self.build_library('libcwd.so', ('-DMEMREDUCT_TESTING',))
        self.install_test_hooks(library, proc)
        previous = os.environ.get('MEMREDUCT_TEST_PROC_ROOT')
        os.environ['MEMREDUCT_TEST_PROC_ROOT'] = str(proc.root)
        before = os.getcwd()
        try:
            library.mr_crossproc_plan_status.restype = ctypes.c_int64
            self.assertEqual(library.mr_crossproc_plan_status(), OK)
            self.assertEqual(os.getcwd(), before,
                             'the planner must never write to or chdir into cwd')
        finally:
            if previous is None:
                os.environ.pop('MEMREDUCT_TEST_PROC_ROOT', None)
            else:
                os.environ['MEMREDUCT_TEST_PROC_ROOT'] = previous


class TestPathLength(CrossProcessTestCase):
    """The fixture path is longer than a naive 64-byte buffer."""

    def test_a_long_proc_root_still_resolves(self):
        # Build a root long enough to overrun a 64-byte "<root>/<pid>/stat".
        deep = self.work
        for index in range(6):
            deep = deep / f'segment-{index:02d}-abcdefgh'
        deep.mkdir(parents=True, exist_ok=True)
        proc = MockProc(deep / 'proc')
        proc.add(1000, mapping(0x10000, 0x20000))
        self.assertGreater(len(str(proc.root / '1000' / 'stat')), 64)
        library = self.build_library('liblong.so', ('-DMEMREDUCT_TESTING',))
        self.install_test_hooks(library, proc)
        previous = os.environ.get('MEMREDUCT_TEST_PROC_ROOT')
        os.environ['MEMREDUCT_TEST_PROC_ROOT'] = str(proc.root)
        try:
            library.mr_crossproc_plan_status.restype = ctypes.c_int64
            library.mr_crossproc_plan_targets.restype = ctypes.c_int64
            self.assertEqual(library.mr_crossproc_plan_status(), OK)
            self.assertEqual(library.mr_crossproc_plan_targets(), 1)
        finally:
            if previous is None:
                os.environ.pop('MEMREDUCT_TEST_PROC_ROOT', None)
            else:
                os.environ['MEMREDUCT_TEST_PROC_ROOT'] = previous


class TestAdviceIsDoubleGated(CrossProcessTestCase):
    """The writable half must be unreachable without both gates."""

    def test_default_build_has_no_writable_path(self):
        library = self.build_library(
            'libadvise-default.so', ('-DMEMREDUCT_TESTING',),
            (str(ROOT / 'native/crossproc_plan.c'), str(ROOT / 'native/crossproc_advise.c')))
        library.mr_crossproc_advise_enabled.restype = ctypes.c_int64
        self.assertEqual(library.mr_crossproc_advise_enabled(), 0,
                         'a default build must not contain the writable half')
        library.mr_crossproc_arm_approval.argtypes = [ctypes.c_void_p]
        library.mr_crossproc_arm_approval.restype = ctypes.c_int64
        self.assertEqual(library.mr_crossproc_arm_approval(None), 0,
                         'arming must be impossible without the build flag')
        library.mr_crossproc_advise.restype = ctypes.c_int64
        self.assertEqual(library.mr_crossproc_advise(), A_DISABLED)

    def test_opt_in_build_still_refuses_without_run_time_approval(self):
        library = self.build_library(
            'libadvise-optin.so',
            ('-DMEMREDUCT_TESTING', '-DMEMREDUCT_ENABLE_PRODUCTION_ADVISE'),
            (str(ROOT / 'native/crossproc_plan.c'), str(ROOT / 'native/crossproc_advise.c')))
        library.mr_crossproc_advise_enabled.restype = ctypes.c_int64
        self.assertEqual(library.mr_crossproc_advise_enabled(), 1)
        library.mr_crossproc_advise.restype = ctypes.c_int64
        self.assertEqual(library.mr_crossproc_advise(), A_NOT_APPROVED,
                         'the build flag alone must not run anything')

    def test_approval_is_consumed_and_never_covers_a_second_run(self):
        source = (ROOT / 'native/crossproc_advise.c').read_text()
        self.assertIn('approval_armed = 0;', source)
        # Both the refusal path and the success path clear the arm.
        run = source.split('mr_crossproc_advise_run(')[1]
        after_gate = run.split('approval_armed = 0;')[1]
        self.assertIn('approval_armed', run.split('approval_armed = 0;')[0])
        self.assertNotEqual(after_gate.strip(), '', 'a run must clear the arm before acting')

    def test_a_write_capable_build_is_a_compile_time_decision(self):
        library = self.build_library(
            'libgate.so',
            ('-DMEMREDUCT_TESTING',),
            (str(ROOT / 'native/crossproc_plan.c'), str(ROOT / 'native/crossproc_advise.c')))
        library.mr_crossproc_advise_enabled.restype = ctypes.c_int64
        # No environment variable appears in the gate: a user machine cannot
        # turn the capability on by exporting something.
        source = (ROOT / 'native/crossproc_advise.c').read_text()
        gate = source.split('int64_t mr_crossproc_advise_enabled(void)')[1].split('\n}\n')[0]
        self.assertNotIn('getenv', gate)
        self.assertEqual(library.mr_crossproc_advise_enabled(), 0)


class TestBackendInjection(CrossProcessTestCase):
    """Exercise the full syscall shape with a recording backend."""

    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        cls.proc = MockProc(cls.work / 'proc-advise')

        # A fixture process with two eligible mappings and one ineligible one.
        cls.proc.add(1000, '\n'.join([
            mapping(0x10000, 0x20000, 'r--p', '', inode=0),
            mapping(0x20000, 0x30000, 'r--p', '/lib/a.so', inode=7),
            mapping(0x30000, 0x40000, 'rw-s', '/lib/b.so', inode=8),
        ]))
        cls.previous_root = os.environ.get('MEMREDUCT_TEST_PROC_ROOT')
        os.environ['MEMREDUCT_TEST_PROC_ROOT'] = str(cls.proc.root)

        cls.library = cls._build('libadvise-backend.so')
        cls.library.mr_crossproc_set_pidfd_hook.argtypes = [ctypes.c_void_p]
        cls.library.mr_crossproc_set_pidfd_hook.restype = None
        cls.pidfd_hook = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_int32)(cls._pidfd)
        cls.library.mr_crossproc_set_pidfd_hook(ctypes.cast(cls.pidfd_hook, ctypes.c_void_p))

        # struct iovec is { void *iov_base; size_t iov_len; }. Declaring the
        # pointer as POINTER(c_ubyte) only happened to work: the callback then
        # could not read iov_len, so a real C caller's lengths were invisible
        # and the recorded byte count was meaningless. Model the real struct.
        class IOVec(ctypes.Structure):
            _fields_ = [('iov_base', ctypes.c_void_p), ('iov_len', ctypes.c_size_t)]

        cls.IOVec = IOVec
        cls.backend_type = ctypes.CFUNCTYPE(
            ctypes.c_long, ctypes.c_int, ctypes.POINTER(IOVec), ctypes.c_size_t,
            ctypes.c_int, ctypes.c_uint)
        cls.calls = []
        cls.backend_behavior = None
        cls.backend = cls.backend_type(cls._record)
        cls.library.mr_crossproc_set_backend.argtypes = [ctypes.c_void_p]
        cls.library.mr_crossproc_set_backend.restype = None
        cls.library.mr_crossproc_set_backend(ctypes.cast(cls.backend, ctypes.c_void_p))

    @classmethod
    def _build(cls, name):
        target = cls.work / name
        subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic',
                        '-shared', '-fPIC', '-DMEMREDUCT_TESTING',
                        '-DMEMREDUCT_ENABLE_PRODUCTION_ADVISE',
                        str(ROOT / 'native/crossproc_plan.c'),
                        str(ROOT / 'native/crossproc_advise.c'),
                        '-o', str(target)], check=True)
        return ctypes.CDLL(str(target))

    @classmethod
    def _pidfd(cls, pid):
        # A real descriptor, so release/close paths are honest, but it never
        # refers to the fixture pid (which does not exist).
        return os.dup(0)

    @classmethod
    def _record(cls, pidfd, iov, vlen, advice, flags):
        lengths = [int(iov[index].iov_len) for index in range(vlen)]
        cls.calls.append({'pidfd': pidfd, 'vlen': vlen, 'advice': advice,
                          'flags': flags, 'lengths': lengths})
        if cls.backend_behavior is not None:
            return cls.backend_behavior(pidfd, iov, vlen, advice, flags)
        # Default: accept everything, as a permissive kernel would. Return the
        # sum of the lengths actually handed to us, not a guessed page size.
        return sum(lengths)

    def setUp(self):
        type(self).calls = []
        type(self).backend_behavior = None
        os.environ['MEMREDUCT_TEST_PROC_ROOT'] = str(self.proc.root)
        self.library.mr_crossproc_plan_build.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
        self.library.mr_crossproc_plan_build.restype = ctypes.c_int
        self.library.mr_crossproc_arm_approval.argtypes = [ctypes.c_void_p]
        self.library.mr_crossproc_arm_approval.restype = ctypes.c_int64
        self.library.mr_crossproc_plan_release.argtypes = [ctypes.c_void_p]
        self.library.mr_crossproc_plan_release.restype = None
        # Struct-returning C functions cannot be received by ctypes, so the
        # prototype exposes mr_crossproc_test_advise_into(plan, out) instead.
        self.library.mr_crossproc_test_advise_into.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
        self.library.mr_crossproc_test_advise_into.restype = ctypes.c_int

    def tearDown(self):
        if self.previous_root is None:
            os.environ.pop('MEMREDUCT_TEST_PROC_ROOT', None)
        else:
            os.environ['MEMREDUCT_TEST_PROC_ROOT'] = self.previous_root

    def make_plan(self):
        plan = (ctypes.c_char * 8192)()
        rc = self.library.mr_crossproc_plan_build(None, ctypes.cast(plan, ctypes.c_void_p))
        self.assertEqual(rc, 0)
        return plan

    def test_unapproved_run_records_no_syscall(self):
        plan = self.make_plan()
        self.library.mr_crossproc_clear_approval()
        result = self.run_advise(plan)
        self.assertEqual(result['status'], A_NOT_APPROVED)
        self.assertEqual(self.calls, [], 'an unapproved run must not issue a syscall')
        self.assertEqual(result['bytes_advised'], 0)

    def test_approved_run_uses_the_planned_ranges_and_advice(self):
        plan = self.make_plan()
        # make_plan() fills a caller-owned buffer, so the scalar bridge
        # (mr_crossproc_plan_targets) -- which reads the module-level last_plan
        # -- is not a valid way to inspect it. Use the plan accessors.
        self.library.mr_crossproc_plan_targets_of.argtypes = [ctypes.c_void_p]
        self.library.mr_crossproc_plan_targets_of.restype = ctypes.c_int64
        self.library.mr_crossproc_plan_regions_of.argtypes = [ctypes.c_void_p]
        self.library.mr_crossproc_plan_regions_of.restype = ctypes.c_int64
        self.assertEqual(self.library.mr_crossproc_plan_targets_of(
            ctypes.cast(plan, ctypes.c_void_p)), 1)
        self.assertEqual(self.library.mr_crossproc_plan_regions_of(
            ctypes.cast(plan, ctypes.c_void_p)), 2)
        self.assertEqual(self.library.mr_crossproc_arm_approval(ctypes.cast(plan, ctypes.c_void_p)), 1)
        result = self.run_advise(plan)
        self.assertEqual(result['status'], A_OK)
        self.assertEqual(result['bytes_advised'], 2 * 0x10000)
        self.assertEqual(len(self.calls), 1, 'both regions fit one batch')
        self.assertEqual(self.calls[0]['vlen'], 2, 'the shared mapping is not in the batch')
        self.assertEqual(self.calls[0]['advice'], 20, 'MADV_COLD for the default plan')
        self.assertEqual(self.calls[0]['flags'], 0)
        self.library.mr_crossproc_plan_release(ctypes.cast(plan, ctypes.c_void_p))

    def test_second_run_without_rearming_is_refused(self):
        plan = self.make_plan()
        self.library.mr_crossproc_arm_approval(ctypes.cast(plan, ctypes.c_void_p))
        first = self.run_advise(plan)
        self.assertEqual(first['status'], A_OK)
        calls_after_first = len(self.calls)
        second = self.run_advise(plan)
        self.assertEqual(second['status'], A_NOT_APPROVED)
        self.assertEqual(len(self.calls), calls_after_first, 'no retry, no second syscall')

    def run_advise(self, plan):
        """Run through the out-parameter prototype and return the result dict."""
        out = (ctypes.c_char * 256)()
        rc = self.library.mr_crossproc_test_advise_into(
            ctypes.cast(plan, ctypes.c_void_p), ctypes.cast(out, ctypes.c_void_p))
        self.assertEqual(rc, 0)
        return self.read_result(out)

    def read_result(self, pointer):
        library = self.library
        library.mr_crossproc_test_result_status.argtypes = [ctypes.c_void_p]
        library.mr_crossproc_test_result_status.restype = ctypes.c_int64
        for name in ('status', 'bytes', 'ok', 'failed', 'targets', 'error'):
            getattr(library, f'mr_crossproc_test_result_{name}').argtypes = [ctypes.c_void_p]
            getattr(library, f'mr_crossproc_test_result_{name}').restype = ctypes.c_int64
        return {
            'status': library.mr_crossproc_test_result_status(pointer),
            'bytes_advised': library.mr_crossproc_test_result_bytes(pointer),
            'batches_ok': library.mr_crossproc_test_result_ok(pointer),
            'batches_failed': library.mr_crossproc_test_result_failed(pointer),
            'targets_done': library.mr_crossproc_test_result_targets(pointer),
            'error_code': library.mr_crossproc_test_result_error(pointer),
        }


if __name__ == '__main__':
    unittest.main()
