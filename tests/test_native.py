"""Tests no privileged kernel operations and never starts a real pkexec."""
import ctypes
import os
import pwd
import pathlib
import subprocess
import tempfile
import time
import unittest

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

    def test_bridge_nonblocking_with_fake_pkexec(self):
        # POSIX spawn / waitpid is exercised, but the program executed is our harmless fake.
        fake = self.work / 'pkexec'
        fake.write_text('#!/bin/sh\n[ "$1" = "/usr/libexec/memreduct-zamak-helper" ] || exit 88\n[ "$2" = "pagecache" ] || exit 89\nexit 0\n')
        fake.chmod(0o755)
        old_override = os.environ.get('MEMREDUCT_TEST_PKEXEC')
        old_waitpid_fault = os.environ.get('MEMREDUCT_TEST_WAITPID_EINTR_ONCE')
        try:
            os.environ['MEMREDUCT_TEST_PKEXEC'] = str(fake)
            os.environ['MEMREDUCT_TEST_WAITPID_EINTR_ONCE'] = '1'
            library = ctypes.CDLL(str(self.bridge))
            library.mr_start_reclaim.argtypes = [ctypes.c_int64]
            library.mr_start_reclaim.restype = ctypes.c_int64
            library.mr_poll_reclaim.restype = ctypes.c_int64
            self.assertEqual(library.mr_start_reclaim(99), 22)
            self.assertEqual(library.mr_start_reclaim(1), 22)
            self.assertEqual(library.mr_start_reclaim(0), 0)
            self.assertEqual(library.mr_poll_reclaim(), -1)
            result = -1
            for _ in range(100):
                result = library.mr_poll_reclaim()
                if result != -1:
                    break
                time.sleep(0.01)
            self.assertEqual(result, 0)
            self.assertEqual(library.mr_poll_reclaim(), -2)
        finally:
            if old_override is None:
                os.environ.pop('MEMREDUCT_TEST_PKEXEC', None)
            else:
                os.environ['MEMREDUCT_TEST_PKEXEC'] = old_override
            if old_waitpid_fault is None:
                os.environ.pop('MEMREDUCT_TEST_WAITPID_EINTR_ONCE', None)
            else:
                os.environ['MEMREDUCT_TEST_WAITPID_EINTR_ONCE'] = old_waitpid_fault

    def test_bridge_preserves_failure_and_uncertain_exit_status_without_retry(self):
        fake = self.work / 'pkexec-status'
        calls = self.work / 'pkexec-status-calls.log'
        fake.write_text(
            '#!/bin/sh\n'
            '[ "$1" = "/usr/libexec/memreduct-zamak-helper" ] || exit 88\n'
            '[ "$2" = "pagecache" ] || exit 89\n'
            'printf "%s %s\\n" "$1" "$2" >> "$MEMREDUCT_TEST_PKEXEC_LOG"\n'
            'exit "$MEMREDUCT_TEST_PKEXEC_EXIT"\n'
        )
        fake.chmod(0o755)
        env_names = (
            'MEMREDUCT_TEST_PKEXEC',
            'MEMREDUCT_TEST_PKEXEC_LOG',
            'MEMREDUCT_TEST_PKEXEC_EXIT',
        )
        previous = {name: os.environ.get(name) for name in env_names}
        try:
            os.environ['MEMREDUCT_TEST_PKEXEC'] = str(fake)
            os.environ['MEMREDUCT_TEST_PKEXEC_LOG'] = str(calls)
            library = ctypes.CDLL(str(self.bridge))
            library.mr_start_reclaim.argtypes = [ctypes.c_int64]
            library.mr_start_reclaim.restype = ctypes.c_int64
            library.mr_poll_reclaim.restype = ctypes.c_int64
            for expected in (1, 3, 126):
                os.environ['MEMREDUCT_TEST_PKEXEC_EXIT'] = str(expected)
                self.assertEqual(library.mr_start_reclaim(0), 0)
                result = -1
                for _ in range(100):
                    result = library.mr_poll_reclaim()
                    if result != -1:
                        break
                    time.sleep(0.01)
                self.assertEqual(result, expected)
                self.assertEqual(library.mr_poll_reclaim(), -2)
            self.assertEqual(len(calls.read_text().splitlines()), 3)
        finally:
            for name, value in previous.items():
                if value is None:
                    os.environ.pop(name, None)
                else:
                    os.environ[name] = value


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
