"""Tests no privileged kernel operations and never starts a real pkexec."""
import ctypes
import os
import pwd
import pathlib
import subprocess
import sys
import tempfile
import time
import unittest
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parent.parent


class TestNativeBackend(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.work = pathlib.Path(self.temp.name)
        self.helper = self.work / 'memreduct-zamak-helper'
        subprocess.run(['cc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-pedantic',
                        str(ROOT / 'native/memreduct_helper.c'), '-o', str(self.helper)], check=True)
        self.test_helper = self.work / 'memreduct-zamak-helper-test'
        subprocess.run(['cc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-pedantic',
                        '-DMEMREDUCT_TESTING', str(ROOT / 'native/memreduct_helper.c'),
                        str(ROOT / 'tests/native_write_shim.c'), '-Wl,--wrap=write', '-Wl,--wrap=close',
                        '-o', str(self.test_helper)], check=True)
        self.bridge = self.work / 'libnative.so'
        subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', '-shared', '-fPIC', '-DMEMREDUCT_TESTING',
                        str(ROOT / 'native/async_privileged.c'), str(ROOT / 'tests/native_waitpid_shim.c'),
                        '-Wl,--wrap=waitpid', '-o', str(self.bridge)], check=True)

    def test_helper_dry_run_and_reject_unknown_mode(self):
        result = subprocess.run([self.helper, '--dry-run'], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0)
        self.assertIn('no kernel', result.stdout)
        for action in ('delete-files', 'metadata', 'all'):
            result = subprocess.run([self.helper, action], capture_output=True, text=True)
            self.assertEqual(result.returncode, 2)

    def test_unprivileged_cannot_touch_kernel(self):
        target = self.work / 'drop-caches-test-target'
        target.write_bytes(b'unchanged')
        env = os.environ.copy()
        env['MEMREDUCT_TEST_DROP_CACHES_PATH'] = str(target)
        preexec = None
        if os.geteuid() == 0:
            # Test a normal unprivileged account even when CI runs as root.
            self.work.chmod(0o755)
            nobody = pwd.getpwnam('nobody')
            preexec = lambda: (os.setgid(nobody.pw_gid), os.setuid(nobody.pw_uid))
        result = subprocess.run([self.helper, 'pagecache'], capture_output=True, text=True,
                                env=env, preexec_fn=preexec)
        self.assertEqual(result.returncode, 1)
        self.assertIn('root privileges', result.stderr)
        self.assertEqual(target.read_bytes(), b'unchanged')

    def _run_test_helper(self, action, target, fault=None):
        env = os.environ.copy()
        env['MEMREDUCT_TEST_DROP_CACHES_PATH'] = str(target)
        if fault:
            env['MEMREDUCT_TEST_WRITE_FAULT'] = fault
        else:
            env.pop('MEMREDUCT_TEST_WRITE_FAULT', None)
        return subprocess.run([self.test_helper, action], capture_output=True, text=True, env=env)

    def test_test_helper_writes_only_pagecache_value_to_regular_file(self):
        target = self.work / 'pagecache.target'
        target.write_bytes(b'')
        result = self._run_test_helper('pagecache', target)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('reclaimed bytes are not measured', result.stdout)
        self.assertEqual(target.read_bytes(), b'1\n')

    def test_test_helper_rejects_metadata_and_all_without_writing(self):
        for action in ('metadata', 'all'):
            with self.subTest(action=action):
                target = self.work / f'{action}.target'
                target.write_bytes(b'unchanged')
                result = self._run_test_helper(action, target)
                self.assertEqual(result.returncode, 2)
                self.assertEqual(target.read_bytes(), b'unchanged')

    def test_test_helper_retries_eintr_once(self):
        target = self.work / 'eintr.target'
        target.write_bytes(b'')
        result = self._run_test_helper('pagecache', target, 'eintr-once')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(target.read_bytes(), b'1\n')

    def test_test_helper_does_not_retry_other_write_errors(self):
        target = self.work / 'write-eio.target'
        target.write_bytes(b'unchanged')
        result = self._run_test_helper('pagecache', target, 'write-eio-once')
        self.assertEqual(result.returncode, 1)
        self.assertIn('Input/output error', result.stderr)
        self.assertEqual(target.read_bytes(), b'unchanged')

    def test_test_helper_reports_partial_write_as_uncertain_without_retry(self):
        target = self.work / 'short-write.target'
        target.write_bytes(b'')
        result = self._run_test_helper('pagecache', target, 'short-once')
        self.assertEqual(result.returncode, 3)
        self.assertIn('outcome is uncertain', result.stderr)
        self.assertIn('do not retry automatically', result.stderr)
        self.assertEqual(target.read_bytes(), b'1')

    def test_test_helper_rejects_symlink_target(self):
        target = self.work / 'real.target'
        target.write_bytes(b'unchanged')
        symlink = self.work / 'drop-caches-symlink'
        symlink.symlink_to(target)
        result = self._run_test_helper('pagecache', symlink)
        self.assertEqual(result.returncode, 1)
        self.assertEqual(target.read_bytes(), b'unchanged')

    def test_test_helper_rejects_non_regular_fifo_target(self):
        target = self.work / 'fifo.target'
        os.mkfifo(target)
        reader_fd = os.open(target, os.O_RDONLY | os.O_NONBLOCK)
        try:
            result = self._run_test_helper('pagecache', target)
        finally:
            os.close(reader_fd)
        self.assertEqual(result.returncode, 1)
        self.assertIn('not a regular file', result.stderr)

    def test_test_helper_reports_close_error_as_uncertain(self):
        target = self.work / 'close-error.target'
        target.write_bytes(b'')
        result = self._run_test_helper('pagecache', target, 'close-fail-once')
        self.assertEqual(result.returncode, 3)
        self.assertIn('outcome is uncertain', result.stderr)
        self.assertIn('close=failed', result.stderr)
        self.assertEqual(target.read_bytes(), b'1\n')

    def _write_fake_pkexec(self):
        fake = self.work / 'pkexec'
        fake.write_text(
            '#!/bin/sh\n'
            '[ "$1" = "/usr/libexec/memreduct-zamak-helper" ] || exit 88\n'
            '[ "$2" = "pagecache" ] || exit 89\n'
            'if [ -n "${MEMREDUCT_TEST_PKEXEC_LOG:-}" ]; then echo called >> "$MEMREDUCT_TEST_PKEXEC_LOG"; fi\n'
            'case "${MEMREDUCT_TEST_PKEXEC_BEHAVIOR:-helper}" in\n'
            '  cancel) exit 126 ;;\n'
            '  pkexec-fail) exit 127 ;;\n'
            '  helper-unsupported) exit 2 ;;\n'
            '  signal) kill -TERM "$$" ;;\n'
            '  gated) while [ ! -e "$MEMREDUCT_TEST_PKEXEC_RELEASE" ]; do sleep 0.01; done ;;\n'
            'esac\n'
            'exec "$MEMREDUCT_TEST_HELPER" "$2"\n'
        )
        fake.chmod(0o755)
        return fake

    def _load_bridge(self):
        library = ctypes.CDLL(str(self.bridge))
        library.mr_start_reclaim.argtypes = [ctypes.c_int64]
        library.mr_start_reclaim.restype = ctypes.c_int64
        library.mr_poll_reclaim.restype = ctypes.c_int64
        return library

    def _bridge_environment(self, fake, behavior='helper', target=None, calls=None, release=None, fault=None):
        return {
            'MEMREDUCT_TEST_PKEXEC': str(fake),
            'MEMREDUCT_TEST_HELPER': str(self.test_helper),
            'MEMREDUCT_TEST_PKEXEC_BEHAVIOR': behavior,
            'MEMREDUCT_TEST_PKEXEC_LOG': str(calls) if calls else '',
            'MEMREDUCT_TEST_PKEXEC_RELEASE': str(release) if release else '',
            'MEMREDUCT_TEST_DROP_CACHES_PATH': str(target) if target else '',
            'MEMREDUCT_TEST_WRITE_FAULT': fault or '',
        }

    @staticmethod
    def _restore_environment(previous):
        for name, value in previous.items():
            if value is None:
                os.environ.pop(name, None)
            else:
                os.environ[name] = value

    def _poll_bridge(self, library):
        for _ in range(300):
            result = library.mr_poll_reclaim()
            if result != -1:
                return result
            time.sleep(0.01)
        self.fail('bridge child did not finish within 3 seconds')

    def _build_bridge(self, name, gate='default'):
        """Compile the bridge plus one gate translation unit.

        The capability switch is a translation-unit choice in the real build
        (zam's [native] table has no defines field), so the test mirrors that
        exactly: 'default' links production_clean_gate_default.c (enabled -> 0)
        and 'enabled' links production_clean_gate_enabled.c (enabled -> 1).
        A binary therefore either contains the reachable bridge or it does
        not, and this test is what proves both halves of that claim.
        """
        gate_file = {
            'default': 'native/production_clean_gate_default.c',
            'enabled': 'native/production_clean_gate_enabled.c',
        }[gate]
        target = self.work / name
        subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', '-shared', '-fPIC',
                        '-DMEMREDUCT_TESTING'] +
                       [str(ROOT / 'native/async_privileged.c'), str(ROOT / gate_file),
                        str(ROOT / 'tests/native_waitpid_shim.c'),
                        '-Wl,--wrap=waitpid', '-o', str(target)], check=True)
        library = ctypes.CDLL(str(target))
        library.mr_production_clean_enabled.restype = ctypes.c_int64
        return library

    def test_production_clean_is_a_compile_time_gate(self):
        off = self._build_bridge('libnative-gate-off.so', gate='default')
        self.assertEqual(off.mr_production_clean_enabled(), 0,
                         'default builds must not expose the privileged path')
        on = self._build_bridge('libnative-gate-on.so', gate='enabled')
        self.assertEqual(on.mr_production_clean_enabled(), 1)
        # The opt-in build compiles the bridge in, but it still refuses to do
        # anything unless the packaged helper path is actually installed.
        with mock.patch.dict(os.environ, {}, clear=False):
            start = on.mr_start_reclaim
            start.argtypes = [ctypes.c_int64]
            start.restype = ctypes.c_int64
            self.assertEqual(start(1), self._start_state('MR_RECLAIM_UNSUPPORTED_ACTION'))
            os.environ['MEMREDUCT_TEST_PKEXEC'] = str(self.work / 'missing-pkexec')
            self.assertEqual(start(0), self._start_state('MR_RECLAIM_LAUNCH_FAILED'))

    @staticmethod
    def _start_state(name):
        return {'MR_RECLAIM_STARTED': 0, 'MR_RECLAIM_BUSY': 1,
                'MR_RECLAIM_UNSUPPORTED_ACTION': 2, 'MR_RECLAIM_LAUNCH_FAILED': 3}[name]

    def test_bridge_nonblocking_busy_and_invalid_action(self):
        fake = self._write_fake_pkexec()
        release = self.work / 'release-child'
        calls = self.work / 'calls.log'
        target = self.work / 'busy.target'
        target.write_bytes(b'')
        previous = {name: os.environ.get(name) for name in (
            'MEMREDUCT_TEST_PKEXEC', 'MEMREDUCT_TEST_HELPER', 'MEMREDUCT_TEST_PKEXEC_BEHAVIOR',
            'MEMREDUCT_TEST_PKEXEC_LOG', 'MEMREDUCT_TEST_PKEXEC_RELEASE',
            'MEMREDUCT_TEST_DROP_CACHES_PATH', 'MEMREDUCT_TEST_WRITE_FAULT')}
        try:
            os.environ.update(self._bridge_environment(fake, 'gated', target=target, calls=calls, release=release))
            library = self._load_bridge()
            self.assertEqual(library.mr_poll_reclaim(), -2)
            self.assertEqual(library.mr_start_reclaim(99), 2)
            self.assertEqual(library.mr_start_reclaim(1), 2)
            self.assertEqual(library.mr_start_reclaim(0), 0)
            self.assertEqual(library.mr_start_reclaim(0), 1)
            self.assertEqual(library.mr_poll_reclaim(), -1)
            release.touch()
            self.assertEqual(self._poll_bridge(library), 0)
            self.assertEqual(library.mr_poll_reclaim(), -2)
            self.assertEqual(len(calls.read_text().splitlines()), 1)
        finally:
            self._restore_environment(previous)

    def test_bridge_runs_fake_helper_and_normalizes_complete_result_mapping(self):
        fake = self._write_fake_pkexec()
        calls = self.work / 'calls.log'
        library = self._load_bridge()
        env_names = (
            'MEMREDUCT_TEST_PKEXEC', 'MEMREDUCT_TEST_HELPER', 'MEMREDUCT_TEST_PKEXEC_BEHAVIOR',
            'MEMREDUCT_TEST_PKEXEC_LOG', 'MEMREDUCT_TEST_PKEXEC_RELEASE',
            'MEMREDUCT_TEST_DROP_CACHES_PATH', 'MEMREDUCT_TEST_WRITE_FAULT',
        )
        previous = {name: os.environ.get(name) for name in env_names}
        scenarios = (
            ('success', 'helper', b'', None, 0, b'1\n'),
            # pkexec forwards the child's status, so a bare 126 cannot prove the
            # user cancelled: it is reported as unattributed, never as "cancelled".
            ('unattributed 126 from pkexec or child', 'cancel', b'', None, 5, b''),
            ('unattributed 127 from pkexec or child', 'pkexec-fail', b'', None, 5, b''),
            ('helper failed', 'helper', b'unchanged', 'write-eio-once', 3, b'unchanged'),
            ('helper reported uncertain result', 'helper', b'', 'short-once', 5, b'1'),
            ('helper rejected action', 'helper-unsupported', b'', None, 4, b''),
        )
        try:
            for index, (label, behavior, initial, fault, expected, final) in enumerate(scenarios):
                with self.subTest(label=label):
                    target = self.work / f'bridge-{index}.target'
                    target.write_bytes(initial)
                    os.environ.update(self._bridge_environment(fake, behavior, target, calls, fault=fault))
                    self.assertEqual(library.mr_start_reclaim(0), 0)
                    self.assertEqual(self._poll_bridge(library), expected)
                    self.assertEqual(library.mr_poll_reclaim(), -2)
                    self.assertEqual(target.read_bytes(), final)
            self.assertEqual(len(calls.read_text().splitlines()), len(scenarios))
        finally:
            self._restore_environment(previous)

    def test_exit_codes_126_and_127_need_child_evidence_to_be_attributed(self):
        """pkexec forwards the child status, so the number alone proves nothing.

        Without evidence the parent must say "unattributed"; only a child that
        reported its own failure turns the same number into a helper failure.
        """
        fake = self._write_fake_pkexec()
        calls = self.work / 'calls.log'
        library = self._load_bridge()
        env_names = (
            'MEMREDUCT_TEST_PKEXEC', 'MEMREDUCT_TEST_HELPER', 'MEMREDUCT_TEST_PKEXEC_BEHAVIOR',
            'MEMREDUCT_TEST_PKEXEC_LOG', 'MEMREDUCT_TEST_PKEXEC_RELEASE',
            'MEMREDUCT_TEST_DROP_CACHES_PATH', 'MEMREDUCT_TEST_WRITE_FAULT',
        )
        previous = {name: os.environ.get(name) for name in env_names}
        try:
            for behavior in ('cancel', 'pkexec-fail'):
                with self.subTest(behavior=behavior, reported=False):
                    target = self.work / f'unattributed-{behavior}.target'
                    target.write_bytes(b'')
                    os.environ.update(self._bridge_environment(fake, behavior, target, calls))
                    self.assertEqual(library.mr_start_reclaim(0), 0)
                    # 5 is OUTCOME_UNCERTAIN, not a cancellation or auth verdict.
                    self.assertEqual(self._poll_bridge(library), 5)

                with self.subTest(behavior=behavior, reported=True):
                    target = self.work / f'attributed-{behavior}.target'
                    target.write_bytes(b'')
                    os.environ.update(self._bridge_environment(fake, behavior, target, calls))
                    self.assertEqual(library.mr_start_reclaim(0), 0)
                    # The child told the parent it failed, so the number is explained.
                    self.assertEqual(library.mr_reclaim_child_reported_failure(1), 1)
                    self.assertEqual(self._poll_bridge(library), 3)

            # Reporting a stage for a child that is not running must do nothing.
            self.assertEqual(library.mr_reclaim_child_reported_failure(1), 0)
        finally:
            for name, value in previous.items():
                if value is None:
                    os.environ.pop(name, None)
                else:
                    os.environ[name] = value

    def test_bridge_reports_spawn_wait_and_abnormal_child_failures(self):
        fake = self._write_fake_pkexec()
        library = self._load_bridge()
        target = self.work / 'wait-failure.target'
        target.write_bytes(b'')
        env_names = (
            'MEMREDUCT_TEST_PKEXEC', 'MEMREDUCT_TEST_HELPER', 'MEMREDUCT_TEST_PKEXEC_BEHAVIOR',
            'MEMREDUCT_TEST_PKEXEC_LOG', 'MEMREDUCT_TEST_PKEXEC_RELEASE',
            'MEMREDUCT_TEST_DROP_CACHES_PATH', 'MEMREDUCT_TEST_WRITE_FAULT',
            'MEMREDUCT_TEST_WAITPID_FAIL_ONCE',
        )
        previous = {name: os.environ.get(name) for name in env_names}
        try:
            os.environ.update(self._bridge_environment(fake, 'signal', target=target))
            os.environ['MEMREDUCT_TEST_PKEXEC'] = str(self.work / 'does-not-exist')
            self.assertEqual(library.mr_start_reclaim(0), 3)

            os.environ['MEMREDUCT_TEST_PKEXEC'] = str(fake)
            os.environ['MEMREDUCT_TEST_PKEXEC_BEHAVIOR'] = 'signal'
            self.assertEqual(library.mr_start_reclaim(0), 0)
            self.assertEqual(self._poll_bridge(library), 6)

            os.environ['MEMREDUCT_TEST_PKEXEC_BEHAVIOR'] = 'helper'
            os.environ['MEMREDUCT_TEST_WAITPID_FAIL_ONCE'] = '1'
            self.assertEqual(library.mr_start_reclaim(0), 0)
            self.assertEqual(self._poll_bridge(library), 7)
        finally:
            self._restore_environment(previous)


