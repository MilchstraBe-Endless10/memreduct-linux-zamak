"""P3A preference store tests; every target is rooted in a TemporaryDirectory."""
import ctypes
import fcntl
import os
import pathlib
import stat
import subprocess
import sys
import tempfile
import time
import unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
DEFAULTS = b'schema=1\nlanguage=en\ntheme=light\n'

ABSENT = 0
VALID = 1
CORRUPT = 2
UNREADABLE = 3
UNSUPPORTED_SCHEMA = 4
UNSAFE_OBJECT = 5
OWNERSHIP_OR_MODE = 6
PATH_UNAVAILABLE = 7

COMMITTED_DURABLE = 0
NO_CHANGE = 1
FAILED_UNCHANGED = 2
BLOCKED_RELOAD = 3
VISIBLE_DURABILITY_UNCERTAIN = 4
RECOVERY_REQUIRED = 5
INVALID_ARGUMENT = 6

STAGE_RENAME = 11
STAGE_DIRECTORY_FSYNC = 12
STAGE_COMPLETE = 14

TEST_FAULTS = {
    'write_eintr': 1,
    'write_short': 2,
    'write_fail': 3,
    'file_fsync_fail': 4,
    'rename_before': 5,
    'rename_after': 6,
    'dir_fsync_fail': 7,
    'openat2_enosys': 8,
}


def compile_store(output, test_root=None):
    command = [
        'cc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-pedantic',
        '-shared', '-fPIC', str(ROOT / 'native/preferences_store.c'),
        '-o', str(output),
    ]
    if test_root is not None:
        command[1:1] = [
            '-DMR_PREFERENCES_TESTING',
            f'-DMR_PREFERENCES_TEST_ROOT="{test_root}"',
        ]
    subprocess.run(command, check=True, capture_output=True, text=True)


def bind_api(library):
    for name in (
        'mr_preferences_load', 'mr_preferences_language', 'mr_preferences_theme',
        'mr_preferences_values_known', 'mr_preferences_can_save',
        'mr_preferences_reset', 'mr_preferences_set_language',
        'mr_preferences_set_theme', 'mr_preferences_last_stage',
        'mr_preferences_last_error', 'mr_preferences_last_rename_succeeded',
        'mr_preferences_last_readback_status',
    ):
        function = getattr(library, name)
        function.restype = ctypes.c_int64
    library.mr_preferences_set_language.argtypes = [ctypes.c_int64]
    library.mr_preferences_set_theme.argtypes = [ctypes.c_int64]
    if hasattr(library, 'mr_preferences_test_inject_fault'):
        library.mr_preferences_test_inject_fault.argtypes = [ctypes.c_int64]
        library.mr_preferences_test_inject_fault.restype = ctypes.c_int64
    return library


