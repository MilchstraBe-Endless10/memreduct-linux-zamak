"""Fail-closed executable and cgroup selection tests for the read-only scanner."""

import ctypes
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NATIVE = os.path.join(ROOT, "native")
UID = 1000


def stat_line(pid, comm, ppid, start_time):
    # Fields 3..21 precede start_time (field 22).
    fields = ["S", str(ppid)] + ["0"] * 17 + [str(start_time)]
    return f"{pid} ({comm}) " + " ".join(fields) + "\n"


class CandidateScannerTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="memreduct-candidate-scan-")
        cls.library_path = os.path.join(cls.tmp, "libcandidates.so")
        subprocess.run(
            ["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-pedantic",
             "-shared", "-fPIC", "-DMEMREDUCT_TESTING", "-I", NATIVE,
             os.path.join(NATIVE, "crossproc_candidates.c"), "-o", cls.library_path],
            check=True, capture_output=True, text=True,
        )
        cls.library = ctypes.CDLL(cls.library_path)
        for name in ("mr_cand_scan_run", "mr_cand_count"):
            getattr(cls.library, name).restype = ctypes.c_int64
        cls.library.mr_cand_skipped.argtypes = [ctypes.c_int64]
        cls.library.mr_cand_skipped.restype = ctypes.c_int64

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def setUp(self):
        self.proc = tempfile.mkdtemp(prefix="fixture-proc-")
        self.previous_root = os.environ.get("MEMREDUCT_TEST_PROC_ROOT")
        self.previous_uid = os.environ.get("MEMREDUCT_TEST_UID")
        os.environ["MEMREDUCT_TEST_PROC_ROOT"] = self.proc
        os.environ["MEMREDUCT_TEST_UID"] = str(UID)
        self.addCleanup(self.cleanup_env)
        self.addCleanup(lambda: shutil.rmtree(self.proc, ignore_errors=True))
        self._add_stat(1, "init", 0, 1)
        self._add_stat(os.getpid(), "scanner", 1, 2)

    def cleanup_env(self):
        if self.previous_root is None:
            os.environ.pop("MEMREDUCT_TEST_PROC_ROOT", None)
        else:
            os.environ["MEMREDUCT_TEST_PROC_ROOT"] = self.previous_root
        if self.previous_uid is None:
            os.environ.pop("MEMREDUCT_TEST_UID", None)
        else:
            os.environ["MEMREDUCT_TEST_UID"] = self.previous_uid

    def _add_stat(self, pid, comm, ppid, start_time):
        directory = os.path.join(self.proc, str(pid))
        os.makedirs(directory, exist_ok=True)
        with open(os.path.join(directory, "stat"), "w", encoding="utf-8") as handle:
            handle.write(stat_line(pid, comm, ppid, start_time))
        return directory

    def add_process(self, pid=4242, name="demo", exe="/usr/bin/demo",
                    cgroup=None, uids=(UID, UID, UID, UID)):
        directory = self._add_stat(pid, name, 1, 77)
        uid_text = "\t".join(map(str, uids))
        with open(os.path.join(directory, "status"), "w", encoding="utf-8") as handle:
            handle.write(f"Name:\t{name}\nState:\tS\nUid:\t{uid_text}\n")
        with open(os.path.join(directory, "maps"), "w", encoding="utf-8") as handle:
            handle.write("00400000-00401000 r--p 00000000 00:00 0\n")
        exe_path = os.path.join(directory, "exe")
        if os.path.lexists(exe_path):
            os.unlink(exe_path)
        if exe is not None:
            os.symlink(exe, exe_path)
        if cgroup is None:
            cgroup = (
                f"0::/user.slice/user-{UID}.slice/user@{UID}.service/"
                f"app.slice/app-{name}.scope\n"
            )
        with open(os.path.join(directory, "cgroup"), "w", encoding="utf-8") as handle:
            handle.write(cgroup)
        return directory

    def scan(self):
        self.assertEqual(self.library.mr_cand_scan_run(), 0)
        return self.library.mr_cand_count()

    def skipped(self, reason):
        return self.library.mr_cand_skipped(reason)

    def test_same_uid_user_app_with_known_exe_and_app_cgroup_is_offered(self):
        self.add_process()
        self.assertEqual(self.scan(), 1)

    def test_system_and_login_session_cgroups_are_hidden(self):
        for cgroup in (
            "0::/system.slice/important.service\n",
            f"0::/user.slice/user-{UID}.slice/session-2.scope\n",
        ):
            with self.subTest(cgroup=cgroup):
                self.add_process(cgroup=cgroup)
                self.assertEqual(self.scan(), 0)
                self.assertEqual(self.skipped(6), 1)

    def test_unreadable_executable_or_cgroup_is_hidden(self):
        self.add_process(exe=None)
        self.add_process(pid=4243)
        os.unlink(os.path.join(self.proc, "4243", "cgroup"))
        self.assertEqual(self.scan(), 0)
        self.assertEqual(self.skipped(5), 2)

    def test_unsupported_cgroup_layout_is_hidden(self):
        self.add_process(cgroup=(
            f"0::/user.slice/user-{UID}.slice/app.slice/not-an-app.scope\n"
        ))
        self.assertEqual(self.scan(), 0)
        self.assertEqual(self.skipped(6), 1)

    def test_protected_executable_is_hidden_even_when_comm_is_unrelated(self):
        self.add_process(name="ordinary-client", exe="/usr/bin/Hyprland")
        self.assertEqual(self.scan(), 0)
        self.assertEqual(self.skipped(6), 1)

    def test_uid_tuple_mismatch_is_not_offered(self):
        self.add_process(uids=(UID, UID + 1, UID, UID))
        self.assertEqual(self.scan(), 0)
        self.assertEqual(self.skipped(5), 1)


if __name__ == "__main__":
    unittest.main()