class TestGuiTestBridge(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.temp.cleanup)
        cls.library_path = pathlib.Path(cls.temp.name) / 'libgui-test.so'
        gtk_flags = subprocess.run(
            ['pkg-config', '--cflags', '--libs', 'gtk+-3.0'],
            check=True, capture_output=True, text=True,
        ).stdout.split()
        subprocess.run(
            ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic',
             '-shared', '-fPIC', str(ROOT / 'native/gui_test.c'), *gtk_flags,
             '-o', str(cls.library_path)],
            check=True,
        )

    def test_sample_failure_trigger_is_private_ci_only_and_one_shot(self):
        token = pathlib.Path(self.temp.name) / 'trigger.token'
        token.write_text('P2B_FAIL_SAMPLE_ONCE\n')
        token.chmod(0o600)
        malformed = pathlib.Path(self.temp.name) / 'malformed.token'
        malformed.write_text('fail later\n')
        malformed.chmod(0o600)
        public = pathlib.Path(self.temp.name) / 'public.token'
        public.write_text('P2B_FAIL_SAMPLE_ONCE\n')
        public.chmod(0o644)
        symlink = pathlib.Path(self.temp.name) / 'trigger-link.token'
        symlink.symlink_to(token)
        fifo = pathlib.Path(self.temp.name) / 'trigger.fifo'
        os.mkfifo(fifo, 0o600)
        missing = pathlib.Path(self.temp.name) / 'missing.token'
        probe = (
            'import ctypes,sys; '
            'lib=ctypes.CDLL(sys.argv[1]); '
            'lib.mr_gui_test_sample_failure_requested.restype=ctypes.c_longlong; '
            'lib.mr_gui_test_consume_sample_failure_trigger.restype=ctypes.c_longlong; '
            'print(lib.mr_gui_test_sample_failure_requested(), '
            'lib.mr_gui_test_consume_sample_failure_trigger(), '
            'lib.mr_gui_test_sample_failure_requested(), '
            'lib.mr_gui_test_consume_sample_failure_trigger())'
        )

        def run_probe(path, enabled=True):
            env = os.environ.copy()
            env['MEMREDUCT_CI_SAMPLE_FAILURE_TRIGGER_FILE'] = str(path)
            if enabled:
                env['MEMREDUCT_CI'] = '1'
            else:
                env.pop('MEMREDUCT_CI', None)
            result = subprocess.run(
                [sys.executable, '-c', probe, str(self.library_path)],
                check=True, capture_output=True, text=True, env=env,
            )
            return tuple(int(value) for value in result.stdout.split())

        self.assertEqual(run_probe(token, enabled=False), (0, 0, 0, 0))
        self.assertEqual(run_probe(missing), (0, 0, 0, 0))
        self.assertEqual(run_probe(malformed), (0, 0, 0, 0))
        self.assertEqual(run_probe(public), (0, 0, 0, 0))
        self.assertEqual(run_probe(symlink), (0, 0, 0, 0))
        self.assertEqual(run_probe(fifo), (0, 0, 0, 0))
        self.assertEqual(run_probe(token), (1, 1, 0, 0))

    def test_widget_preview_environment_is_ci_gated_and_allowlisted(self):
        library = ctypes.CDLL(str(self.library_path))
        library.mr_gui_test_widget_preview_resource.restype = ctypes.c_longlong
        names = ('MEMREDUCT_CI', 'MEMREDUCT_CI_WIDGET_PREVIEW')
        previous = {name: os.environ.get(name) for name in names}
        try:
            os.environ['MEMREDUCT_CI_WIDGET_PREVIEW'] = 'checkbox-136'
            os.environ.pop('MEMREDUCT_CI', None)
            self.assertEqual(library.mr_gui_test_widget_preview_resource(), 0)

            os.environ['MEMREDUCT_CI'] = '1'
            for value, expected in (
                ('checkbox-116', 116),
                ('checkbox-136', 136),
                ('checkbox-125', 0),
                ('checkbox-153', 0),
                ('checkbox-999', 0),
            ):
                with self.subTest(value=value):
                    os.environ['MEMREDUCT_CI_WIDGET_PREVIEW'] = value
                    self.assertEqual(library.mr_gui_test_widget_preview_resource(), expected)
        finally:
            for name, value in previous.items():
                if value is None:
                    os.environ.pop(name, None)
                else:
                    os.environ[name] = value