class TestPreferencesStore(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.work = pathlib.Path(self.temp.name)
        self.root = self.work / 'config'
        self.root.mkdir(mode=0o700)
        self.root.chmod(0o700)
        self.library_path = self.work / 'libpreferences-test.so'
        compile_store(self.library_path, self.root)
        self.lib = bind_api(ctypes.CDLL(str(self.library_path)))
        self.app = self.root / 'memreduct-zamak'
        self.config = self.app / 'preferences.conf'
        self.lock = self.app / 'preferences.lock'

    def make_app(self):
        self.app.mkdir(mode=0o700, exist_ok=True)
        self.app.chmod(0o700)

    def write_config(self, contents, mode=0o600):
        self.make_app()
        self.config.write_bytes(contents)
        self.config.chmod(mode)

    def fault(self, name):
        self.assertEqual(self.lib.mr_preferences_test_inject_fault(TEST_FAULTS[name]), COMMITTED_DURABLE)

    def no_temp_files(self):
        if self.app.exists():
            self.assertEqual(list(self.app.glob('.preferences.tmp.*')), [])

    def test_absent_uses_en_light_without_creating_config(self):
        self.assertEqual(self.lib.mr_preferences_load(), ABSENT)
        self.assertEqual((self.lib.mr_preferences_language(), self.lib.mr_preferences_theme()), (0, 0))
        self.assertEqual(self.lib.mr_preferences_values_known(), 1)
        self.assertEqual(self.lib.mr_preferences_can_save(), 1)
        self.assertFalse(self.app.exists())

    def test_valid_read_and_unknown_keys_are_canonicalized_only_on_explicit_save(self):
        self.write_config(b'schema=1\nlanguage=zh-CN\ntheme=dark\nauto_clean=1\nhelper=/bin/sh\n')
        self.assertEqual(self.lib.mr_preferences_load(), VALID)
        self.assertEqual((self.lib.mr_preferences_language(), self.lib.mr_preferences_theme()), (1, 1))
        self.assertEqual(self.config.read_bytes().splitlines()[-2:], [b'auto_clean=1', b'helper=/bin/sh'])
        self.assertEqual(self.lib.mr_preferences_set_language(1), COMMITTED_DURABLE)
        self.assertEqual(self.config.read_bytes(), b'schema=1\nlanguage=zh-CN\ntheme=dark\n')
        self.assertEqual(self.lib.mr_preferences_set_language(1), NO_CHANGE)

    def test_unknown_future_schema_is_not_downgraded_by_reset_or_set(self):
        original = b'schema=2\nlanguage=ja\ntheme=dim\nnew_setting=keep\n'
        self.write_config(original)
        self.assertEqual(self.lib.mr_preferences_load(), UNSUPPORTED_SCHEMA)
        self.assertEqual((self.lib.mr_preferences_language(), self.lib.mr_preferences_theme()), (0, 0))
        self.assertEqual(self.lib.mr_preferences_can_save(), 0)
        self.assertEqual(self.lib.mr_preferences_set_theme(1), BLOCKED_RELOAD)
        self.assertEqual(self.lib.mr_preferences_reset(), BLOCKED_RELOAD)
        self.assertEqual(self.config.read_bytes(), original)

    def test_corrupt_configurations_block_writes_without_replacement(self):
        malformed = (
            b'schema=1\nlanguage=en\nlanguage=zh-CN\ntheme=light\n',
            b'schema=1\nlanguage=en\n',
            b'schema=1\nlanguage=fr\ntheme=light\n',
            b'schema=1\nlanguage=en\ntheme=light\nextra line\n',
            b'schema=1\nlanguage=en\ntheme=light\x00\n',
            b'x' * 4097,
        )
        for index, original in enumerate(malformed):
            with self.subTest(index=index):
                self.write_config(original)
                self.assertEqual(self.lib.mr_preferences_load(), CORRUPT)
                self.assertEqual(self.lib.mr_preferences_can_save(), 0)
                self.assertEqual(self.lib.mr_preferences_set_language(1), BLOCKED_RELOAD)
                self.assertEqual(self.lib.mr_preferences_reset(), BLOCKED_RELOAD)
                self.assertEqual(self.config.read_bytes(), original)
                self.config.unlink()

    def test_symlink_nonregular_and_permissions_are_distinguished_and_preserved(self):
        target = self.work / 'sentinel'
        target.write_bytes(b'untouched')
        target.chmod(0o600)
        self.make_app()
        self.config.symlink_to(target)
        self.assertEqual(self.lib.mr_preferences_load(), UNSAFE_OBJECT)
        self.assertEqual(self.lib.mr_preferences_set_theme(1), BLOCKED_RELOAD)
        self.assertEqual(target.read_bytes(), b'untouched')
        self.config.unlink()
        self.config.mkdir()
        self.assertEqual(self.lib.mr_preferences_load(), UNSAFE_OBJECT)
        self.config.rmdir()

        self.write_config(DEFAULTS, mode=0o644)
        before_mode = stat.S_IMODE(self.config.stat().st_mode)
        self.assertEqual(self.lib.mr_preferences_load(), OWNERSHIP_OR_MODE)
        self.assertEqual(self.lib.mr_preferences_reset(), BLOCKED_RELOAD)
        self.assertEqual(stat.S_IMODE(self.config.stat().st_mode), before_mode)
        self.assertEqual(self.config.read_bytes(), DEFAULTS)

    def test_app_directory_and_lock_symlink_are_never_followed(self):
        sentinel = self.work / 'outside'
        sentinel.mkdir(mode=0o700)
        (self.root / 'memreduct-zamak').symlink_to(sentinel, target_is_directory=True)
        self.assertEqual(self.lib.mr_preferences_load(), UNSAFE_OBJECT)
        (self.root / 'memreduct-zamak').unlink()

        self.make_app()
        lock_target = self.work / 'lock-sentinel'
        lock_target.write_bytes(b'unchanged')
        lock_target.chmod(0o600)
        self.lock.symlink_to(lock_target)
        self.assertEqual(self.lib.mr_preferences_set_language(1), BLOCKED_RELOAD)
        self.assertEqual(lock_target.read_bytes(), b'unchanged')
        self.assertTrue(self.lock.is_symlink())

    def test_lock_is_private_and_persistent_across_transactions(self):
        self.assertEqual(self.lib.mr_preferences_set_theme(1), COMMITTED_DURABLE)
        first = self.lock.stat()
        self.assertEqual(stat.S_IMODE(first.st_mode), 0o600)
        self.assertEqual(first.st_nlink, 1)
        self.assertEqual(self.lib.mr_preferences_set_language(1), COMMITTED_DURABLE)
        second = self.lock.stat()
        self.assertEqual((first.st_dev, first.st_ino), (second.st_dev, second.st_ino))
        self.assertEqual(stat.S_IMODE(second.st_mode), 0o600)

    def test_lock_mode_type_and_link_count_are_rejected_without_repair(self):
        self.make_app()
        self.lock.write_bytes(b'')
        self.lock.chmod(0o644)
        self.assertEqual(self.lib.mr_preferences_set_language(1), BLOCKED_RELOAD)
        self.assertEqual(stat.S_IMODE(self.lock.stat().st_mode), 0o644)
        self.assertFalse(self.config.exists())

        self.lock.unlink()
        os.mkfifo(self.lock, 0o600)
        self.assertEqual(self.lib.mr_preferences_set_language(1), BLOCKED_RELOAD)
        self.assertTrue(stat.S_ISFIFO(self.lock.lstat().st_mode))
        self.lock.unlink()

        self.lock.write_bytes(b'')
        self.lock.chmod(0o600)
        hardlink = self.work / 'lock-hardlink'
        os.link(self.lock, hardlink)
        self.assertEqual(self.lib.mr_preferences_set_language(1), BLOCKED_RELOAD)
        self.assertEqual(self.lock.stat().st_nlink, 2)
        self.assertFalse(self.config.exists())

    def test_two_process_updates_merge_under_persistent_flock(self):
        self.assertEqual(self.lib.mr_preferences_reset(), COMMITTED_DURABLE)
        lock_fd = os.open(self.lock, os.O_RDWR)
        fcntl.flock(lock_fd, fcntl.LOCK_EX)
        script = (
            'import ctypes,pathlib,sys; '
            'lib=ctypes.CDLL(sys.argv[1]); '
            'fn=getattr(lib,sys.argv[2]); fn.argtypes=[ctypes.c_int64]; fn.restype=ctypes.c_int64; '
            'pathlib.Path(sys.argv[4]).touch(); sys.stdin.buffer.read(1); print(fn(int(sys.argv[3])))'
        )
        jobs = (
            ('mr_preferences_set_language', 1, self.work / 'language.started'),
            ('mr_preferences_set_theme', 1, self.work / 'theme.started'),
        )
        children = []
        try:
            for function, value, marker in jobs:
                children.append(subprocess.Popen(
                    [sys.executable, '-c', script, str(self.library_path), function, str(value), str(marker)],
                    stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
                ))
            deadline = time.monotonic() + 3
            while time.monotonic() < deadline and not all(marker.exists() for _, _, marker in jobs):
                time.sleep(0.01)
            self.assertTrue(all(marker.exists() for _, _, marker in jobs))
            for child in children:
                child.stdin.write('x')
                child.stdin.flush()
            time.sleep(0.05)
            self.assertTrue(all(child.poll() is None for child in children))
        finally:
            fcntl.flock(lock_fd, fcntl.LOCK_UN)
            os.close(lock_fd)

        for child in children:
            stdout, stderr = child.communicate(timeout=5)
            self.assertEqual(child.returncode, 0, stderr)
            self.assertEqual(int(stdout.strip()), COMMITTED_DURABLE)
        self.assertEqual(self.lib.mr_preferences_load(), VALID)
        self.assertEqual((self.lib.mr_preferences_language(), self.lib.mr_preferences_theme()), (1, 1))

    def test_reset_is_a_persisted_transaction(self):
        self.assertEqual(self.lib.mr_preferences_set_language(1), COMMITTED_DURABLE)
        self.assertEqual(self.lib.mr_preferences_set_theme(1), COMMITTED_DURABLE)
        self.assertEqual(self.lib.mr_preferences_reset(), COMMITTED_DURABLE)
        self.assertEqual((self.lib.mr_preferences_language(), self.lib.mr_preferences_theme()), (0, 0))
        self.assertEqual(self.config.read_bytes(), DEFAULTS)

    def test_reset_from_absent_creates_defaults_only_when_requested(self):
        self.assertEqual(self.lib.mr_preferences_load(), ABSENT)
        self.assertFalse(self.app.exists())
        self.assertEqual(self.lib.mr_preferences_reset(), COMMITTED_DURABLE)
        self.assertEqual(self.config.read_bytes(), DEFAULTS)

    def test_preferences_are_restored_in_a_fresh_process(self):
        probe = (
            'import ctypes,sys; lib=ctypes.CDLL(sys.argv[1]); '
            'lib.mr_preferences_load.restype=ctypes.c_int64; '
            'lib.mr_preferences_language.restype=ctypes.c_int64; '
            'lib.mr_preferences_theme.restype=ctypes.c_int64; '
            'print(lib.mr_preferences_load(),lib.mr_preferences_language(),lib.mr_preferences_theme())'
        )
        for language, theme in ((0, 0), (0, 1), (1, 0), (1, 1)):
            with self.subTest(language=language, theme=theme):
                self.assertIn(self.lib.mr_preferences_set_language(language),
                              (COMMITTED_DURABLE, NO_CHANGE))
                self.assertIn(self.lib.mr_preferences_set_theme(theme),
                              (COMMITTED_DURABLE, NO_CHANGE))
                result = subprocess.run(
                    [sys.executable, '-c', probe, str(self.library_path)],
                    check=True, capture_output=True, text=True,
                )
                self.assertEqual(result.stdout.strip(), f'1 {language} {theme}')

    def test_short_write_and_eintr_are_completed_without_data_loss(self):
        self.assertEqual(self.lib.mr_preferences_reset(), COMMITTED_DURABLE)
        for fault in ('write_eintr', 'write_short'):
            with self.subTest(fault=fault):
                self.fault(fault)
                self.assertEqual(self.lib.mr_preferences_set_theme(1), COMMITTED_DURABLE)
                self.assertEqual(self.config.read_bytes(), b'schema=1\nlanguage=en\ntheme=dark\n')
                self.assertEqual(self.lib.mr_preferences_reset(), COMMITTED_DURABLE)

    def test_write_and_file_fsync_failures_keep_old_file_and_clean_owned_temp(self):
        self.assertEqual(self.lib.mr_preferences_reset(), COMMITTED_DURABLE)
        for fault in ('write_fail', 'file_fsync_fail'):
            with self.subTest(fault=fault):
                before = self.config.read_bytes()
                self.fault(fault)
                self.assertEqual(self.lib.mr_preferences_set_theme(1), FAILED_UNCHANGED)
                self.assertEqual(self.config.read_bytes(), before)
                self.no_temp_files()

    def test_rename_before_failure_is_unchanged_and_rename_after_is_uncertain(self):
        self.assertEqual(self.lib.mr_preferences_reset(), COMMITTED_DURABLE)
        before = self.config.read_bytes()
        self.fault('rename_before')
        self.assertEqual(self.lib.mr_preferences_set_theme(1), FAILED_UNCHANGED)
        self.assertEqual(self.lib.mr_preferences_last_stage(), STAGE_RENAME)
        self.assertEqual(self.lib.mr_preferences_last_rename_succeeded(), 0)
        self.assertEqual(self.lib.mr_preferences_last_readback_status(), VALID)
        self.assertEqual(self.config.read_bytes(), before)
        self.no_temp_files()

        self.fault('rename_after')
        self.assertEqual(self.lib.mr_preferences_set_theme(1), VISIBLE_DURABILITY_UNCERTAIN)
        self.assertEqual(self.lib.mr_preferences_last_stage(), STAGE_RENAME)
        self.assertEqual(self.lib.mr_preferences_last_rename_succeeded(), -1)
        self.assertEqual(self.lib.mr_preferences_last_readback_status(), VALID)
        expected = b'schema=1\nlanguage=en\ntheme=dark\n'
        self.assertEqual(self.config.read_bytes(), expected)
        self.assertEqual((self.lib.mr_preferences_language(), self.lib.mr_preferences_theme()), (0, 1))
        self.assertEqual(self.lib.mr_preferences_values_known(), 1)
        self.no_temp_files()

    def test_directory_fsync_failure_reads_back_visible_value(self):
        self.assertEqual(self.lib.mr_preferences_reset(), COMMITTED_DURABLE)
        self.fault('dir_fsync_fail')
        self.assertEqual(self.lib.mr_preferences_set_language(1), VISIBLE_DURABILITY_UNCERTAIN)
        self.assertEqual(self.lib.mr_preferences_last_stage(), STAGE_DIRECTORY_FSYNC)
        self.assertEqual(self.lib.mr_preferences_last_rename_succeeded(), 1)
        self.assertEqual(self.lib.mr_preferences_last_readback_status(), VALID)
        self.assertEqual(self.config.read_bytes(), b'schema=1\nlanguage=zh-CN\ntheme=light\n')
        self.assertEqual((self.lib.mr_preferences_language(), self.lib.mr_preferences_theme()), (1, 0))
        self.assertEqual(self.lib.mr_preferences_values_known(), 1)
        uncertain_inode = (self.config.stat().st_dev, self.config.stat().st_ino)
        self.assertEqual(self.lib.mr_preferences_set_language(1), COMMITTED_DURABLE)
        self.assertEqual(self.lib.mr_preferences_last_stage(), STAGE_COMPLETE)
        self.assertEqual(self.lib.mr_preferences_last_rename_succeeded(), 1)
        self.assertEqual((self.config.stat().st_dev, self.config.stat().st_ino), uncertain_inode)

    def test_temp_residue_is_ignored_and_openat_fallback_is_used_only_on_enosys(self):
        self.make_app()
        residue = self.app / '.preferences.tmp.untrusted'
        residue.write_bytes(b'schema=1\nlanguage=zh-CN\ntheme=dark\n')
        residue.chmod(0o600)
        self.assertEqual(self.lib.mr_preferences_load(), ABSENT)
        self.fault('openat2_enosys')
        self.assertEqual(self.lib.mr_preferences_reset(), COMMITTED_DURABLE)
        self.assertEqual(self.config.read_bytes(), DEFAULTS)
        self.assertEqual(residue.read_bytes(), b'schema=1\nlanguage=zh-CN\ntheme=dark\n')

    def test_invalid_enum_has_distinct_status_and_no_write(self):
        before = self.lib.mr_preferences_reset()
        self.assertEqual(before, COMMITTED_DURABLE)
        original = self.config.read_bytes()
        self.assertEqual(self.lib.mr_preferences_set_language(2), INVALID_ARGUMENT)
        self.assertEqual(self.lib.mr_preferences_set_theme(-1), INVALID_ARGUMENT)
        self.assertEqual(self.config.read_bytes(), original)


class TestProductionPathResolution(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.temp.cleanup)
        cls.work = pathlib.Path(cls.temp.name)
        cls.library_path = cls.work / 'libpreferences-production.so'
        compile_store(cls.library_path)

    def run_probe(self, xdg, home, operation='load'):
        code = (
            'import ctypes,sys; '
            'lib=ctypes.CDLL(sys.argv[1]); '
            'names=("mr_preferences_load","mr_preferences_set_language","mr_preferences_set_theme",'
            '"mr_preferences_can_save"); '
            '[setattr(getattr(lib,n),"restype",ctypes.c_int64) for n in names]; '
            'print(getattr(lib,sys.argv[2])(*(tuple(map(int,sys.argv[3:])))))'
            if operation != 'load' else
            'import ctypes,sys; lib=ctypes.CDLL(sys.argv[1]); '
            'lib.mr_preferences_load.restype=ctypes.c_int64; print(lib.mr_preferences_load())'
        )
        env = os.environ.copy()
        env['HOME'] = str(home)
        if xdg is None:
            env.pop('XDG_CONFIG_HOME', None)
        else:
            env['XDG_CONFIG_HOME'] = str(xdg)
        arguments = [sys.executable, '-c', code, str(self.library_path)]
        if operation != 'load':
            arguments.append(operation)
            arguments.extend(('1',))
        result = subprocess.run(arguments, env=env, check=True, capture_output=True, text=True)
        return int(result.stdout.strip())

    def test_absolute_xdg_wins_over_fake_home(self):
        xdg = self.work / 'xdg-selected'
        home = self.work / 'fake-home'
        xdg.mkdir(mode=0o700)
        home.mkdir(mode=0o700)
        (home / '.config').mkdir(mode=0o700)
        self.assertEqual(self.run_probe(xdg, home, 'mr_preferences_set_language'), COMMITTED_DURABLE)
        self.assertTrue((xdg / 'memreduct-zamak/preferences.conf').exists())
        self.assertFalse((home / '.config/memreduct-zamak').exists())

    def test_invalid_relative_xdg_falls_back_to_fake_home_config(self):
        home = self.work / 'fallback-home'
        (home / '.config').mkdir(mode=0o700, parents=True)
        self.assertEqual(self.run_probe('relative/config', home, 'mr_preferences_set_theme'), COMMITTED_DURABLE)
        self.assertTrue((home / '.config/memreduct-zamak/preferences.conf').exists())

    def test_missing_or_empty_xdg_uses_home_config(self):
        for index, xdg in enumerate((None, '')):
            with self.subTest(xdg=xdg):
                home = self.work / f'home-fallback-{index}'
                (home / '.config').mkdir(mode=0o700, parents=True)
                self.assertEqual(self.run_probe(xdg, home, 'mr_preferences_set_theme'), COMMITTED_DURABLE)
                self.assertTrue((home / '.config/memreduct-zamak/preferences.conf').exists())

    def test_absolute_xdg_with_dot_component_is_rejected_without_home_fallback(self):
        home = self.work / 'dot-xdg-home'
        (home / '.config').mkdir(mode=0o700, parents=True)
        invalid = str(self.work / 'config') + '/../unsafe'
        self.assertEqual(self.run_probe(invalid, home), PATH_UNAVAILABLE)
        self.assertFalse((home / '.config/memreduct-zamak').exists())

    def test_selected_missing_xdg_does_not_fall_back_to_home(self):
        home = self.work / 'missing-xdg-home'
        (home / '.config').mkdir(mode=0o700, parents=True)
        missing = self.work / 'does-not-exist'
        self.assertEqual(self.run_probe(missing, home), PATH_UNAVAILABLE)
        self.assertFalse((home / '.config/memreduct-zamak').exists())

    def test_selected_unsafe_xdg_does_not_fall_back_to_home(self):
        xdg = self.work / 'unsafe-xdg'
        xdg.mkdir(mode=0o700)
        xdg.chmod(0o770)
        home = self.work / 'unsafe-xdg-home'
        (home / '.config').mkdir(mode=0o700, parents=True)
        self.assertEqual(self.run_probe(xdg, home), OWNERSHIP_OR_MODE)
        self.assertFalse((home / '.config/memreduct-zamak').exists())

    def test_symlinked_xdg_root_is_allowed_after_target_validation(self):
        real_root = self.work / 'real-xdg'
        real_root.mkdir(mode=0o700)
        link = self.work / 'xdg-link'
        link.symlink_to(real_root, target_is_directory=True)
        home = self.work / 'other-home'
        home.mkdir(mode=0o700)
        self.assertEqual(self.run_probe(link, home, 'mr_preferences_set_language'), COMMITTED_DURABLE)
        self.assertTrue((real_root / 'memreduct-zamak/preferences.conf').exists())

    def test_production_library_contains_no_test_injection_symbol(self):
        result = subprocess.run(['nm', '-D', str(self.library_path)], check=True,
                                capture_output=True, text=True)
        self.assertNotIn('mr_preferences_test_inject_fault', result.stdout)


if __name__ == '__main__':
    unittest.main()
