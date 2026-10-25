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
        self.bridge = self.work / 'libnative.so'
        subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', '-shared', '-fPIC', '-DMEMREDUCT_TESTING',
                        str(ROOT / 'native/async_privileged.c'), '-o', str(self.bridge)], check=True)

    def test_helper_dry_run_and_reject_unknown_mode(self):
        result = subprocess.run([self.helper, '--dry-run'], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0)
        self.assertIn('no kernel', result.stdout)
        result = subprocess.run([self.helper, 'delete-files'], capture_output=True, text=True)
        self.assertEqual(result.returncode, 2)

    def test_unprivileged_cannot_touch_kernel(self):
        preexec = None
        if os.geteuid() == 0:
            # Test a normal unprivileged account even when CI runs as root.
            self.work.chmod(0o755)
            nobody = pwd.getpwnam('nobody')
            preexec = lambda: (os.setgid(nobody.pw_gid), os.setuid(nobody.pw_uid))
        result = subprocess.run([self.helper, 'all'], capture_output=True, text=True, preexec_fn=preexec)
        self.assertEqual(result.returncode, 1)
        self.assertIn('root privileges', result.stderr)

    def test_bridge_nonblocking_with_fake_pkexec(self):
        # POSIX spawn / waitpid is exercised, but the program executed is our harmless fake.
        fake = self.work / 'pkexec'
        fake.write_text('#!/bin/sh\n[ "$1" = "/usr/libexec/memreduct-zamak-helper" ] || exit 88\n[ "$2" = "all" ] || exit 89\nexit 0\n')
        fake.chmod(0o755)
        old_override = os.environ.get('MEMREDUCT_TEST_PKEXEC')
        try:
            os.environ['MEMREDUCT_TEST_PKEXEC'] = str(fake)
            library = ctypes.CDLL(str(self.bridge))
            library.mr_start_reclaim.argtypes = [ctypes.c_int64]
            library.mr_start_reclaim.restype = ctypes.c_int64
            library.mr_poll_reclaim.restype = ctypes.c_int64
            self.assertEqual(library.mr_start_reclaim(99), 22)
            self.assertEqual(library.mr_start_reclaim(2), 0)
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


if __name__ == '__main__':
    unittest.main()