if __name__ == '__main__':
    unittest.main()


class TestConfirmationWording(unittest.TestCase):
    """The production warning must not borrow the simulation's harmless wording.

    The simulated path says nothing changes; the production path must state the
    cost of the action and must not promise an amount, because the kernel only
    accepts a request. Both live in the same file, so the check reads the source.
    """

    @classmethod
    def setUpClass(cls):
        cls.source = (ROOT / 'src/main.zm').read_text()

    def _dialog_block(self):
        start = self.source.index('fn draw_clean_confirmation')
        end = self.source.index('fn draw(s: &mut MainWindow', start)
        return self.source[start:end]

    def test_simulation_and_production_use_distinct_text(self):
        block = self._dialog_block()
        self.assertIn('Test-only simulated clean', block)
        self.assertIn('仅测试模拟清理', block)
        # The production title must not be the test wording.
        self.assertIn('"Clean page cache", "清理页缓存"', block)

    def test_production_warning_states_cost_and_promises_nothing(self):
        block = self._dialog_block()
        # The warning must keep both kernel-documented costs (slower re-reads,
        # no guaranteed amount) while fitting the fixed panel: the measured
        # line widths come back in CI_CLEAN_CONFIRM_TEXT and the E2E run fails
        # on fits=false, which is what forced the shorter wording here.
        self.assertIn('Re-reads slower; no amount guaranteed.', block)
        self.assertIn('重读变慢；不保证释放量。', block)
        self.assertIn('仅丢弃干净页缓存。', block)

    def test_production_warning_does_not_reuse_simulation_sentences(self):
        block = self._dialog_block()
        # Each simulation sentence must appear only on the simulation branch.
        for sentence in ('No system memory is changed.', '不会改动系统内存。'):
            self.assertEqual(block.count(sentence), 1, sentence)

    def test_authorization_cancel_and_failure_have_distinct_feedback(self):
        # The fixed helper cannot exit 126/127: those pkexec outcomes identify
        # dismissal vs failure to authorize/start the fixed helper.
        self.assertIn('"authorization-cancelled"', self.source)
        self.assertIn('"authorization-failed"', self.source)
        self.assertIn('Authorization was cancelled; no helper result was received', self.source)
        self.assertIn('Polkit could not authorize or start the fixed helper', self.source)
        self.assertIn('不会自动重试', self.source)


