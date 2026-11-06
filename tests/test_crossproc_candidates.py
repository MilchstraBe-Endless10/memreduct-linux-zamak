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
CANDIDATE_CAP = 256


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
        for name in ("mr_cand_name", "mr_cand_app_name"):
            getattr(cls.library, name).argtypes = [ctypes.c_int64]
            getattr(cls.library, name).restype = ctypes.c_char_p
        cls.library.mr_cand_state.argtypes = [ctypes.c_int64]
        cls.library.mr_cand_state.restype = ctypes.c_int64
        for name in ("mr_cand_identity_token", "mr_cand_scope_token",
                     "mr_cand_region_digest", "mr_cand_snapshot_select",
                     "mr_cand_snapshot_bind_request", "mr_cand_snapshot_request_id"):
            getattr(cls.library, name).argtypes = [ctypes.c_int64]
            getattr(cls.library, name).restype = ctypes.c_int64
        cls.library.mr_cand_snapshot_request_id.argtypes = []
        cls.library.mr_cand_snapshot_clear.argtypes = []
        cls.library.mr_cand_snapshot_clear.restype = None
        cls.library.mr_cand_snapshot_verify.argtypes = [ctypes.c_int64] * 9
        cls.library.mr_cand_snapshot_verify.restype = ctypes.c_int64
        for name in ("mr_cand_scope_anon_file_bytes", "mr_cand_rss_bytes"):
            getattr(cls.library, name).argtypes = [ctypes.c_int64]
            getattr(cls.library, name).restype = ctypes.c_int64
        cls.library.mr_cand_private_resident_bytes.argtypes = [ctypes.c_int64]
        cls.library.mr_cand_private_resident_bytes.restype = ctypes.c_int64

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def setUp(self):
        self.proc = tempfile.mkdtemp(prefix="fixture-proc-")
        self.cgroups = tempfile.mkdtemp(prefix="fixture-cgroup-")
        self.previous_root = os.environ.get("MEMREDUCT_TEST_PROC_ROOT")
        self.previous_cgroup_root = os.environ.get("MEMREDUCT_TEST_CGROUP_ROOT")
        self.previous_uid = os.environ.get("MEMREDUCT_TEST_UID")
        os.environ["MEMREDUCT_TEST_PROC_ROOT"] = self.proc
        os.environ["MEMREDUCT_TEST_CGROUP_ROOT"] = self.cgroups
        os.environ["MEMREDUCT_TEST_UID"] = str(UID)
        self.addCleanup(self.cleanup_env)
        self.addCleanup(lambda: shutil.rmtree(self.proc, ignore_errors=True))
        self.addCleanup(lambda: shutil.rmtree(self.cgroups, ignore_errors=True))
        self._add_stat(1, "init", 0, 1)
        self._add_stat(os.getpid(), "scanner", 1, 2)

    def cleanup_env(self):
        if self.previous_root is None:
            os.environ.pop("MEMREDUCT_TEST_PROC_ROOT", None)
        else:
            os.environ["MEMREDUCT_TEST_PROC_ROOT"] = self.previous_root
        if self.previous_cgroup_root is None:
            os.environ.pop("MEMREDUCT_TEST_CGROUP_ROOT", None)
        else:
            os.environ["MEMREDUCT_TEST_CGROUP_ROOT"] = self.previous_cgroup_root
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
                    cgroup=None, uids=(UID, UID, UID, UID), state="S"):
        directory = self._add_stat(pid, name, 1, 77)
        uid_text = "\t".join(map(str, uids))
        with open(os.path.join(directory, "status"), "w", encoding="utf-8") as handle:
            handle.write(f"Name:\t{name}\nState:\t{state}\nUid:\t{uid_text}\nVmRSS:\t4096 kB\nVmSwap:\t1024 kB\n")
        self.write_mapping_files(directory, "00400000-00401000 r--p 00000000 00:00 0\n", 4)
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
        argv0 = (exe or f"/usr/bin/{name}").encode("utf-8")
        with open(os.path.join(directory, "cmdline"), "wb") as handle:
            handle.write(argv0 + b"\0--fixture\0")
        return directory

    @staticmethod
    def write_mapping_files(directory, maps, private_dirty_kib=4):
        with open(os.path.join(directory, "maps"), "w", encoding="utf-8") as handle:
            handle.write(maps)
        headers = []
        for line in maps.splitlines():
            fields = line.split(maxsplit=5)
            if len(fields) < 5:
                continue
            start, end = (int(value, 16) for value in fields[0].split("-", 1))
            name = f" {fields[5]}" if len(fields) == 6 else ""
            private = private_dirty_kib if fields[1][3] == "p" and not fields[1].startswith("---") and not (len(fields) >= 6 and fields[5].startswith("[")) else 0
            headers.append(
                f"{start:08x}-{end:08x} {fields[1]} {fields[2]} {fields[3]} {fields[4]}{name}\n"
                f"Size: {(end - start) // 1024} kB\n"
                f"Private_Clean: 0 kB\nPrivate_Dirty: {private} kB\n"
            )
        with open(os.path.join(directory, "smaps"), "w", encoding="utf-8") as handle:
            handle.write("".join(headers))

    def add_scope_stats(self, process_name="demo", anon_bytes=128 * 1024 * 1024,
                        file_bytes=64 * 1024 * 1024, include_reclaim=True):
        scope = os.path.join(
            self.cgroups, "user.slice", f"user-{UID}.slice", f"user@{UID}.service",
            "app.slice", f"app-{process_name}.scope",
        )
        os.makedirs(scope, exist_ok=True)
        with open(os.path.join(scope, "memory.stat"), "w", encoding="ascii") as handle:
            handle.write(f"anon {anon_bytes}\nfile {file_bytes}\n")
        if include_reclaim:
            with open(os.path.join(scope, "memory.reclaim"), "w", encoding="ascii") as handle:
                handle.write("")
        return scope

    def scan(self):
        self.assertEqual(self.library.mr_cand_scan_run(), 0)
        return self.library.mr_cand_count()

    def skipped(self, reason):
        return self.library.mr_cand_skipped(reason)

    def select_snapshot(self, index, request_id=1234567):
        self.assertEqual(self.library.mr_cand_snapshot_select(index), 0)
        self.assertEqual(self.library.mr_cand_snapshot_bind_request(request_id), 0)
        self.assertEqual(self.library.mr_cand_snapshot_request_id(), request_id)
        return (
            request_id, self.library.mr_cand_pid(index),
            self.library.mr_cand_start_time(index), self.library.mr_cand_uid(index),
            self.library.mr_cand_identity_token(index), self.library.mr_cand_scope_token(index),
            self.library.mr_cand_region_digest(index),
            self.library.mr_cand_reclaimable_bytes(index), self.library.mr_cand_regions(index),
        )

    def test_same_uid_user_app_with_known_exe_and_app_cgroup_is_offered(self):
        self.add_process()
        self.assertEqual(self.scan(), 1)

    def test_candidate_exposes_live_rss_and_app_scope_charge_estimate(self):
        self.add_process()
        self.add_scope_stats(anon_bytes=768 * 1024 * 1024,
                             file_bytes=512 * 1024 * 1024)
        self.assertEqual(self.scan(), 1)
        self.assertEqual(self.library.mr_cand_rss_bytes(0), 4 * 1024 * 1024)
        self.assertEqual(self.library.mr_cand_scope_anon_file_bytes(0),
                         1280 * 1024 * 1024)
        self.assertEqual(self.library.mr_cand_private_resident_bytes(0), 4096)

    def test_app_scope_mode_is_unavailable_without_its_kernel_control(self):
        self.add_process()
        self.add_scope_stats(include_reclaim=False)
        self.assertEqual(self.scan(), 1)
        self.assertEqual(self.library.mr_cand_scope_anon_file_bytes(0), -1)

    def test_app_scope_charge_is_not_part_of_immutable_process_snapshot(self):
        self.add_process()
        scope = self.add_scope_stats()
        self.assertEqual(self.scan(), 1)
        values = self.select_snapshot(0, request_id=900008)
        with open(os.path.join(scope, "memory.stat"), "w", encoding="ascii") as handle:
            handle.write("anon 536870912\nfile 268435456\n")
        self.assertEqual(self.library.mr_cand_snapshot_verify(*values), 0)

    def test_dotnet_application_in_user_scope_is_not_protected(self):
        directory = self.add_process(
            name="dotnet", exe="/usr/share/dotnet/dotnet",
            cgroup=(f"0::/user.slice/user-{UID}.slice/user@{UID}.service/"
                    "app.slice/app-pinta-m13-test.scope\n"),
        )
        with open(os.path.join(directory, "cmdline"), "wb") as handle:
            handle.write(b"/usr/bin/dotnet\0/usr/lib/pinta/Pinta.dll\0")
        self.assertEqual(self.scan(), 1)
        self.assertEqual(self.library.mr_cand_app_name(0), b"Pinta")

    def test_candidate_list_limit_is_reported(self):
        for index in range(CANDIDATE_CAP + 1):
            self.add_process(pid=4242 + index, name=f"demo-{index}",
                             exe=f"/usr/bin/demo-{index}")
        self.assertEqual(self.scan(), CANDIDATE_CAP)
        self.assertEqual(self.skipped(10), 1)

    def test_candidate_after_previous_32_row_limit_is_offered(self):
        for index in range(40):
            self.add_process(pid=5000 + index, name=f"demo-{index}",
                             exe=f"/usr/bin/demo-{index}")
        self.add_process(
            pid=9000, name="dotnet", exe="/usr/share/dotnet/dotnet",
            cgroup=(f"0::/user.slice/user-{UID}.slice/user@{UID}.service/"
                    "app.slice/app-pinta-m13-test.scope\n"),
        )
        self.assertEqual(self.scan(), 41)
        offered = [self.library.mr_cand_name(index) for index in range(41)]
        self.assertIn(b"dotnet", offered)

    def test_candidate_exposes_snapshot_state_and_executable_basename_without_path(self):
        self.add_process(name="editor", exe="/home/alice/Private Projects/editor")
        self.assertEqual(self.scan(), 1)
        self.assertEqual(self.library.mr_cand_name(0), b"editor")
        self.assertEqual(self.library.mr_cand_app_name(0), b"editor")
        self.assertEqual(self.library.mr_cand_state(0), ord("S"))
        self.assertEqual(self.library.mr_cand_app_name(-1), b"")
        self.assertEqual(self.library.mr_cand_state(-1), -1)

    def test_selection_snapshot_survives_a_rescan_and_same_name_candidates(self):
        self.add_process(pid=4242, name="pinta", exe="/usr/bin/pinta-a")
        self.add_process(pid=4243, name="pinta", exe="/usr/bin/pinta-b")
        self.assertEqual(self.scan(), 2)
        pid = self.library.mr_cand_pid(1)
        values = self.select_snapshot(1, request_id=900001)
        token = values[4]
        scope = values[5]
        digest = values[6]
        self.assertGreater(token, 0)
        self.assertGreater(scope, 0)
        self.assertGreater(digest, 0)

        # A new list can have different rows and order. The confirmed target
        # remains the copied PID/start/UID/token, never "whatever is row 1 now".
        other_pid = self.library.mr_cand_pid(0)
        os.unlink(os.path.join(self.proc, str(other_pid), "status"))
        self.assertEqual(self.scan(), 1)
        self.assertEqual(self.library.mr_cand_pid(0), pid)
        self.assertEqual(
            self.library.mr_cand_snapshot_verify(*values),
            0,
        )
        other_values = list(values)
        other_values[1] = other_pid
        self.assertNotEqual(
            self.library.mr_cand_snapshot_verify(*other_values),
            0,
        )

    def test_snapshot_fails_closed_when_target_identity_changes(self):
        directory = self.add_process()
        self.assertEqual(self.scan(), 1)
        values = self.select_snapshot(0, request_id=900002)
        with open(os.path.join(directory, "cmdline"), "wb") as handle:
            handle.write(b"/usr/bin/other\0--fixture\0")
        self.assertNotEqual(self.library.mr_cand_snapshot_verify(*values), 0)

    def test_snapshot_fails_closed_when_scope_or_executable_changes(self):
        directory = self.add_process()
        self.assertEqual(self.scan(), 1)
        values = self.select_snapshot(0, request_id=900003)
        with open(os.path.join(directory, "cgroup"), "w", encoding="utf-8") as handle:
            handle.write(
                f"0::/user.slice/user-{UID}.slice/user@{UID}.service/"
                "app.slice/app-another.scope\n"
            )
        self.assertNotEqual(self.library.mr_cand_snapshot_verify(*values), 0)

    def test_snapshot_fails_closed_when_start_time_changes(self):
        directory = self.add_process()
        self.assertEqual(self.scan(), 1)
        values = self.select_snapshot(0, request_id=900004)
        with open(os.path.join(directory, "stat"), "w", encoding="utf-8") as handle:
            handle.write(stat_line(4242, "demo", 1, 78))
        self.assertNotEqual(self.library.mr_cand_snapshot_verify(*values), 0)

    def test_snapshot_fails_closed_when_mapping_summary_changes(self):
        directory = self.add_process()
        self.assertEqual(self.scan(), 1)
        values = self.select_snapshot(0, request_id=900005)
        self.write_mapping_files(directory, "00400000-00402000 r--p 00000000 00:00 0\n", 8)
        self.assertNotEqual(self.library.mr_cand_snapshot_verify(*values), 0)

    def test_region_digest_detects_same_sized_mapping_replacement(self):
        directory = self.add_process()
        self.assertEqual(self.scan(), 1)
        values = self.select_snapshot(0, request_id=900006)
        before_bytes = values[7]
        before_regions = values[8]
        self.write_mapping_files(directory, "00500000-00501000 r--p 00000000 00:00 0\n", 4)
        # Total bytes and region count are unchanged; only an exact map digest
        # detects that the user confirmed a different virtual range.
        self.assertEqual(before_bytes, 4096)
        self.assertEqual(before_regions, 1)
        self.assertNotEqual(self.library.mr_cand_snapshot_verify(*values), 0)

    def test_private_resident_change_invalidates_selected_snapshot(self):
        directory = self.add_process()
        self.assertEqual(self.scan(), 1)
        values = self.select_snapshot(0, request_id=900009)
        self.assertEqual(self.library.mr_cand_private_resident_bytes(0), 4096)
        self.write_mapping_files(directory, "00400000-00401000 r--p 00000000 00:00 0\n", 0)
        self.assertNotEqual(self.library.mr_cand_snapshot_verify(*values), 0)

    def test_malformed_smaps_is_reported_as_unreadable_not_kernel_thread(self):
        directory = self.add_process()
        with open(os.path.join(directory, "smaps"), "w", encoding="utf-8") as handle:
            handle.write("00400000-00401000 r--p 00000000 00:00 0\nPrivate_Clean: 0 kB\n")
        self.assertEqual(self.scan(), 0)
        self.assertEqual(self.skipped(5), 1)
        self.assertEqual(self.skipped(3), 0)

    def test_request_id_is_bound_to_snapshot_and_cleared_on_refresh(self):
        self.add_process()
        self.assertEqual(self.scan(), 1)
        values = self.select_snapshot(0, request_id=900007)
        wrong_request = list(values)
        wrong_request[0] += 1
        self.assertNotEqual(self.library.mr_cand_snapshot_verify(*wrong_request), 0)
        self.library.mr_cand_snapshot_clear()
        self.assertEqual(self.library.mr_cand_snapshot_request_id(), -1)
        self.assertNotEqual(self.library.mr_cand_snapshot_verify(*values), 0)

    def test_candidate_display_name_preserves_valid_utf8(self):
        self.add_process(name="编辑器", exe="/usr/bin/编辑器")
        self.assertEqual(self.scan(), 1)
        self.assertEqual(self.library.mr_cand_name(0).decode("utf-8"), "编辑器")
        self.assertEqual(self.library.mr_cand_app_name(0).decode("utf-8"), "编辑器")

    def test_candidate_display_strings_replace_controls(self):
        self.add_process(name="bad-name", exe="/usr/bin/app\x1b")
        with open(os.path.join(self.proc, "4242", "status"), "wb") as handle:
            handle.write(f"Name:\tbad\x1bname\nState:\tR\nUid:\t{UID}\t{UID}\t{UID}\t{UID}\n".encode())
        self.assertEqual(self.scan(), 1)
        self.assertEqual(self.library.mr_cand_name(0), b"bad?name")
        self.assertEqual(self.library.mr_cand_app_name(0), b"app?")

    def test_unknown_process_state_is_not_offered(self):
        self.add_process(state="")
        self.assertEqual(self.scan(), 0)
        self.assertEqual(self.skipped(5), 1)

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