class TestCiMarkerOutputPath(unittest.TestCase):
    """CI markers must reach an unbuffered sink, or a killed run loses them all.

    Regression for the P3B defect where every `src/main.zm` marker went to the
    buffered stdout of the Zamak runtime: the process is normally terminated by
    its bounded test timer, so buffered markers never reached any capture file
    and startup evidence such as CI_PRODUCTION_START was invisible.
    """

    def test_main_markers_use_the_unbuffered_native_emitter(self):
        source = (ROOT / 'src/main.zm').read_text()
        self.assertIn('extern fn mr_ci_emit(line: &string)', source)
        leaked = [
            line.strip()
            for line in source.splitlines()
            if 'println(' in line and 'CI_' in line
        ]
        self.assertEqual(leaked, [], 'CI markers must not use buffered println')
        self.assertNotIn('println(', source)

    def test_emitter_is_silent_without_ci_and_flushes_with_it(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        library_path = pathlib.Path(temp.name) / 'libemit.so'
        gtk_flags = subprocess.run(
            ['pkg-config', '--cflags', '--libs', 'gtk+-3.0'],
            check=True, capture_output=True, text=True,
        ).stdout.split()
        subprocess.run(
            ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic',
             '-shared', '-fPIC', str(ROOT / 'native/gui_test.c'), *gtk_flags,
             '-o', str(library_path)],
            check=True,
        )
        # The emitter writes to the process-level stderr the C runtime owns, so
        # the assertion has to observe a real child process, not sys.stderr.
        script = (
            'import ctypes, sys\n'
            'emit = ctypes.CDLL(sys.argv[1]).mr_ci_emit\n'
            'emit.argtypes = [ctypes.c_char_p]\n'
            'emit.restype = None\n'
            'emit(b"CI_PRODUCTION_START")\n'
        )
        quiet_env = {k: v for k, v in os.environ.items() if k != 'MEMREDUCT_CI'}
        quiet = subprocess.run(
            [sys.executable, '-c', script, str(library_path)],
            capture_output=True, text=True, env=quiet_env, check=True,
        )
        self.assertEqual(quiet.stderr, '')
        self.assertEqual(quiet.stdout, '')

        loud_env = dict(quiet_env, MEMREDUCT_CI='1')
        loud = subprocess.run(
            [sys.executable, '-c', script, str(library_path)],
            capture_output=True, text=True, env=loud_env, check=True,
        )
        self.assertEqual(loud.stdout, '')
        self.assertEqual(loud.stderr, 'CI_PRODUCTION_START\n')


class TestSelfReclaim(unittest.TestCase):
    """ADR-0006 action 2, self mode: page out this process only, no privilege.

    The security property is that this path cannot reach another process. It has
    no pid argument at all; the C side always opens a pidfd for getpid(). These
    tests pin that shape so a later "optimisation" cannot add a target.
    """

    @classmethod
    def setUpClass(cls):
        cls.source = (ROOT / 'native/self_reclaim.c').read_text()

    def test_it_has_no_way_to_name_another_process(self):
        source = self.source
        # The only pid that may ever be opened is the caller's own.
        self.assertIn('pidfd_open, (pid_t)getpid()', source)
        self.assertNotIn('argv', source, 'the reclaim path must take no arguments')
        self.assertNotIn('getenv', source, 'no environment variable may redirect it')
        # Every process_madvise call uses that same pidfd.
        calls = source.count('__NR_process_madvise')
        uses_own_pidfd = source.count('syscall(__NR_process_madvise, pidfd,')
        self.assertGreaterEqual(calls, uses_own_pidfd + 1)
        self.assertEqual(uses_own_pidfd, 2, 'both batch and tail call the same pidfd')

    def test_it_skips_shared_and_unreadable_mappings(self):
        # Shared mappings belong to other processes; pushing them out from under
        # those processes is exactly what this action must never do.
        self.assertIn("if (perms[0] != 'r' || perms[3] != 'p')", self.source)

    def test_it_reports_an_amount_it_measured_itself(self):
        # bytes_paged_out is the length the kernel accepted, so the caller must
        # be able to tell it apart from "memory freed". The wording test below
        # pins the user-visible half; this pins the field.
        self.assertIn('bytes_paged_out', self.source)
        self.assertIn('failed_batches', self.source)

    def test_gui_wording_does_not_claim_system_wide_effect(self):
        gui = (ROOT / 'src/main.zm').read_text()
        self.assertIn('Trim this app', gui)
        self.assertIn("释放本程序内存", gui)
        # The self action must not be described as touching other processes.
        self.assertNotIn('Trim all processes', gui.split('let names_en')[1].split(']')[0])

    def test_marker_states_that_it_cannot_touch_other_processes(self):
        gui = (ROOT / 'src/main.zm').read_text()
        self.assertIn('target=self only other_processes=false no_privilege=true', gui)

    def test_runs_and_only_ever_affects_itself(self):
        """Drive the real function and check the outcome shape."""
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        library_path = pathlib.Path(temp.name) / 'libself.so'
        subprocess.run(
            ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic',
             '-shared', '-fPIC', str(ROOT / 'native/self_reclaim.c'),
             '-o', str(library_path)],
            check=True,
        )
        library = ctypes.CDLL(str(library_path))
        library.mr_self_reclaim.restype = ctypes.c_int64
        library.mr_self_reclaim_bytes.restype = ctypes.c_int64
        library.mr_self_reclaim_failed_batches.restype = ctypes.c_int64
        library.mr_self_reclaim_error.restype = ctypes.c_int64

        status = library.mr_self_reclaim()
        self.assertIn(status, (0, 1), 'either it worked or the kernel lacks the syscall')
        if status == 0:
            self.assertGreaterEqual(library.mr_self_reclaim_bytes(), 0)
            self.assertGreaterEqual(library.mr_self_reclaim_failed_batches(), 0)
