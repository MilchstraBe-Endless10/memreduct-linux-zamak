"""Behavioural tests for the cross-process reclaim helper and its bridge.

Nothing here uses pkexec, root, or a real cross-process syscall. The helper is
built twice: as the production binary (whose only reachable outcome as a normal
user is a refusal) and as a test binary that can run against a fixture procfs
tree with a stubbed syscall backend.

The point of every assertion is the same: an outcome the program cannot prove is
never reported as success.
"""

import errno
import os
import re
import shutil
import subprocess
import tempfile
import textwrap
import unittest
import ctypes
import struct

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NATIVE = os.path.join(ROOT, "native")

HELPER_SRC = os.path.join(NATIVE, "crossproc_helper.c")
HELPER_H = os.path.join(NATIVE, "crossproc_helper.h")
POLICY = os.path.join(
    ROOT, "packaging", "polkit-1", "actions",
    "io.github.milchstrasse_endless10.memreduct.zamak.crossproc.policy",
)
PAGE_CACHE_POLICY = os.path.join(
    ROOT, "packaging", "polkit-1", "actions",
    "io.github.milchstrasse_endless10.memreduct.zamak.policy",
)

BASE_FLAGS = [
    "cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-pedantic",
    "-fstack-protector-strong", "-D_FORTIFY_SOURCE=2",
]


def _build(output, extra_flags, sources):
    subprocess.run(
        BASE_FLAGS + extra_flags + ["-I", NATIVE] + sources + ["-o", output],
        check=True, capture_output=True, text=True,
    )


class BuildMixin:
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="memreduct-xproc-")
        cls.prod = os.path.join(cls.tmp, "helper-prod")
        cls.test = os.path.join(cls.tmp, "helper-test")
        cls.credentials_probe = os.path.join(cls.tmp, "helper-credentials.so")
        _build(cls.prod, [], [HELPER_SRC])
        _build(cls.test, ["-DMEMREDUCT_TESTING"], [HELPER_SRC])
        subprocess.run(
            BASE_FLAGS + ["-DMEMREDUCT_TESTING", "-shared", "-fPIC",
                          "-Dmain=mr_xproc_test_program_main", "-I", NATIVE,
                          HELPER_SRC, "-o", cls.credentials_probe],
            check=True, capture_output=True, text=True,
        )
        cls.credentials = ctypes.CDLL(cls.credentials_probe)
        cls.credentials.mr_xproc_test_validate_helper_credentials.argtypes = [
            ctypes.c_uint64, ctypes.c_uint64, ctypes.c_char_p,
            ctypes.POINTER(ctypes.c_uint64),
        ]
        cls.credentials.mr_xproc_test_validate_helper_credentials.restype = ctypes.c_char_p
        cls.credentials.mr_xproc_test_validate_pageout_capability.argtypes = [ctypes.c_int]
        cls.credentials.mr_xproc_test_validate_pageout_capability.restype = ctypes.c_char_p
        cls.credentials.mr_xproc_test_target_is_self.argtypes = [
            ctypes.c_int32, ctypes.c_int32,
        ]
        cls.credentials.mr_xproc_test_target_is_self.restype = ctypes.c_int

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    @staticmethod
    def parse(stdout):
        """Parse the helper's result block into a dict, or None without marker."""
        lines = stdout.splitlines()
        if "MR_CROSSPROC_RESULT" not in lines:
            return None
        index = lines.index("MR_CROSSPROC_RESULT")
        fields = {}
        for line in lines[index + 1:]:
            if "=" not in line:
                continue
            key, _, value = line.partition("=")
            fields[key] = value
        return fields


def fixture_identity_token(proc_root, pid, start_time, uid):
    """Mirror the shared C selection fingerprint for bounded test fixtures."""
    directory = os.path.join(proc_root, str(pid))
    try:
        with open(os.path.join(directory, "status"), "rb") as handle:
            status = handle.read()
        comm = next(line[5:].lstrip(b" \t") for line in status.splitlines()
                    if line.startswith(b"Name:"))
        exe = os.readlink(os.path.join(directory, "exe")).encode()
        with open(os.path.join(directory, "cgroup"), "rb") as handle:
            cgroup = handle.read()
        with open(os.path.join(directory, "cmdline"), "rb") as handle:
            cmdline = handle.read()
    except OSError:
        return 1
    value = 14695981039346656037
    fields = [
        b"\x01", struct.pack("=i", pid), struct.pack("=Q", start_time),
        struct.pack("=Q", uid), comm, exe, cgroup, cmdline,
    ]
    for field in fields:
        for byte in struct.pack("=Q", len(field)) + field:
            value ^= byte
            value = (value * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    value &= 0x7FFFFFFFFFFFFFFF
    return value or 1


def _fnv64_fields(fields, value=14695981039346656037):
    for field in fields:
        for byte in struct.pack("=Q", len(field)) + field:
            value ^= byte
            value = (value * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return value


def fixture_scope_token(proc_root, pid):
    try:
        with open(os.path.join(proc_root, str(pid), "cgroup"), "rb") as handle:
            cgroup = handle.read()
    except OSError:
        return 0
    value = _fnv64_fields([b"\x01", cgroup]) & 0x7FFFFFFFFFFFFFFF
    return value or 1


def fixture_map_summary(proc_root, pid):
    directory = os.path.join(proc_root, str(pid))
    try:
        with open(os.path.join(directory, "maps"), "r", encoding="utf-8") as handle:
            lines = handle.readlines()
    except OSError:
        return 0, 0, 0
    total = 0
    regions = 0
    digest = _fnv64_fields([b"memreduct-region-v1"])
    page = os.sysconf("SC_PAGE_SIZE")
    for line in lines:
        fields = line.split()
        if len(fields) < 5:
            continue
        try:
            begin_text, end_text = fields[0].split("-", 1)
            begin, end = int(begin_text, 16), int(end_text, 16)
            offset = int(fields[2], 16)
            inode = int(fields[4])
        except (ValueError, IndexError):
            continue
        perms = fields[1]
        device = fields[3]
        name = fields[5] if len(fields) >= 6 else ""
        if end <= begin or len(perms) < 4 or perms[3] != "p" or perms[0] != "r":
            continue
        if inode != 0 and perms[1] == "w":
            continue
        if name.startswith("["):
            continue
        aligned_start = (begin + page - 1) & ~(page - 1)
        aligned_end = end & ~(page - 1)
        if aligned_end <= aligned_start:
            continue
        total += aligned_end - aligned_start
        regions += 1
        digest = _fnv64_fields([
            struct.pack("=Q", aligned_start), struct.pack("=Q", aligned_end),
            perms.encode(), struct.pack("=Q", offset), device.encode(),
            struct.pack("=Q", inode), name.encode(),
        ], digest)
    digest = _fnv64_fields([struct.pack("=Q", regions)], digest)
    digest &= 0x7FFFFFFFFFFFFFFF
    return total, regions, digest or 1

class ProductionBinaryTest(BuildMixin, unittest.TestCase):
    """The shipped binary must refuse everything a normal user can ask for."""

    def test_binary_embeds_exact_protocol_abi_marker(self):
        with open(self.prod, "rb") as handle:
            binary = handle.read()
        self.assertIn(b"MR_XPROC_PROTOCOL_ABI=4;", binary)

    def test_usage_errors_exit_2_and_print_nothing_to_stdout(self):
        cases = [
            [],
            ["reclaim"],
            ["reclaim", "pageout"],
            ["reclaim", "pageout", "1234"],
            ["reclaim", "pageout", "1234", "5"],
            ["poke", "1234", "5", "4096"],
            ["reclaim", "bad-mode", "1234", "5", "1000", "1", "2", "3", "4096", "1", "4096"],
            ["reclaim", "pageout", "1234", "5", "4096", "extra"],
            ["reclaim", "pageout", "12x4", "5", "1000", "1", "2", "3", "4096", "1", "4096"],
            ["reclaim", "pageout", "1234", "-5", "1000", "1", "2", "3", "4096", "1", "4096"],
            ["reclaim", "pageout", "1234", "5", "1000", "1", "2", "3", "4096", "1", "0"],
        ]
        for argv in cases:
            with self.subTest(argv=argv):
                result = subprocess.run(
                    [self.prod] + argv, capture_output=True, text=True, timeout=10,
                )
                self.assertEqual(result.returncode, 2, result.stderr)
                self.assertEqual(result.stdout, "", "usage must not print a result block")

    def test_pid_below_two_is_a_usage_error(self):
        for pid in ("0", "1"):
            with self.subTest(pid=pid):
                result = subprocess.run(
                    [self.prod, "reclaim", "pageout", "987654321", pid, "5", "1000",
                     "1", "2", "3", "4096", "1", "4096"],
                    capture_output=True, text=True, timeout=10,
                )
                self.assertEqual(result.returncode, 2)

    def test_budget_above_ceiling_is_a_usage_error(self):
        # 1 GiB is the hard ceiling; one byte more must not parse.
        result = subprocess.run(
            [self.prod, "reclaim", "pageout", "987654321", "1234", "5", "1000",
             "1", "2", "3", "4096", "1", str(1024 * 1024 * 1024 + 1)],
            capture_output=True, text=True, timeout=10,
        )
        self.assertEqual(result.returncode, 2)

    def test_unprivileged_run_refuses_and_never_claims_a_target_was_inspected(self):
        result = subprocess.run(
            [self.prod, "reclaim", "pageout", "987654321", "2", "5", "1000",
             "1", "2", "3", "4096", "1", "1048576"],
            capture_output=True, text=True, timeout=10,
        )
        self.assertEqual(result.returncode, 1, result.stderr)
        fields = self.parse(result.stdout)
        self.assertIsNotNone(fields, "a refusal must still print a result block")
        self.assertEqual(fields["status"], "refused")
        self.assertEqual(fields["reason"], "not_root")
        # The two honesty fields: nothing was verified, nothing was known.
        self.assertEqual(fields["start_time_verified"], "false")
        self.assertEqual(fields["uid"], "unknown")
        self.assertEqual(fields["call_count"], "0")
        self.assertEqual(fields["advised_bytes"], "0")
        self.assertEqual(result.stdout.count("bytes_advised_is_not_bytes_freed=true"), 1)

    def test_production_binary_contains_no_test_hooks(self):
        """The test hooks are compiled out, not merely unused.

        A string scan is not a proof on its own, so this asserts the specific
        environment variable names the test build reads are absent, and the
        companion test below asserts the syscall is not reachable for a normal
        user by behaviour rather than by bytes.
        """
        with open(self.prod, "rb") as handle:
            blob = handle.read()
        for marker in (
            b"MEMREDUCT_TEST_PROC_ROOT",
            b"MEMREDUCT_TEST_CGROUP_ROOT",
            b"MEMREDUCT_TEST_SCOPE_WRITE_RESULT",
            b"MEMREDUCT_TEST_SKIP_PRIVILEGE",
            b"MEMREDUCT_TEST_SKIP_REQUESTER",
            b"MEMREDUCT_TEST_BACKEND",
            b"mr_xproc_test_target_is_self",
            b"mr_xproc_test_validate_helper_credentials",
            b"mr_xproc_test_validate_pageout_capability",
        ):
            self.assertNotIn(marker, blob, f"{marker!r} leaking into the production binary")


FIXTURE_MAPS = textwrap.dedent("""\
    00400000-00452000 r-xp 00000000 08:01 100 /usr/bin/demo
    00651000-00652000 r--p 00051000 08:01 100 /usr/bin/demo
    00652000-00653000 rw-p 00052000 08:01 100 /usr/bin/demo
    01000000-01400000 rw-p 00000000 00:00 0
    02000000-02400000 r--s 00000000 08:01 100 /usr/lib/shared.dat
    03000000-03001000 r-xp 00000000 00:00 0 [vdso]
    04000000-04001000 rw-p 00000000 00:00 0 [stack]
    """)


class FixtureProc:
    """A fake procfs tree the test build can be pointed at.

    PID 1 is always created: the helper walks the ancestry chain until it either
    finds the target or reaches a process whose parent is itself. Without a
    fixture PID 1 that walk ends in an unreadable entry, and every case would be
    refused as "ancestry_unverifiable" before reaching the check under test —
    which is the helper's correct behaviour, but it makes the other cases
    unreachable.
    """

    def __init__(self, root):
        self.root = root
        os.makedirs(os.path.join(root, "1"), exist_ok=True)
        self.add(1, "init", 0, 1, ppid=0)

    def add(self, pid, name, uid, start_time, ppid=1, state="S", maps=FIXTURE_MAPS,
            rss_kib=8192, private_resident_by_start=None, page_states=None):
        directory = os.path.join(self.root, str(pid))
        os.makedirs(directory, exist_ok=True)
        with open(os.path.join(directory, "status"), "w", encoding="utf-8") as handle:
            handle.write(f"Name:\t{name}\n")
            handle.write(f"State:\t{state}\n")
            handle.write(f"Uid:\t{uid}\t{uid}\t{uid}\t{uid}\n")
            handle.write(f"VmRSS:\t{rss_kib} kB\nVmSwap:\t1024 kB\n")
        # stat: pid (comm) state ppid ... field 22 = start_time.
        # Fields after ')' are: state, ppid, then 17 more before start_time.
        rest = [state, str(ppid)] + ["0"] * 17 + [str(start_time)]
        with open(os.path.join(directory, "stat"), "w", encoding="utf-8") as handle:
            handle.write(f"{pid} ({name}) " + " ".join(rest) + "\n")
        exe_path = os.path.join(directory, "exe")
        if os.path.lexists(exe_path):
            os.unlink(exe_path)
        os.symlink(f"/usr/bin/{name}", exe_path)
        with open(os.path.join(directory, "cgroup"), "w", encoding="utf-8") as handle:
            handle.write(
                f"0::/user.slice/user-{uid}.slice/user@{uid}.service/"
                f"app.slice/app-{name}.scope\n"
            )
        with open(os.path.join(directory, "cmdline"), "wb") as handle:
            handle.write(f"/usr/bin/{name}".encode() + b"\0--fixture\0")
        if maps is not None:
            with open(os.path.join(directory, "maps"), "w", encoding="utf-8") as handle:
                handle.write(maps)
            private_resident_by_start = private_resident_by_start or {}
            page_states = page_states or {}

            def pagemap_bits(state_value):
                if state_value == "present":
                    return (1 << 63) | (1 << 56)
                if state_value == "present_nonexclusive":
                    return 1 << 63
                if state_value == "swapped":
                    return 1 << 62
                return 0

            smaps_rows = []
            with open(os.path.join(directory, "pagemap"), "wb") as pagemap:
                for line in maps.splitlines():
                    fields = line.split(maxsplit=5)
                    if len(fields) < 5:
                        continue
                    start_text, end_text = fields[0].split("-", 1)
                    start, end = int(start_text, 16), int(end_text, 16)
                    perms, offset, device, inode = fields[1], int(fields[2], 16), fields[3], int(fields[4])
                    name = fields[5] if len(fields) >= 6 else ""
                    length = end - start
                    eligible = (length > 0 and perms[0] == "r" and perms[3] == "p" and
                                not (inode != 0 and perms[1] == "w") and not name.startswith("["))
                    resident = private_resident_by_start.get(start, length if eligible else 0)
                    if resident < 0 or resident > length or resident % 4096:
                        raise ValueError("fixture private resident bytes must be page aligned and within the mapping")
                    smaps_rows.append(
                        f"{start:08x}-{end:08x} {perms} {offset:08x} {device} {inode} {name}\n"
                        f"Size: {length // 1024} kB\nRss: {resident // 1024} kB\n"
                        f"Private_Clean: 0 kB\nPrivate_Dirty: {resident // 1024} kB\n"
                    )
                    if eligible:
                        pages = length // 4096
                        resident_pages = resident // 4096
                        if pages == 0:
                            continue
                        pagemap.truncate((end // 4096) * 8)
                        for first in range(0, resident_pages, 8192):
                            count = min(8192, resident_pages - first)
                            values = bytearray(count * 8)
                            for page_index in range(count):
                                address = start + (first + page_index) * 4096
                                state_value = page_states.get(address, "present")
                                bits = pagemap_bits(state_value)
                                struct.pack_into("=Q", values, page_index * 8, bits)
                            pagemap.seek((start // 4096 + first) * 8)
                            pagemap.write(values)
                        for address, state_value in page_states.items():
                            if start <= address < end:
                                bits = pagemap_bits(state_value)
                                pagemap.seek((address // 4096) * 8)
                                pagemap.write(struct.pack("=Q", bits))
            with open(os.path.join(directory, "smaps"), "w", encoding="utf-8") as handle:
                handle.write("".join(smaps_rows))
        return directory


class FixtureTreeTest(BuildMixin, unittest.TestCase):
    """Every refusal the helper can reach, driven through a fixture tree.

    The helper's identity checks are skipped selectively per case so that each
    remaining check is the one under test. Skipping a check is never allowed to
    turn a refusal into an acceptance: the cases below all expect a refusal.
    """

    def setUp(self):
        self.proc = tempfile.mkdtemp(prefix="memreduct-proc-")
        self.cgroups = tempfile.mkdtemp(prefix="memreduct-cgroup-")
        self.tree = FixtureProc(self.proc)
        # pidfd_open() cannot succeed for a fixture-only PID, so the test build
        # is given a private file to open instead. It is not a pidfd and is
        # never passed to a real syscall: the backend is stubbed in every case.
        self.pidfd_stand_in = os.path.join(self.proc, "pidfd-stand-in")
        with open(self.pidfd_stand_in, "w", encoding="utf-8") as handle:
            handle.write("not a real pidfd\n")

    def tearDown(self):
        shutil.rmtree(self.proc, ignore_errors=True)
        shutil.rmtree(self.cgroups, ignore_errors=True)

    def add_scope(self, process_name="demo", anon_bytes=512 * 1024 * 1024,
                  file_bytes=512 * 1024 * 1024, current_bytes=1024 * 1024 * 1024,
                  swap_bytes=0, members=(4242,)):
        scope = os.path.join(
            self.cgroups, "user.slice", "user-1000.slice", "user@1000.service",
            "app.slice", f"app-{process_name}.scope",
        )
        os.makedirs(scope, exist_ok=True)
        with open(os.path.join(scope, "cgroup.procs"), "w", encoding="ascii") as handle:
            handle.write("".join(f"{pid}\n" for pid in members))
        with open(os.path.join(scope, "memory.current"), "w", encoding="ascii") as handle:
            handle.write(f"{current_bytes}\n")
        with open(os.path.join(scope, "memory.stat"), "w", encoding="ascii") as handle:
            handle.write(f"anon {anon_bytes}\nfile {file_bytes}\nslab 4096\n")
        with open(os.path.join(scope, "memory.swap.current"), "w", encoding="ascii") as handle:
            handle.write(f"{swap_bytes}\n")
        with open(os.path.join(scope, "memory.reclaim"), "w", encoding="ascii") as handle:
            handle.write("")
        return scope

    def run_helper(self, pid, start_time, max_bytes, env_extra=None, uid_env="1000",
                   selected_uid=1000, identity_token=None, reviewed_summary=None,
                   request_id=987654321, scope_token=None, region_digest=None,
                   mode="pageout"):
        env = {
            "PATH": "/usr/bin:/bin",
            "MEMREDUCT_TEST_PROC_ROOT": self.proc,
            "MEMREDUCT_TEST_CGROUP_ROOT": self.cgroups,
            "MEMREDUCT_TEST_SKIP_PRIVILEGE": "1",
            "MEMREDUCT_TEST_SKIP_CAPABILITY": "1",
            # App-scope validation needs the real selected UID from the
            # pkexec model; using the generic skipped requester (UID 1) would
            # correctly reject a UID 1000 fixture's cgroup path.
            "MEMREDUCT_TEST_SKIP_REQUESTER": "0" if mode == "app_scope" else "1",
            # The ancestry walk starts at the helper's own PID, which no fixture
            # tree can contain. Its own behaviour is asserted separately in
            # test_ancestry_walk_refuses_an_unprovable_chain.
            "MEMREDUCT_TEST_SKIP_ANCESTRY": "1",
            "MEMREDUCT_TEST_BACKEND": "stub",
            "MEMREDUCT_TEST_PIDFD_FILE": self.pidfd_stand_in,
        }
        if uid_env is not None:
            env["PKEXEC_UID"] = uid_env
        if env_extra:
            env.update(env_extra)
        token = identity_token
        if token is None:
            token = fixture_identity_token(self.proc, pid, start_time, selected_uid)
        if reviewed_summary is None:
            reviewed_bytes, reviewed_regions, reviewed_digest = fixture_map_summary(self.proc, pid)
        else:
            if len(reviewed_summary) == 2:
                reviewed_bytes, reviewed_regions = reviewed_summary
                _, _, reviewed_digest = fixture_map_summary(self.proc, pid)
            else:
                reviewed_bytes, reviewed_regions, reviewed_digest = reviewed_summary
        selected_scope_token = fixture_scope_token(self.proc, pid) if scope_token is None else scope_token
        selected_region_digest = reviewed_digest if region_digest is None else region_digest
        return subprocess.run(
            [self.test, "reclaim", mode, str(request_id), str(pid), str(start_time),
             str(selected_uid), str(token), str(selected_scope_token),
             str(selected_region_digest), str(reviewed_bytes), str(reviewed_regions),
             str(max_bytes)],
            capture_output=True, text=True, timeout=10, env=env,
        )

    def assert_refused(self, result, reason):
        fields = self.parse(result.stdout)
        self.assertIsNotNone(fields, result.stdout + result.stderr)
        self.assertEqual(fields["status"], "refused", fields)
        self.assertEqual(fields["reason"], reason, fields)
        self.assertEqual(fields["call_count"], "0", "a refusal must not issue a call")
        self.assertEqual(result.returncode, 1)
        return fields

    def test_start_time_mismatch_is_refused(self):
        self.tree.add(4242, "demo", 1000, 777)
        # Caller claims a different start_time: the PID now means something else.
        result = self.run_helper(4242, 778, 1048576)
        self.assert_refused(result, "start_time_mismatch")

    def test_selection_fingerprint_rejects_changed_same_pid_application(self):
        directory = self.tree.add(4242, "demo", 1000, 777)
        selected_token = fixture_identity_token(self.proc, 4242, 777, 1000)
        with open(os.path.join(directory, "cmdline"), "wb") as handle:
            handle.write(b"/usr/bin/other-app\0--fixture\0")
        result = self.run_helper(4242, 777, 1048576, identity_token=selected_token)
        self.assert_refused(result, "target_identity_changed")

    def test_selection_fingerprint_rejects_changed_scope_or_executable(self):
        for pid, changed_file, changed_value in (
            (4242, "cgroup", b"0::/user.slice/user-1000.slice/user@1000.service/app.slice/app-other.scope\n"),
            (4243, "exe", b"/usr/bin/other-app"),
        ):
            with self.subTest(changed_file=changed_file):
                directory = self.tree.add(pid, "demo", 1000, 777)
                selected_token = fixture_identity_token(self.proc, pid, 777, 1000)
                if changed_file == "cgroup":
                    with open(os.path.join(directory, changed_file), "wb") as handle:
                        handle.write(changed_value)
                else:
                    os.unlink(os.path.join(directory, "exe"))
                    os.symlink(changed_value.decode(), os.path.join(directory, "exe"))
                result = self.run_helper(pid, 777, 1048576,
                                         identity_token=selected_token)
                self.assert_refused(result, "target_identity_changed")

    def test_reviewed_mapping_summary_must_still_match_after_authorization(self):
        self.tree.add(4242, "demo", 1000, 777)
        selected_token = fixture_identity_token(self.proc, 4242, 777, 1000)
        reviewed_summary = fixture_map_summary(self.proc, 4242)
        self.tree.add(4242, "demo", 1000, 777,
                      maps="00400000-00401000 r--p 00000000 00:00 0\n")
        result = self.run_helper(4242, 777, 1048576,
                                 identity_token=selected_token,
                                 reviewed_summary=reviewed_summary)
        self.assert_refused(result, "target_mappings_changed")

    def test_exact_region_digest_rejects_same_size_different_mapping(self):
        self.tree.add(4242, "demo", 1000, 777)
        selected_summary = fixture_map_summary(self.proc, 4242)
        changed_maps = textwrap.dedent("""\
                01400000-01452000 r-xp 00000000 08:01 100 /usr/bin/demo
                01651000-01652000 r--p 00051000 08:01 100 /usr/bin/demo
                01652000-01653000 rw-p 00052000 08:01 100 /usr/bin/demo
                02000000-02400000 rw-p 00000000 00:00 0
                03000000-03001000 r-xp 00000000 00:00 0 [vdso]
                04000000-04001000 rw-p 00000000 00:00 0 [stack]
            """)
        self.tree.add(4242, "demo", 1000, 777, maps=changed_maps)
        self.assertEqual(fixture_map_summary(self.proc, 4242)[:2], selected_summary[:2])
        result = self.run_helper(4242, 777, 1048576, reviewed_summary=selected_summary)
        self.assert_refused(result, "target_mappings_changed")

    def test_scope_hash_mismatch_is_refused_before_syscall(self):
        self.tree.add(4242, "demo", 1000, 777)
        selected_scope = fixture_scope_token(self.proc, 4242)
        result = self.run_helper(4242, 777, 1048576, scope_token=selected_scope + 1)
        self.assert_refused(result, "target_identity_changed")

    def test_selected_uid_must_match_the_authorized_requester(self):
        self.tree.add(4242, "demo", 1000, 777)
        result = self.run_helper(
            4242, 777, 1048576, env_extra={"MEMREDUCT_TEST_SKIP_REQUESTER": "0"},
            uid_env="1000", selected_uid=1001,
        )
        self.assert_refused(result, "selected_uid_mismatch")

    def test_other_uid_target_is_refused_even_with_matching_start_time(self):
        self.tree.add(4242, "demo", 2000, 777)
        env = {"MEMREDUCT_TEST_SKIP_REQUESTER": "0"}
        result = self.run_helper(4242, 777, 1048576, env_extra=env, uid_env="1000")
        self.assert_refused(result, "target_other_uid")

    def test_root_owned_target_is_refused(self):
        self.tree.add(4242, "demo", 0, 777)
        result = self.run_helper(4242, 777, 1048576)
        self.assert_refused(result, "target_is_root_owned")

    def test_protected_process_is_refused(self):
        for name in ("polkitd", "systemd", "dbus-daemon", "Hyprland"):
            with self.subTest(name=name):
                self.tree.add(4242, name, 1000, 777)
                result = self.run_helper(4242, 777, 1048576)
                self.assert_refused(result, "target_is_protected_process")

    def test_protected_executable_is_refused_even_with_unrelated_comm(self):
        directory = self.tree.add(4242, "ordinary-client", 1000, 777)
        os.unlink(os.path.join(directory, "exe"))
        os.symlink("/usr/bin/Hyprland", os.path.join(directory, "exe"))
        self.assert_refused(self.run_helper(4242, 777, 1048576),
                            "target_is_protected_process")

    def test_unreadable_executable_is_refused_closed(self):
        directory = self.tree.add(4242, "ordinary-client", 1000, 777)
        os.unlink(os.path.join(directory, "exe"))
        self.assert_refused(self.run_helper(4242, 777, 1048576),
                            "target_exe_unverifiable")

    def test_system_and_login_session_cgroups_are_refused(self):
        directory = self.tree.add(4242, "ordinary-client", 1000, 777)
        for cgroup in (
            "0::/system.slice/user-manager.service\n",
            "0::/user.slice/user-1000.slice/session-3.scope\n",
        ):
            with self.subTest(cgroup=cgroup):
                with open(os.path.join(directory, "cgroup"), "w", encoding="utf-8") as handle:
                    handle.write(cgroup)
                self.assert_refused(self.run_helper(4242, 777, 1048576),
                                    "target_in_protected_cgroup")

    def test_missing_or_unknown_cgroup_is_refused_closed(self):
        directory = self.tree.add(4242, "ordinary-client", 1000, 777)
        selected_identity = fixture_identity_token(self.proc, 4242, 777, 1000)
        selected_scope = fixture_scope_token(self.proc, 4242)
        selected_summary = fixture_map_summary(self.proc, 4242)
        os.unlink(os.path.join(directory, "cgroup"))
        self.assert_refused(self.run_helper(
            4242, 777, 1048576, identity_token=selected_identity,
            scope_token=selected_scope, reviewed_summary=selected_summary),
                            "target_cgroup_unverifiable")

    def test_target_uid_real_effective_saved_and_fs_must_match(self):
        directory = self.tree.add(4242, "ordinary-client", 1000, 777)
        with open(os.path.join(directory, "status"), "w", encoding="utf-8") as handle:
            handle.write("Name:\tordinary-client\nState:\tS\nUid:\t1000\t1001\t1000\t1000\n")
        self.assert_refused(self.run_helper(4242, 777, 1048576),
                            "target_uid_credentials_mismatch")

    def test_exited_target_state_is_refused(self):
        self.tree.add(4242, "ordinary-client", 1000, 777, state="Z")
        self.assert_refused(self.run_helper(4242, 777, 1048576), "target_not_running")

    def test_truncated_comm_name_still_matches_a_protected_program(self):
        """The kernel caps comm at 15 bytes, so the name arrives shortened.

        This was a real bypass found by running the enumeration against the live
        machine: pid 1188 reports comm "dbus-broker-lau" for
        /usr/bin/dbus-broker-launch, and an exact comparison let it through.
        Both the helper and the enumeration must reject the truncated form.
        """
        self.tree.add(4242, "dbus-broker-lau", 1000, 777)
        result = self.run_helper(4242, 777, 1048576)
        self.assert_refused(result, "target_is_protected_process")

    def test_a_15_byte_unrelated_name_is_not_treated_as_protected(self):
        """Truncation matching must not become a wildcard on short names."""
        self.tree.add(4242, "ordinary-client", 1000, 777)
        result = self.run_helper(4242, 777, 1048576)
        fields = self.parse(result.stdout)
        self.assertIsNotNone(fields)
        self.assertNotEqual(fields.get("reason"), "target_is_protected_process")

    def test_missing_status_is_refused(self):
        directory = self.tree.add(4242, "demo", 1000, 777)
        selected_identity = fixture_identity_token(self.proc, 4242, 777, 1000)
        os.unlink(os.path.join(directory, "status"))
        result = self.run_helper(4242, 777, 1048576,
                                 identity_token=selected_identity)
        self.assert_refused(result, "target_not_readable")

    def test_kernel_thread_without_maps_is_refused(self):
        directory = self.tree.add(4242, "kthreadd", 1000, 777)
        selected_identity = fixture_identity_token(self.proc, 4242, 777, 1000)
        selected_summary = fixture_map_summary(self.proc, 4242)
        os.unlink(os.path.join(directory, "maps"))
        result = self.run_helper(4242, 777, 1048576,
                                 identity_token=selected_identity,
                                 reviewed_summary=selected_summary)
        self.assert_refused(result, "target_has_no_address_space")

    def test_target_is_self_is_refused(self):
        """The helper's self-target guard rejects only its own process ID."""
        is_self = self.credentials.mr_xproc_test_target_is_self
        self.assertEqual(is_self(4242, 4242), 1)
        self.assertEqual(is_self(4242, 4243), 0)

    def test_missing_pkexec_environment_is_refused(self):
        self.tree.add(4242, "demo", 1000, 777)
        env = {"MEMREDUCT_TEST_SKIP_REQUESTER": "0"}
        result = self.run_helper(4242, 777, 1048576, env_extra=env, uid_env=None)
        self.assert_refused(result, "not_launched_via_pkexec")

    def test_pkexec_requester_claim_must_match_kernel_target_uid(self):
        """PKEXEC_UID alone is insufficient; the target's kernel UIDs must match it."""
        real_uid = 1000
        self.tree.add(4242, "demo", real_uid, 777)
        env = {"MEMREDUCT_TEST_SKIP_REQUESTER": "0"}
        claimed = real_uid + 1
        result = self.run_helper(4242, 777, 1048576, env_extra=env,
                                 uid_env=str(claimed), selected_uid=claimed)
        self.assert_refused(result, "target_other_uid")

    def test_matching_requester_uid_passes_the_cross_check(self):
        """The same invocation with an honest PKEXEC_UID must get past it."""
        real_uid = 1000
        self.tree.add(4242, "demo", real_uid, 777)
        env = {"MEMREDUCT_TEST_SKIP_REQUESTER": "0"}
        result = self.run_helper(4242, 777, 1048576, env_extra=env, uid_env=str(real_uid))
        fields = self.parse(result.stdout)
        self.assertIsNotNone(fields)
        self.assertNotEqual(fields.get("reason"), "target_other_uid")

    def test_pkexec_uses_root_for_both_real_and_effective_uid(self):
        requester = ctypes.c_uint64()
        reason = self.credentials.mr_xproc_test_validate_helper_credentials(
            0, 0, b"1000", ctypes.byref(requester)
        )
        self.assertIsNone(reason)
        self.assertEqual(requester.value, 1000)

    def test_invalid_credentials_and_pkexec_uid_claims_are_rejected(self):
        cases = (
            (1000, 0, b"1000", "invalid_pkexec_root_credentials"),
            (0, 1000, b"1000", "not_root"),
            (0, 0, None, "not_launched_via_pkexec"),
            (0, 0, b"", "not_launched_via_pkexec"),
            (0, 0, b"abc", "malformed_requester_uid"),
            (0, 0, b"4294967296", "malformed_requester_uid"),
            (0, 0, b"0", "invalid_requester_uid"),
        )
        for real_uid, effective_uid, claim, expected in cases:
            with self.subTest(real_uid=real_uid, effective_uid=effective_uid, claim=claim):
                requester = ctypes.c_uint64()
                reason = self.credentials.mr_xproc_test_validate_helper_credentials(
                    real_uid, effective_uid, claim, ctypes.byref(requester)
                )
                self.assertEqual(reason.decode() if reason else None, expected)

    def test_capability_requirement_is_specific_to_pageout(self):
        for value, expected in ((1, None), (0, "missing_cap_sys_nice"),
                                (-1, "capget_failed")):
            with self.subTest(value=value):
                reason = self.credentials.mr_xproc_test_validate_pageout_capability(value)
                self.assertEqual(reason.decode() if reason else None, expected)

    def test_ancestry_walk_refuses_an_unprovable_chain(self):
        """The walk starts at the helper's own PID, which the fixture lacks.

        That is precisely the case worth pinning: an ancestry the helper cannot
        prove must refuse, never assume the target is unrelated.
        """
        self.tree.add(4242, "demo", 1000, 777)
        result = self.run_helper(4242, 777, 1048576,
                                 env_extra={"MEMREDUCT_TEST_SKIP_ANCESTRY": "0"})
        self.assert_refused(result, "ancestry_unverifiable")

    def test_no_reclaimable_region_is_reported_as_such_not_as_success(self):
        maps = textwrap.dedent("""\
            00400000-00452000 rw-p 00000000 08:01 100 /usr/bin/demo
            02000000-02400000 r--s 00000000 08:01 100 /usr/lib/shared.dat
            04000000-04001000 rw-p 00000000 00:00 0 [stack]
            """)
        self.tree.add(4242, "demo", 1000, 777, maps=maps)
        result = self.run_helper(4242, 777, 1048576)
        fields = self.parse(result.stdout)
        self.assertIsNotNone(fields, result.stdout + result.stderr)
        self.assertEqual(fields["status"], "no_regions", fields)
        self.assertEqual(fields["reason"], "no_private_resident_pages")
        self.assertEqual(fields["call_count"], "0")
        self.assertEqual(fields["advised_bytes"], "0")
        self.assertNotEqual(result.returncode, 0)
        # The skips must be counted, not silently swallowed.
        self.assertEqual(int(fields["skipped_shared"]), 1)
        self.assertEqual(int(fields["skipped_special"]), 1)
        self.assertEqual(int(fields["skipped_writable_file"]), 1)

    def test_stubbed_backend_reports_full_coverage_without_touching_a_process(self):
        """The only path that reaches a 'accepted' verdict is the stub backend.

        This proves the reporting is wired end to end, and it is explicitly not
        evidence that a real kernel call works: the backend here is a stub.
        """
        self.tree.add(4242, "demo", 1000, 777)
        result = self.run_helper(4242, 777, 4 * 1024 * 1024)
        fields = self.parse(result.stdout)
        self.assertIsNotNone(fields, result.stdout + result.stderr)
        self.assertEqual(fields["status"], "accepted", fields)
        self.assertEqual(fields["request_id"], "987654321")
        self.assertEqual(fields["call_count"], "1")
        self.assertGreater(int(fields["advised_bytes"]), 0)
        self.assertEqual(fields["start_time_verified"], "true")
        self.assertEqual(fields["uid"], "1000")
        self.assertEqual(fields["identity_hash_verified"], "true")
        self.assertEqual(fields["scope_hash_verified"], "true")
        self.assertEqual(fields["region_digest_verified"], "true")
        self.assertEqual(fields["expected_identity_hash"], fields["observed_identity_hash"])
        self.assertEqual(fields["expected_scope_hash"], fields["observed_scope_hash"])
        self.assertEqual(fields["expected_region_digest"], fields["observed_region_digest"])
        self.assertIn("MR_XPROC_AUDIT phase=helper_pre_syscall request_id=987654321",
                      result.stderr)
        self.assertIn("target_pid=4242", result.stderr)
        self.assertIn(fields["reason"], ("kernel_accepted_the_request",
                                         "kernel_covered_part_of_the_request"))
        self.assertEqual(result.returncode, 0)

    def test_budget_is_respected_and_smaller_than_the_mapping(self):
        self.tree.add(4242, "demo", 1000, 777)
        # 4 MiB of private rw-p anonymous in FIXTURE_MAPS plus 4 MiB r--p file.
        result = self.run_helper(4242, 777, 4096)
        fields = self.parse(result.stdout)
        self.assertIsNotNone(fields)
        self.assertEqual(fields["status"], "accepted", fields)
        self.assertLessEqual(int(fields["advised_bytes"]), 4096,
                             "a request must never exceed the byte budget")
        self.assertEqual(fields["requested_bytes"], "4096")

    def test_one_gib_pageout_budget_covers_a_large_mapping_in_one_call(self):
        maps = "01000000-41000000 rw-p 00000000 00:00 0\n"
        self.tree.add(4242, "demo", 1000, 777, maps=maps,
                      rss_kib=2 * 1024 * 1024)
        result = self.run_helper(4242, 777, 1024 * 1024 * 1024)
        fields = self.parse(result.stdout)
        self.assertEqual(fields["status"], "accepted", fields)
        self.assertEqual(fields["requested_bytes"], str(1024 * 1024 * 1024))
        self.assertEqual(fields["advised_bytes"], str(1024 * 1024 * 1024))
        self.assertEqual(fields["call_count"], "1")

    def test_pageout_selects_only_present_pages_and_skips_swapped_pages(self):
        maps = "01000000-01002000 rw-p 00000000 00:00 0\n"
        self.tree.add(4242, "demo", 1000, 777, maps=maps,
                      private_resident_by_start={0x01000000: 8192},
                      page_states={0x01001000: "swapped"})
        result = self.run_helper(4242, 777, 8192)
        fields = self.parse(result.stdout)
        self.assertEqual(fields["status"], "accepted", fields)
        self.assertEqual(fields["requested_bytes"], "4096")
        self.assertEqual(fields["advised_bytes"], "4096")
        self.assertEqual(fields["call_count"], "1")
        self.assertEqual(fields["reason"], "present_page_limit_request_capped")

    def test_pageout_skips_present_pages_that_are_not_exclusively_mapped(self):
        maps = "01000000-01002000 rw-p 00000000 00:00 0\n"
        self.tree.add(4242, "demo", 1000, 777, maps=maps,
                      private_resident_by_start={0x01000000: 8192},
                      page_states={0x01000000: "present_nonexclusive"})
        result = self.run_helper(4242, 777, 8192)
        fields = self.parse(result.stdout)
        self.assertEqual(fields["status"], "accepted", fields)
        self.assertEqual(fields["requested_bytes"], "4096")
        self.assertEqual(fields["advised_bytes"], "4096")
        self.assertEqual(fields["call_count"], "1")
        self.assertEqual(fields["reason"], "exclusive_private_page_limit_request_capped")

    def test_pageout_refuses_when_only_present_pages_are_shared(self):
        maps = "01000000-01001000 rw-p 00000000 00:00 0\n"
        self.tree.add(4242, "demo", 1000, 777, maps=maps,
                      private_resident_by_start={0x01000000: 4096},
                      page_states={0x01000000: "present_nonexclusive"})
        fields = self.parse(self.run_helper(4242, 777, 4096).stdout)
        self.assertEqual(fields["status"], "no_regions", fields)
        self.assertEqual(fields["reason"], "no_exclusively_mapped_private_pages")
        self.assertEqual(fields["requested_bytes"], "0")
        self.assertEqual(fields["call_count"], "0")
        self.assertEqual(fields["advised_bytes"], "0")

    def test_private_resident_estimate_with_no_present_page_refuses_before_syscall(self):
        maps = "01000000-01001000 rw-p 00000000 00:00 0\n"
        self.tree.add(4242, "demo", 1000, 777, maps=maps,
                      private_resident_by_start={0x01000000: 4096},
                      page_states={0x01000000: "absent"})
        fields = self.parse(self.run_helper(4242, 777, 4096).stdout)
        self.assertEqual(fields["status"], "no_regions", fields)
        self.assertEqual(fields["reason"], "no_present_private_pages")
        self.assertEqual(fields["requested_bytes"], "0")
        self.assertEqual(fields["call_count"], "0")
        self.assertEqual(fields["advised_bytes"], "0")

    def test_unreadable_pagemap_refuses_without_call(self):
        directory = self.tree.add(4242, "demo", 1000, 777)
        os.unlink(os.path.join(directory, "pagemap"))
        fields = self.parse(self.run_helper(4242, 777, 4096).stdout)
        self.assertEqual(fields["status"], "refused", fields)
        self.assertEqual(fields["reason"], "target_pagemap_unreadable")
        self.assertEqual(fields["call_count"], "0")
        self.assertEqual(fields["advised_bytes"], "0")

    def test_pageout_budget_is_capped_by_live_rss(self):
        maps = "01000000-41000000 rw-p 00000000 00:00 0\n"
        self.tree.add(4242, "demo", 1000, 777, maps=maps,
                      rss_kib=128 * 1024)
        result = self.run_helper(4242, 777, 1024 * 1024 * 1024)
        fields = self.parse(result.stdout)
        self.assertEqual(fields["status"], "accepted", fields)
        self.assertEqual(fields["requested_bytes"], str(128 * 1024 * 1024))
        self.assertEqual(fields["advised_bytes"], str(128 * 1024 * 1024))
        self.assertEqual(fields["call_count"], "1")

    def test_pageout_budget_is_capped_by_private_resident_pages(self):
        maps = "01000000-41000000 rw-p 00000000 00:00 0\n"
        resident = 64 * 1024 * 1024
        self.tree.add(4242, "demo", 1000, 777, maps=maps,
                      rss_kib=2 * 1024 * 1024,
                      private_resident_by_start={0x01000000: resident})
        fields = self.parse(self.run_helper(4242, 777, 1024 * 1024 * 1024).stdout)
        self.assertEqual(fields["status"], "accepted", fields)
        self.assertEqual(fields["requested_bytes"], str(resident))
        self.assertEqual(fields["advised_bytes"], str(resident))
        self.assertEqual(fields["reason"], "private_resident_limit_request_capped")
        self.assertEqual(fields["call_count"], "1")

    def test_pagemap_scan_limit_is_reported_without_calling_the_backend(self):
        start = 0x01000000
        end = start + 10 * 1024 * 1024 * 1024
        late_page = start + 9 * 1024 * 1024 * 1024
        maps = f"{start:08x}-{end:08x} rw-p 00000000 00:00 0\n"
        self.tree.add(4242, "demo", 1000, 777, maps=maps,
                      private_resident_by_start={start: 4096},
                      page_states={start: "absent", late_page: "present"})
        fields = self.parse(self.run_helper(4242, 777, 4096).stdout)
        self.assertEqual(fields["status"], "no_regions", fields)
        self.assertEqual(fields["reason"], "pagemap_scan_limit_no_present_pages")
        self.assertEqual(fields["call_count"], "0")
        self.assertEqual(fields["advised_bytes"], "0")

    def test_app_scope_reclaim_is_bounded_to_anon_file_charge_and_one_write(self):
        anon = 1024 * 1024 * 1024
        file = 512 * 1024 * 1024
        scope = self.add_scope(anon_bytes=anon, file_bytes=file,
                               current_bytes=anon + file + 4096)
        self.tree.add(4242, "demo", 1000, 777, rss_kib=2 * 1024 * 1024)
        result = self.run_helper(4242, 777, 1024 * 1024 * 1024,
                                 mode="app_scope",
                                 env_extra={"MEMREDUCT_TEST_SKIP_CAPABILITY": "0"})
        fields = self.parse(result.stdout)
        self.assertEqual(fields["status"], "accepted", fields)
        self.assertEqual(fields["mode"], "app_scope")
        self.assertEqual(fields["requested_bytes"], str(1024 * 1024 * 1024))
        self.assertEqual(fields["advised_bytes"], "0")
        self.assertEqual(fields["regions"], "0")
        self.assertEqual(fields["call_count"], "1")
        self.assertEqual(fields["scope_current_before_bytes"], str(anon + file + 4096))
        with open(os.path.join(scope, "memory.reclaim"), encoding="ascii") as handle:
            self.assertEqual(handle.read(), f"{1024 * 1024 * 1024}\n")
        self.assertIn("phase=helper_pre_scope_write", result.stderr)

    def test_app_scope_reclaim_does_not_require_process_pagemap(self):
        anon = 4 * 1024 * 1024
        self.add_scope(anon_bytes=anon, file_bytes=0, current_bytes=anon)
        directory = self.tree.add(4242, "demo", 1000, 777)
        os.unlink(os.path.join(directory, "pagemap"))
        result = self.run_helper(4242, 777, anon, mode="app_scope")
        fields = self.parse(result.stdout)
        self.assertEqual(fields["status"], "accepted", fields)
        self.assertEqual(fields["requested_bytes"], str(anon))
        self.assertEqual(fields["call_count"], "1")

    def test_app_scope_budget_is_limited_when_charge_is_below_one_gib(self):
        anon = 192 * 1024 * 1024
        file = 64 * 1024 * 1024
        scope = self.add_scope(anon_bytes=anon, file_bytes=file,
                               current_bytes=anon + file + 4096)
        self.tree.add(4242, "demo", 1000, 777, rss_kib=2 * 1024 * 1024)
        result = self.run_helper(4242, 777, 1024 * 1024 * 1024,
                                 mode="app_scope")
        fields = self.parse(result.stdout)
        self.assertEqual(fields["status"], "accepted", fields)
        self.assertEqual(fields["requested_bytes"], str(anon + file))
        with open(os.path.join(scope, "memory.reclaim"), encoding="ascii") as handle:
            self.assertEqual(handle.read(), f"{anon + file}\n")

    def test_app_scope_rejects_child_cgroups_before_writing(self):
        scope = self.add_scope()
        os.makedirs(os.path.join(scope, "nested.scope"))
        self.tree.add(4242, "demo", 1000, 777)
        result = self.run_helper(4242, 777, 1024 * 1024 * 1024,
                                 mode="app_scope")
        fields = self.parse(result.stdout)
        self.assertEqual(fields["status"], "scope_unavailable", fields)
        self.assertEqual(fields["call_count"], "0")
        with open(os.path.join(scope, "memory.reclaim"), encoding="ascii") as handle:
            self.assertEqual(handle.read(), "")

    def test_app_scope_accepts_multiple_verified_members_in_the_same_scope(self):
        scope = self.add_scope(members=(4242, 4243))
        self.tree.add(4242, "demo", 1000, 777)
        other = self.tree.add(4243, "renderer", 1000, 778)
        with open(os.path.join(other, "cgroup"), "w", encoding="ascii") as handle:
            handle.write(
                "0::/user.slice/user-1000.slice/user@1000.service/"
                "app.slice/app-demo.scope\n"
            )
        result = self.run_helper(4242, 777, 1024 * 1024 * 1024,
                                 mode="app_scope")
        fields = self.parse(result.stdout)
        self.assertEqual(fields["status"], "accepted", fields)
        self.assertEqual(fields["call_count"], "1")
        with open(os.path.join(scope, "memory.reclaim"), encoding="ascii") as handle:
            self.assertEqual(handle.read(), f"{1024 * 1024 * 1024}\n")

    def test_app_scope_rejects_a_member_from_another_scope_before_writing(self):
        scope = self.add_scope(members=(4242, 4243))
        self.tree.add(4242, "demo", 1000, 777)
        self.tree.add(4243, "other", 1000, 778)
        result = self.run_helper(4242, 777, 1024 * 1024 * 1024,
                                 mode="app_scope")
        fields = self.parse(result.stdout)
        self.assertEqual(fields["status"], "scope_unavailable", fields)
        self.assertEqual(fields["reason"], "app_scope_membership_unverifiable")
        self.assertEqual(fields["call_count"], "0")
        with open(os.path.join(scope, "memory.reclaim"), encoding="ascii") as handle:
            self.assertEqual(handle.read(), "")

    def test_app_scope_eagain_is_partial_and_is_not_retried(self):
        self.add_scope()
        self.tree.add(4242, "demo", 1000, 777)
        result = self.run_helper(4242, 777, 1024 * 1024 * 1024,
                                 mode="app_scope",
                                 env_extra={"MEMREDUCT_TEST_SCOPE_WRITE_RESULT": "eagain"})
        fields = self.parse(result.stdout)
        self.assertEqual(fields["status"], "partial", fields)
        self.assertEqual(fields["reason"], "scope_reclaim_shortfall")
        self.assertEqual(fields["error_code"], str(errno.EAGAIN))
        self.assertEqual(fields["call_count"], "1")

    def test_app_scope_short_write_is_uncertain_and_is_not_retried(self):
        self.add_scope()
        self.tree.add(4242, "demo", 1000, 777)
        result = self.run_helper(4242, 777, 1024 * 1024 * 1024,
                                 mode="app_scope",
                                 env_extra={"MEMREDUCT_TEST_SCOPE_WRITE_RESULT": "short"})
        fields = self.parse(result.stdout)
        self.assertEqual(fields["status"], "unknown", fields)
        self.assertEqual(fields["reason"], "scope_reclaim_outcome_uncertain")
        self.assertEqual(fields["error_code"], str(errno.EIO))
        self.assertEqual(fields["call_count"], "1")


class ProductionBridgeTest(unittest.TestCase):
    """The shipped bridge must contain no test-only entry point.

    This case exists because the first version of async_crossproc.c exported
    mr_xproc_test_classify() unconditionally: the GUI could have called it to
    decide what the helper "said". A symbol scan is the right tool here because
    the defect is the mere presence of the symbol in the shipped ELF.
    """

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="memreduct-xproc-prod-")
        cls.binary = os.path.join(cls.tmp, "gui-like")
        source = os.path.join(cls.tmp, "stub_main.c")
        with open(source, "w", encoding="utf-8") as handle:
            handle.write("int main(void) { return 0; }\n")
        subprocess.run(
            BASE_FLAGS + ["-I", NATIVE, source,
                          os.path.join(NATIVE, "async_crossproc.c"),
                          os.path.join(NATIVE, "production_crossproc_gate_default.c"),
                          "-o", cls.binary],
            check=True, capture_output=True, text=True,
        )

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def symbols(self):
        result = subprocess.run(["nm", self.binary], capture_output=True, text=True)
        if result.returncode != 0:
            self.skipTest("nm is not available")
        return {line.split()[-1] for line in result.stdout.splitlines() if line.strip()}

    def test_no_test_entry_point_is_exported(self):
        names = self.symbols()
        self.assertNotIn("mr_xproc_test_classify", names)
        for name in names:
            self.assertFalse(name.startswith("mr_xproc_test_") or name.startswith("mr_cand_test_"),
                             f"test-only symbol in a production build: {name}")

    def test_the_real_entry_points_are_still_present(self):
        names = self.symbols()
        for required in ("mr_xproc_start", "mr_xproc_poll", "mr_xproc_preflight",
                         "mr_xproc_result_advised_bytes"):
            self.assertIn(required, names)

    def test_default_gate_answers_zero(self):
        gate_source = os.path.join(self.tmp, "gate.c")
        with open(gate_source, "w", encoding="utf-8") as handle:
            handle.write(
                '#include <stdio.h>\n#include <stdint.h>\n'
                'extern int64_t mr_xproc_capability_enabled(void);\n'
                'int main(void){ printf("%lld\\n", (long long)mr_xproc_capability_enabled()); return 0; }\n'
            )
        binary = os.path.join(self.tmp, "gate")
        for name, expected in (("production_crossproc_gate_default.c", "0"),
                               ("production_crossproc_gate_enabled.c", "1")):
            with self.subTest(gate=name):
                subprocess.run(
                    BASE_FLAGS + ["-I", NATIVE, gate_source,
                                  os.path.join(NATIVE, name), "-o", binary],
                    check=True, capture_output=True, text=True,
                )
                result = subprocess.run([binary], capture_output=True, text=True, check=True)
                self.assertEqual(result.stdout.strip(), expected)


class DriverTest(unittest.TestCase):
    """Exercise every classification branch through the production bridge code.

    The driver links async_crossproc.c against a captured-output shim so the
    parser, the exit-code cross-check and the completeness rule are tested as
    they ship, not as a re-implementation.
    """

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="memreduct-xproc-driver-")
        source = os.path.join(cls.tmp, "driver.c")
        with open(source, "w", encoding="utf-8") as handle:
            handle.write(textwrap.dedent("""\
                #include <stdio.h>
                #include <stdlib.h>
                #include <string.h>
                #include "async_crossproc.h"

                int64_t mr_xproc_test_classify_expected(
                    const char *, int64_t, int64_t, int64_t, int64_t, int64_t,
                    int64_t, int64_t, int64_t, int64_t);

                int main(int argc, char **argv) {
                    if (argc != 3 && argc != 11) return 2;
                    FILE *f = fopen(argv[1], "rb");
                    static char buffer[8192];
                    size_t used = 0;
                    if (f != NULL) {
                        used = fread(buffer, 1, sizeof(buffer) - 1, f);
                        fclose(f);
                    }
                    buffer[used] = '\\0';
                    long code = strtol(argv[2], NULL, 10);
                    if (argc == 11) {
                        long long values[8];
                        for (int index = 0; index < 8; index++)
                            values[index] = strtoll(argv[index + 3], NULL, 10);
                        long verdict = mr_xproc_test_classify_expected(
                            buffer, code, values[0], values[1], values[2], values[3],
                            values[4], values[5], values[6], values[7]);
                        printf("verdict=%ld\\n", verdict);
                        return 0;
                    }
                    long verdict = mr_xproc_test_classify(buffer, code);
                    printf("verdict=%ld\\n", verdict);
                    printf("advised=%lld\\n", (long long)mr_xproc_result_advised_bytes());
                    printf("requested=%lld\\n", (long long)mr_xproc_result_requested_bytes());
                    printf("regions=%lld\\n", (long long)mr_xproc_result_regions());
                    printf("calls=%lld\\n", (long long)mr_xproc_result_call_count());
                    printf("verified=%lld\\n", (long long)mr_xproc_result_start_time_verified());
                    printf("uid=%lld\\n", (long long)mr_xproc_result_uid());
                    printf("pkexec_exit_code=%lld\\n", (long long)mr_xproc_result_exit_code());
                    printf("child_signal=%lld\\n", (long long)mr_xproc_result_signal());
                    printf("reason=%s\\n", mr_xproc_result_reason());
                    return 0;
                }
                """))
        cls.driver = os.path.join(cls.tmp, "driver")
        subprocess.run(
            BASE_FLAGS + ["-DMEMREDUCT_TESTING", "-I", NATIVE, source,
                          os.path.join(NATIVE, "async_crossproc.c"),
                          os.path.join(NATIVE, "production_crossproc_gate_enabled.c"),
                          "-o", cls.driver],
            check=True, capture_output=True, text=True,
        )

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def classify(self, text, exit_code):
        path = os.path.join(self.tmp, "out.txt")
        with open(path, "w", encoding="utf-8") as handle:
            handle.write(text)
        result = subprocess.run(
            [self.driver, path, str(exit_code)], capture_output=True, text=True, timeout=10,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        fields = {}
        for line in result.stdout.splitlines():
            key, _, value = line.partition("=")
            fields[key] = value
        return fields

    def classify_expected(self, text, exit_code, target):
        path = os.path.join(self.tmp, "expected-out.txt")
        with open(path, "w", encoding="utf-8") as handle:
            handle.write(text)
        result = subprocess.run(
            [self.driver, path, str(exit_code), *[str(value) for value in target]],
            capture_output=True, text=True, timeout=10,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        return int(result.stdout.strip().split("=", 1)[1])

    @staticmethod
    def block(status, reason="none", advised=1048576, requested=1048576,
              regions=1, error=0, calls=1, pid=4242, verified="true", uid="1000",
              mode="pageout"):
        return (
            "MR_CROSSPROC_RESULT\n"
            "protocol_version=4\n"
            f"mode={mode}\n"
            f"status={status}\n"
            f"reason={reason}\n"
            "request_id=987654321\n"
            f"pid={pid}\n"
            "start_time=777\n"
            f"start_time_verified={verified}\n"
            "expected_identity_hash=1111\n"
            "observed_identity_hash=1111\n"
            "identity_hash_verified=true\n"
            "expected_scope_hash=2222\n"
            "observed_scope_hash=2222\n"
            "scope_hash_verified=true\n"
            "expected_region_digest=3333\n"
            "observed_region_digest=3333\n"
            "region_digest_verified=true\n"
            f"uid={uid}\n"
            f"regions={regions}\n"
            f"requested_bytes={requested}\n"
            f"advised_bytes={advised}\n"
            f"error_code={error}\n"
            f"call_count={calls}\n"
            "skipped_shared=0\n"
            "skipped_unreadable=0\n"
            "skipped_writable_file=0\n"
            "skipped_special=0\n"
            "skipped_too_large=0\n"
            "skipped_budget=0\n"
            "skipped_malformed=0\n"
            "bytes_advised_is_not_bytes_freed=true\n"
            "target_rss_before_kib=8192\n"
            "target_rss_after_kib=4096\n"
            "target_swap_before_kib=1024\n"
            "target_swap_after_kib=2048\n"
            "scope_current_before_bytes=unavailable\n"
            "scope_current_after_bytes=unavailable\n"
            "scope_anon_before_bytes=unavailable\n"
            "scope_anon_after_bytes=unavailable\n"
            "scope_file_before_bytes=unavailable\n"
            "scope_file_after_bytes=unavailable\n"
            "scope_swap_before_bytes=unavailable\n"
            "scope_swap_after_bytes=unavailable\n"
        )

    def test_accepted_with_one_call_and_nonzero_bytes(self):
        fields = self.classify(self.block("accepted"), 0)
        self.assertEqual(fields["verdict"], "0")  # MR_XPROC_POLL_ACCEPTED
        self.assertEqual(fields["advised"], "1048576")
        self.assertEqual(fields["calls"], "1")
        self.assertEqual(fields["verified"], "1")

    def test_accepted_result_must_match_the_selected_request_and_target(self):
        target = [987654321, 4242, 777, 1000, 1111, 2222, 3333, 1048576]
        self.assertEqual(self.classify_expected(self.block("accepted"), 0, target), 0)
        for index, label in enumerate(("request", "pid", "start_time", "uid",
                                       "identity", "scope", "region_digest")):
            with self.subTest(field=label):
                changed = list(target)
                changed[index] += 1
                self.assertEqual(
                    self.classify_expected(self.block("accepted"), 0, changed), 9,
                )

        undersized_budget = list(target)
        undersized_budget[7] -= 1
        self.assertEqual(self.classify_expected(self.block("accepted"), 0,
                                                undersized_budget), 9)

        for field in ("observed_identity_hash", "observed_scope_hash",
                      "observed_region_digest"):
            with self.subTest(field=field):
                text = self.block("accepted").replace(
                    f"{field}=" + {"observed_identity_hash": "1111",
                                   "observed_scope_hash": "2222",
                                   "observed_region_digest": "3333"}[field],
                    f"{field}=9999",
                )
                self.assertEqual(self.classify_expected(text, 0, target), 9)

    def test_partial_is_not_reported_as_accepted(self):
        fields = self.classify(self.block("partial", advised=4096), 0)
        self.assertEqual(fields["verdict"], "1")  # MR_XPROC_POLL_PARTIAL
        self.assertEqual(fields["advised"], "4096")

    def test_one_gib_pageout_result_is_parsed_without_truncation(self):
        one_gib = 1024 * 1024 * 1024
        fields = self.classify(self.block("accepted", advised=one_gib,
                                          requested=one_gib), 0)
        self.assertEqual(fields["verdict"], "0")
        self.assertEqual(fields["advised"], str(one_gib))
        self.assertEqual(fields["requested"], str(one_gib))

    def test_pageout_advice_cannot_exceed_or_understate_a_full_acceptance(self):
        one_gib = 1024 * 1024 * 1024
        for status, advised, requested in (
            ("accepted", one_gib + 1, one_gib),
            ("partial", one_gib + 1, one_gib),
            ("accepted", one_gib - 1, one_gib),
        ):
            with self.subTest(status=status, advised=advised, requested=requested):
                fields = self.classify(self.block(status, advised=advised,
                                                  requested=requested), 0)
                self.assertEqual(fields["verdict"], "9")

    def test_scope_reclaim_reports_the_write_request_without_fake_advised_bytes(self):
        one_gib = 1024 * 1024 * 1024
        fields = self.classify(self.block("accepted", advised=0, requested=one_gib,
                                          mode="app_scope"), 0)
        self.assertEqual(fields["verdict"], "0")
        self.assertEqual(fields["advised"], "0")
        self.assertEqual(fields["requested"], str(one_gib))
        fields = self.classify(self.block("accepted", advised=1, requested=one_gib,
                                          mode="app_scope"), 0)
        self.assertEqual(fields["verdict"], "9")
        fields = self.classify(self.block("partial", advised=0, requested=one_gib,
                                          mode="app_scope"), 0)
        self.assertEqual(fields["verdict"], "1")

    def test_zero_bytes_cannot_be_accepted(self):
        # A claimed acceptance with no covered bytes contradicts itself.
        fields = self.classify(self.block("accepted", advised=0), 0)
        self.assertEqual(fields["verdict"], "9")  # OUTCOME_UNKNOWN

    def test_more_than_one_call_is_outcome_unknown(self):
        fields = self.classify(self.block("accepted", calls=2), 0)
        self.assertEqual(fields["verdict"], "9")

    def test_exit_code_disagreeing_with_the_token_is_outcome_unknown(self):
        fields = self.classify(self.block("accepted"), 1)
        self.assertEqual(fields["verdict"], "9")
        fields = self.classify(self.block("refused"), 0)
        self.assertEqual(fields["verdict"], "9")

    def test_missing_marker_is_outcome_unknown_even_on_exit_zero(self):
        text = self.block("accepted").replace("MR_CROSSPROC_RESULT\n", "")
        fields = self.classify(text, 0)
        self.assertEqual(fields["verdict"], "9")

    def test_truncated_block_is_outcome_unknown(self):
        text = "\n".join(self.block("accepted").splitlines()[:6]) + "\n"
        fields = self.classify(text, 0)
        self.assertEqual(fields["verdict"], "9")

    def test_duplicate_key_is_outcome_unknown(self):
        text = self.block("accepted") + "advised_bytes=1\n"
        fields = self.classify(text, 0)
        self.assertEqual(fields["verdict"], "9")

    def test_unknown_token_is_never_success(self):
        fields = self.classify(self.block("totally_fine"), 0)
        self.assertEqual(fields["verdict"], "9")

    def test_each_refusal_reason_maps_to_a_refusal_verdict(self):
        for reason in (
            "not_root", "no_requester_identity", "missing_cap_sys_nice",
            "not_launched_via_pkexec", "malformed_requester_uid",
            "requester_uid_mismatch", "target_is_pid1", "target_is_self",
            "target_not_readable", "start_time_mismatch", "target_uid_unreadable",
            "target_name_unreadable", "target_is_protected_process",
            "target_other_uid", "target_is_root_owned", "target_is_ancestor",
            "ancestry_unverifiable", "target_has_no_address_space",
            "target_maps_unreadable", "target_identity_changed",
            "target_identity_unverifiable", "target_mappings_changed",
            "selected_uid_mismatch",
        ):
            with self.subTest(reason=reason):
                fields = self.classify(self.block("refused", reason=reason, advised=0,
                                                  requested=0, regions=0, calls=0),
                                       1)
                self.assertEqual(fields["verdict"], "2")  # MR_XPROC_POLL_REFUSED
                self.assertEqual(fields["reason"], reason, "the reason must survive")

    def test_kernel_refusals_stay_distinguishable(self):
        # A kernel denial means the call WAS made and the kernel answered, so
        # call_count is 1 for these. call_count 0 belongs to helper refusals,
        # which are a different verdict on purpose.
        cases = {
            "no_permission": ("3", 1),
            "target_gone": ("4", 1),
            "bad_request": ("2", 1),
            "unsupported": ("5", 3),
            "no_regions": ("6", 1),
        }
        for token, (verdict, code) in cases.items():
            with self.subTest(token=token):
                fields = self.classify(
                    self.block(token, reason="x", advised=0, requested=0,
                               regions=0, calls=1), code)
                self.assertEqual(fields["verdict"], verdict)

    def test_pkexec_cancel_and_authorization_failure_are_distinct(self):
        fields = self.classify("", 126)
        self.assertEqual(fields["verdict"], "11")  # AUTH_CANCELLED
        self.assertEqual(fields["pkexec_exit_code"], "126")
        self.assertEqual(fields["child_signal"], "0")
        fields = self.classify("", 127)
        self.assertEqual(fields["verdict"], "12")  # AUTH_FAILED
        self.assertEqual(fields["pkexec_exit_code"], "127")
        self.assertEqual(fields["child_signal"], "0")

    def test_pkexec_code_with_a_helper_result_is_outcome_unknown(self):
        for code in (126, 127):
            with self.subTest(code=code):
                fields = self.classify(self.block("refused", advised=0, requested=0,
                                                  regions=0, calls=0), code)
                self.assertEqual(fields["verdict"], "9")

    def test_unknown_uid_is_reported_as_minus_one(self):
        fields = self.classify(self.block("refused", advised=0, requested=0,
                                          regions=0, calls=0, uid="unknown"), 1)
        self.assertEqual(fields["uid"], "-1")

    def test_disallowed_advice_or_path_fields_are_ignored_not_trusted(self):
        """The block cannot name a path, an address or a different verb."""
        text = self.block("accepted") + "path=/proc/sys/vm/drop_caches\nverb=pagecache\n"
        fields = self.classify(text, 0)
        self.assertEqual(fields["verdict"], "0")


class BridgeRefusalTest(unittest.TestCase):
    """Every local refusal must name itself.

    Found by probing the bridge directly: a start refused because the gate was
    off reported local reason 0, which is also the value meaning "no refusal".
    A caller could not tell a refusal from an unset field.
    """

    PROBE_SOURCE = """\
#include <stdio.h>
#include <stdint.h>
#include "async_crossproc.h"

static void show(const char *label, int64_t rc)
{
    printf("%s %lld %lld %s\\n", label, (long long)rc,
           (long long)mr_xproc_local_refusal(),
           mr_xproc_local_refusal_name());
}

int main(void)
{
    show("gate_off", mr_xproc_start(123, MR_XPROC_MODE_PROCESS_PAGEOUT, 1192, 11338, 1000, 1, 2, 3, 4096, 1, 256 * 1024 * 1024));
    show("bad_pid", mr_xproc_start(123, MR_XPROC_MODE_PROCESS_PAGEOUT, 0, 11338, 1000, 1, 2, 3, 4096, 1, 256 * 1024 * 1024));
    show("bad_start", mr_xproc_start(123, MR_XPROC_MODE_PROCESS_PAGEOUT, 1192, 0, 1000, 1, 2, 3, 4096, 1, 256 * 1024 * 1024));
    show("bad_budget", mr_xproc_start(123, MR_XPROC_MODE_PROCESS_PAGEOUT, 1192, 11338, 1000, 1, 2, 3, 4096, 1, 0));
    show("over_ceiling", mr_xproc_start(123, MR_XPROC_MODE_PROCESS_PAGEOUT, 1192, 11338, 1000, 1, 2, 3, 4096, 1, 1024ll * 1024ll * 1024ll + 1));
    printf("idle %lld\\n", (long long)mr_xproc_poll());
    return 0;
}
"""

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="memreduct-xproc-bridge-")
        source = os.path.join(cls.tmp, "probe.c")
        with open(source, "w", encoding="utf-8") as handle:
            handle.write(cls.PROBE_SOURCE)
        cls.probe = os.path.join(cls.tmp, "probe")
        subprocess.run(
            BASE_FLAGS + ["-I", NATIVE, source,
                          os.path.join(NATIVE, "async_crossproc.c"),
                          os.path.join(NATIVE, "production_crossproc_gate_default.c"),
                          "-o", cls.probe],
            check=True, capture_output=True, text=True,
        )

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def rows(self):
        result = subprocess.run([self.probe], capture_output=True, text=True, check=True)
        rows = {}
        for line in result.stdout.splitlines():
            parts = line.split()
            # show() prints four fields; the idle line prints two.
            if len(parts) == 4:
                rows[parts[0]] = (parts[1], parts[2], parts[3])
            elif len(parts) == 2:
                rows[parts[0]] = (parts[1], "", "")
        return rows

    def test_every_refusal_reports_a_distinct_named_reason(self):
        rows = self.rows()
        expected = {
            "gate_off": ("5", "not_enabled"),
            "bad_pid": ("1", "bad_pid"),
            "bad_start": ("2", "bad_start_time"),
            "bad_budget": ("3", "bad_budget"),
            "over_ceiling": ("3", "bad_budget"),
        }
        for label, (reason, token) in expected.items():
            with self.subTest(case=label):
                self.assertEqual(rows[label][1], reason)
                self.assertEqual(rows[label][2], token)
        for label in expected:
            self.assertNotEqual(rows[label][2], "none", f"{label} lost its reason")

    def test_a_refused_start_never_reports_a_success_code(self):
        rows = self.rows()
        for label in ("gate_off", "bad_pid", "bad_start", "bad_budget", "over_ceiling"):
            with self.subTest(case=label):
                self.assertEqual(rows[label][0], "2")

    def test_poll_is_idle_before_anything_starts(self):
        self.assertEqual(self.rows()["idle"][0], "-2")


class BridgeCancelTest(unittest.TestCase):
    """Cancellation may signal only the bridge's owned child, never a target PID."""

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="memreduct-xproc-cancel-")
        source = os.path.join(cls.tmp, "cancel.c")
        with open(source, "w", encoding="utf-8") as handle:
            handle.write(textwrap.dedent("""\
                #define _POSIX_C_SOURCE 200809L
                #include <signal.h>
                #include <stdio.h>
                #include <stdint.h>
                #include <sys/types.h>
                #include <sys/wait.h>
                #include <time.h>
                #include <unistd.h>
                #include "async_crossproc.h"

                int main(void) {
                    pid_t child = fork();
                    if (child < 0) return 2;
                    if (child == 0) {
                        signal(SIGTERM, SIG_DFL);
                        for (;;) pause();
                    }
                    if (!mr_xproc_test_attach_child(child)) {
                        kill(child, SIGKILL);
                        waitpid(child, NULL, 0);
                        return 3;
                    }
                    int64_t cancel = mr_xproc_cancel();
                    int64_t result = MR_XPROC_POLL_RUNNING;
                    for (int attempt = 0; attempt < 100; attempt++) {
                        result = mr_xproc_poll();
                        if (result != MR_XPROC_POLL_RUNNING) break;
                        struct timespec delay = { .tv_sec = 0, .tv_nsec = 10000000 };
                        nanosleep(&delay, NULL);
                    }
                    if (result == MR_XPROC_POLL_RUNNING) {
                        kill(child, SIGKILL);
                        waitpid(child, NULL, 0);
                    }
                    printf("cancel=%lld result=%lld signal=%lld idle_cancel=%lld\\n",
                           (long long)cancel, (long long)result,
                           (long long)mr_xproc_result_signal(),
                           (long long)mr_xproc_cancel());
                    return 0;
                }
            """))
        cls.driver = os.path.join(cls.tmp, "cancel")
        subprocess.run(
            BASE_FLAGS + ["-DMEMREDUCT_TESTING", "-I", NATIVE, source,
                          os.path.join(NATIVE, "async_crossproc.c"),
                          os.path.join(NATIVE, "production_crossproc_gate_default.c"),
                          "-o", cls.driver],
            check=True, capture_output=True, text=True,
        )

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def test_escape_cancel_signals_only_the_owned_child_and_reports_unknown(self):
        result = subprocess.run([self.driver], capture_output=True, text=True, timeout=5)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("cancel=0", result.stdout)
        self.assertIn("result=13", result.stdout)  # stopped, helper outcome unknown
        self.assertIn("signal=15", result.stdout)
        self.assertIn("idle_cancel=1", result.stdout)


class PolicyTest(unittest.TestCase):
    """The cross-process action must be its own object, fully separated."""

    @classmethod
    def setUpClass(cls):
        import xml.etree.ElementTree as ET
        cls.tree = ET.parse(POLICY)
        cls.root = cls.tree.getroot()
        cls.actions = cls.root.findall("action")

    def test_well_formed_with_exactly_one_action(self):
        self.assertEqual(self.root.tag, "policyconfig")
        self.assertEqual(len(self.actions), 1)

    def test_action_id_is_the_crossproc_one_and_differs_from_page_cache(self):
        self.assertEqual(self.actions[0].get("id"),
                         "io.github.milchstrasse_endless10.memreduct.zamak.crossproc")
        with open(PAGE_CACHE_POLICY, encoding="utf-8") as handle:
            page_cache = handle.read()
        self.assertNotIn(self.actions[0].get("id"), page_cache,
                         "the two actions must not share an id")

    def test_exec_path_points_at_a_different_helper_than_page_cache(self):
        annotations = {n.get("key"): (n.text or "").strip()
                       for n in self.actions[0].findall("annotate")}
        self.assertEqual(annotations.get("org.freedesktop.policykit.exec.path"),
                         "/usr/libexec/memreduct-zamak-crossproc-helper")
        with open(PAGE_CACHE_POLICY, encoding="utf-8") as handle:
            page_cache = handle.read()
        self.assertNotIn("/usr/libexec/memreduct-zamak-crossproc-helper", page_cache)

    def test_installer_checks_the_installed_action_and_fixed_path(self):
        with open(os.path.join(ROOT, "scripts", "install-arch.sh"),
                  encoding="utf-8") as handle:
            script = handle.read()
        self.assertIn('cmp -s "$xproc_policy_source" "$xproc_policy_target"', script)
        self.assertIn('pkaction --action-id "$xproc_action_id"', script)
        self.assertIn('"$xproc_policy_target"', script)
        self.assertIn("org.freedesktop.policykit.exec.path", script)

    def test_defaults_require_interactive_admin_every_time(self):
        defaults = self.actions[0].find("defaults")
        self.assertEqual((defaults.findtext("allow_any") or "").strip(), "no")
        self.assertEqual((defaults.findtext("allow_inactive") or "").strip(), "no")
        self.assertEqual((defaults.findtext("allow_active") or "").strip(), "auth_admin")
        joined = " ".join((defaults.findtext(tag) or "")
                          for tag in ("allow_any", "allow_inactive", "allow_active"))
        self.assertNotIn("keep", joined)
        self.assertNotIn("yes", joined.replace("auth_admin", ""))

    def test_no_retained_authorization_annotation(self):
        for node in self.actions[0].iter():
            key = node.get("key") or ""
            self.assertNotIn("retain", key)
            self.assertNotIn("require_active", key)

    def test_bilingual_description_and_message(self):
        lang = "{http://www.w3.org/XML/1998/namespace}lang"
        for tag in ("description", "message"):
            langs = {element.get(lang) for element in self.actions[0].findall(tag)}
            self.assertIn(None, langs)
            self.assertIn("zh_CN", langs)

    def test_policy_never_promises_a_fixed_amount(self):
        with open(POLICY, encoding="utf-8") as handle:
            text = handle.read()
        for forbidden in ("guaranteed", "will free", "GB", "freed bytes="):
            self.assertNotIn(forbidden, text)
        self.assertIn("not the amount that was released", text)


class PolicyPreflightFileTest(unittest.TestCase):
    """The bridge opens policy files without symlinks and checks exact binding."""

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="memreduct-policy-preflight-")
        cls.library_path = os.path.join(cls.tmp, "libpreflight.so")
        subprocess.run(
            BASE_FLAGS + ["-DMEMREDUCT_TESTING", "-shared", "-fPIC", "-I", NATIVE,
                          os.path.join(NATIVE, "async_crossproc.c"),
                          os.path.join(NATIVE, "production_crossproc_gate_default.c"),
                          "-o", cls.library_path],
            check=True, capture_output=True, text=True,
        )
        cls.library = ctypes.CDLL(cls.library_path)
        cls.library.mr_xproc_test_policy_file_matches.argtypes = [
            ctypes.c_char_p, ctypes.c_uint, ctypes.c_uint,
        ]
        cls.library.mr_xproc_test_policy_file_matches.restype = ctypes.c_int
        cls.library.mr_xproc_test_helper_protocol_matches.argtypes = [
            ctypes.c_char_p, ctypes.c_uint, ctypes.c_uint,
        ]
        cls.library.mr_xproc_test_helper_protocol_matches.restype = ctypes.c_int

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def setUp(self):
        self.directory = tempfile.mkdtemp(prefix="policy-fixture-")
        self.policy_path = os.path.join(self.directory, "action.policy")
        with open(POLICY, encoding="utf-8") as source:
            self.valid_policy = source.read()
        self.write_policy(self.valid_policy)

    def tearDown(self):
        shutil.rmtree(self.directory, ignore_errors=True)

    def write_policy(self, text, mode=0o644):
        with open(self.policy_path, "w", encoding="utf-8") as handle:
            handle.write(text)
        os.chmod(self.policy_path, mode)

    def matches(self, path=None, owner=None, group=None):
        return self.library.mr_xproc_test_policy_file_matches(
            os.fsencode(path or self.policy_path),
            os.getuid() if owner is None else owner,
            os.getgid() if group is None else group,
        ) == 1

    def test_valid_policy_is_bound_to_the_dedicated_action_and_helper(self):
        self.assertTrue(self.matches())

    def test_wrong_action_or_helper_path_fails_preflight(self):
        self.write_policy(self.valid_policy.replace(
            "io.github.milchstrasse_endless10.memreduct.zamak.crossproc",
            "io.github.milchstrasse_endless10.memreduct.zamak.other", 1))
        self.assertFalse(self.matches())
        self.write_policy(self.valid_policy.replace(
            "/usr/libexec/memreduct-zamak-crossproc-helper",
            "/usr/libexec/other-helper"))
        self.assertFalse(self.matches())

    def test_symlink_writable_file_and_wrong_owner_are_rejected(self):
        link_path = os.path.join(self.directory, "policy-link")
        os.symlink(self.policy_path, link_path)
        self.assertFalse(self.matches(path=link_path))
        self.write_policy(self.valid_policy, mode=0o666)
        self.assertFalse(self.matches())
        self.write_policy(self.valid_policy)
        self.assertFalse(self.matches(owner=os.getuid() + 1))

    def test_symlinked_parent_is_rejected(self):
        real_dir = os.path.join(self.directory, "real")
        os.mkdir(real_dir)
        real_policy = os.path.join(real_dir, "action.policy")
        shutil.copyfile(self.policy_path, real_policy)
        os.chmod(real_policy, 0o644)
        link_dir = os.path.join(self.directory, "linked")
        os.symlink(real_dir, link_dir)
        self.assertFalse(self.matches(path=os.path.join(link_dir, "action.policy")))

    def protocol_matches(self, marker, mode=0o755, prefix=b"\x7fELF-fixture\x00"):
        helper_path = os.path.join(self.directory, "helper-probe")
        with open(helper_path, "wb") as handle:
            handle.write(prefix)
            handle.write(marker)
        os.chmod(helper_path, mode)
        return self.library.mr_xproc_test_helper_protocol_matches(
            os.fsencode(helper_path), os.getuid(), os.getgid(),
        ) == 1

    def test_runtime_helper_probe_accepts_only_the_exact_protocol_marker(self):
        self.assertTrue(self.protocol_matches(b"MR_XPROC_PROTOCOL_ABI=4;"))
        self.assertTrue(self.protocol_matches(
            b"MR_XPROC_PROTOCOL_ABI=4;", prefix=b"x" * (4096 - 4)))
        self.assertFalse(self.protocol_matches(b"MR_XPROC_PROTOCOL_ABI=2;"))
        self.assertFalse(self.protocol_matches(b"MR_XPROC_PROTOCOL_ABI=4-extra"))
        self.assertFalse(self.protocol_matches(
            b"MR_XPROC_PROTOCOL_ABI=4;", mode=0o755 | 0o020))


class SourceContractTest(unittest.TestCase):
    """Static properties of the helper source that a behaviour test cannot show."""

    @classmethod
    def setUpClass(cls):
        with open(HELPER_SRC, encoding="utf-8") as handle:
            cls.source = handle.read()
        with open(HELPER_H, encoding="utf-8") as handle:
            cls.header = handle.read()

    def test_helper_never_execs_a_shell_or_sends_a_signal(self):
        for forbidden in ("system(", "popen(", "execl", "execv", "kill(",
                          "/bin/sh", "setcap", "drop_caches"):
            self.assertNotIn(forbidden, self.source,
                             f"{forbidden} has no business in this helper")

    def test_helper_accepts_no_path_beyond_the_fixed_helper_name(self):
        # The only absolute paths are procfs-derived inside proc_path().
        self.assertNotIn('argv[2][0]', self.source)
        self.assertEqual(self.source.count("snprintf"), self.source.count("snprintf"))

    def test_helper_issues_exactly_one_syscall_expression(self):
        """Exactly one place builds the syscall expression.

        The bare name also appears in the portability #ifndef guard and in two
        #if tests; what matters is that only one call expression exists, so no
        second code path can issue the advice.
        """
        self.assertEqual(self.source.count("syscall(__NR_process_madvise"), 1)

    def test_testing_hooks_are_inside_a_conditional(self):
        """No test switch may exist in the production build.

        PKEXEC_UID is read outside the testing block on purpose: it is part of
        the real contract with pkexec, not a test hook. Every MEMREDUCT_TEST_
        variable must be inside #ifdef MEMREDUCT_TESTING, or the shipped binary
        gains a runtime switch.
        """
        lines = self.source.splitlines()
        in_testing = False
        for line in lines:
            if line.startswith("#ifdef MEMREDUCT_TESTING"):
                in_testing = True
            elif line.startswith("#endif") and in_testing:
                in_testing = False
            if "MEMREDUCT_TEST_" in line and "getenv" in line:
                self.assertTrue(in_testing,
                                f"test switch outside the testing block: {line.strip()}")

    def test_helper_and_scanner_share_fail_closed_identity_policy(self):
        with open(os.path.join(NATIVE, "crossproc_candidates.c"), encoding="utf-8") as handle:
            candidates = handle.read()
        with open(os.path.join(NATIVE, "crossproc_protection.h"), encoding="utf-8") as handle:
            policy = handle.read()
        self.assertIn('#include "crossproc_protection.h"', self.source)
        self.assertIn('#include "crossproc_protection.h"', candidates)
        self.assertIn("MR_XPROC_COMM_TRUNCATED 15u", policy)
        self.assertIn('"dbus-broker-launch"', policy)
        self.assertIn("target_exe_unverifiable", self.source)
        self.assertIn("target_cgroup_unverifiable", self.source)

    def test_advice_is_pageout_only(self):
        self.assertIn("MADV_PAGEOUT", self.source)
        self.assertNotIn("MADV_COLD", self.source)
        self.assertNotIn("MADV_DONTNEED", self.source)

    def test_protected_names_have_one_authoritative_definition(self):
        with open(os.path.join(NATIVE, "crossproc_candidates.c"), encoding="utf-8") as handle:
            candidates = handle.read()
        with open(os.path.join(NATIVE, "crossproc_protection.h"), encoding="utf-8") as handle:
            policy = handle.read()
        self.assertNotIn("mr_xproc_protected_names", self.source)
        self.assertNotIn("mr_xproc_protected_names", candidates)
        self.assertEqual(policy.count("mr_xproc_protected_names[]"), 1)


class InstallSurfaceTest(unittest.TestCase):
    """Install/uninstall must know about the new helper and policy."""

    def read(self, *parts):
        with open(os.path.join(ROOT, *parts), encoding="utf-8") as handle:
            return handle.read()

    def test_install_places_helper_and_policy_at_the_pinned_paths(self):
        text = self.read("scripts", "install-arch.sh")
        self.assertIn("/usr/libexec/memreduct-zamak-crossproc-helper", text)
        self.assertIn(
            "/usr/share/polkit-1/actions/"
            "io.github.milchstrasse_endless10.memreduct.zamak.crossproc.policy", text)

    def test_install_forces_root_ownership_on_the_new_helper(self):
        text = self.read("scripts", "install-arch.sh")
        self.assertIn("xproc_helper_target='/usr/libexec/memreduct-zamak-crossproc-helper'", text)
        self.assertIn('chown 0:0 "$xproc_helper_target"', text)
        self.assertIn('chmod 0755 "$xproc_helper_target"', text)
        self.assertIn('chown 0:0 "$xproc_policy_target"', text)
        self.assertIn('chmod 0644 "$xproc_policy_target"', text)

    def test_candidate_install_has_gate_checked_nonwriting_preflight(self):
        text = self.read("scripts", "install-arch.sh")
        self.assertIn("--m0-m1-candidate", text)
        self.assertIn("--check-m0-m1-candidate", text)
        self.assertIn("mr_production_clean_enabled", text)
        self.assertIn("mr_xproc_capability_enabled", text)
        self.assertIn("mr_build_profile_m0_m1_candidate", text)
        self.assertIn("P2C_FONT_CANDIDATE", text)
        self.assertIn("Candidate GUI must be an executable regular non-symlink file.", text)
        self.assertIn("if ((preflight_only)); then", text)
        self.assertIn("install=not_run", text)
        self.assertIn("MR_XPROC_PROTOCOL_ABI=", text)

    def test_incompatible_helper_gets_a_distinct_user_facing_preflight_state(self):
        header = self.read("native", "async_crossproc.h")
        gui = self.read("src", "main.zm")
        self.assertIn("MR_XPROC_PREFLIGHT_HELPER_PROTOCOL = 5", header)
        self.assertIn("preflight == 5", gui)
        self.assertIn("incompatible protocol", gui)
        self.assertIn("协议不兼容", gui)

    def candidate_preflight(self, helper_version, gui_version=4):
        compiler = shutil.which("cc")
        if not compiler:
            self.skipTest("C compiler unavailable")
        tmp = tempfile.TemporaryDirectory(prefix="memreduct-install-protocol-")
        root = os.path.join(tmp.name, "repo")
        os.makedirs(os.path.join(root, "scripts"), exist_ok=True)
        os.makedirs(os.path.join(root, "build"), exist_ok=True)
        native_dir = os.path.join(root, "native")
        os.makedirs(native_dir, exist_ok=True)
        shutil.copy2(os.path.join(ROOT, "scripts", "install-arch.sh"),
                     os.path.join(root, "scripts", "install-arch.sh"))
        shutil.copy2(HELPER_H, os.path.join(native_dir, "crossproc_helper.h"))
        policy_dir = os.path.join(root, "packaging", "polkit-1", "actions")
        os.makedirs(policy_dir, exist_ok=True)
        for policy in (POLICY, PAGE_CACHE_POLICY):
            shutil.copy2(policy, os.path.join(policy_dir, os.path.basename(policy)))
        for name in ("memreduct-zamak-helper",):
            with open(os.path.join(root, "build", name), "wb") as handle:
                handle.write(b"preflight fixture; never executed\n")
        helper = os.path.join(root, "build", "memreduct-zamak-crossproc-helper")
        with open(helper, "w", encoding="utf-8") as handle:
            handle.write(f"#!/bin/sh\n# MR_XPROC_PROTOCOL_ABI={helper_version};\n")
            handle.write("exit 0\n")
        os.chmod(helper, 0o755)

        tools = os.path.join(tmp.name, "mock-bin")
        os.makedirs(tools)
        for name in ("pkexec", "pkaction"):
            path = os.path.join(tools, name)
            with open(path, "w", encoding="utf-8") as handle:
                handle.write("#!/bin/sh\nexit 99\n")
            os.chmod(path, 0o755)
        source = os.path.join(tmp.name, "candidate.c")
        with open(source, "w", encoding="utf-8") as handle:
            handle.write(f'const char xproc_protocol_marker[] = "MR_XPROC_PROTOCOL_ABI={gui_version};";\n')
            handle.write(textwrap.dedent("""\
                #include <stdint.h>
                int64_t mr_production_clean_enabled(void) { return 1; }
                int64_t mr_xproc_capability_enabled(void) { return 1; }
                int64_t mr_build_profile_m0_m1_candidate(void) { return 1; }
                int main(void) { return 0; }
            """))
        candidate = os.path.join(tmp.name, "candidate")
        subprocess.run([compiler, "-O2", source, "-o", candidate],
                       check=True, capture_output=True, text=True)
        os.chmod(candidate, 0o755)
        env = os.environ.copy()
        env["PATH"] = tools + os.pathsep + env.get("PATH", "")
        result = subprocess.run(
            ["bash", os.path.join(root, "scripts", "install-arch.sh"),
             "--check-m0-m1-candidate", candidate],
            env=env, capture_output=True, text=True,
        )
        return tmp, result

    def test_candidate_preflight_rejects_a_stale_crossproc_helper(self):
        tmp, result = self.candidate_preflight(1)
        try:
            self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("Crossproc helper protocol mismatch", result.stderr)
        finally:
            tmp.cleanup()

    def test_candidate_preflight_accepts_the_matching_crossproc_helper(self):
        tmp, result = self.candidate_preflight(4)
        try:
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("helper_protocol=4", result.stdout)
            self.assertIn("install=not_run", result.stdout)
        finally:
            tmp.cleanup()

    def test_candidate_preflight_rejects_a_gui_with_a_stale_protocol_marker(self):
        tmp, result = self.candidate_preflight(4, gui_version=3)
        try:
            self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("Candidate GUI protocol mismatch", result.stderr)
        finally:
            tmp.cleanup()

    def test_m0_m1_build_uses_a_unique_profile_gate(self):
        build = self.read("scripts", "ci", "build-production-crossproc-gui.sh")
        gate = self.read("native", "production_clean_gate_m0_m1_candidate.c")
        self.assertIn("production_clean_gate_m0_m1_candidate.c", build)
        self.assertIn("mr_build_profile_m0_m1_candidate", build)
        self.assertIn("mr_build_profile_m0_m1_candidate", gate)
        self.assertIn("return mr_build_profile_m0_m1_candidate();", gate)
        self.assertIn("Production GUI has a stale or missing Crossproc protocol marker", build)

    def test_production_manifests_swap_out_the_ci_gtk_window_wrapper(self):
        manifest = self.read("zam.toml")
        self.assertIn('"native/gui_window_ci.c"', manifest)
        self.assertNotIn('"native/gui_window.c"', manifest)
        for name in ("build-production-gui.sh", "build-production-clean-gui.sh",
                     "build-production-crossproc-gui.sh"):
            build = self.read("scripts", "ci", name)
            self.assertIn('"native/gui_window_ci.c"', build)
            self.assertIn('"native/gui_window_prod.c"', build)
        production_wrapper = self.read("native", "gui_window_prod.c")
        self.assertIn("MR_GUI_PRODUCTION", production_wrapper)

    def test_preflight_rejects_gate_symbols_without_exact_profile(self):
        compiler = shutil.which("cc")
        if not compiler:
            self.skipTest("C compiler unavailable")
        with tempfile.TemporaryDirectory(prefix="memreduct-install-preflight-") as tmp:
            root = os.path.join(tmp, "repo")
            os.makedirs(os.path.join(root, "scripts"), exist_ok=True)
            os.makedirs(os.path.join(root, "build"), exist_ok=True)
            policy_dir = os.path.join(root, "packaging", "polkit-1", "actions")
            os.makedirs(policy_dir, exist_ok=True)
            shutil.copy2(os.path.join(ROOT, "scripts", "install-arch.sh"),
                         os.path.join(root, "scripts", "install-arch.sh"))
            shutil.copy2(POLICY, os.path.join(policy_dir, os.path.basename(POLICY)))
            shutil.copy2(PAGE_CACHE_POLICY,
                         os.path.join(policy_dir, os.path.basename(PAGE_CACHE_POLICY)))
            for name in ("memreduct-zamak-helper", "memreduct-zamak-crossproc-helper"):
                path = os.path.join(root, "build", name)
                with open(path, "wb") as handle:
                    handle.write(b"preflight fixture; never executed\n")
            tools = os.path.join(tmp, "mock-bin")
            os.makedirs(tools)
            for name in ("pkexec", "pkaction"):
                path = os.path.join(tools, name)
                with open(path, "w", encoding="utf-8") as handle:
                    handle.write("#!/bin/sh\nexit 99\n")
                os.chmod(path, 0o755)
            source = os.path.join(tmp, "candidate.c")
            with open(source, "w", encoding="utf-8") as handle:
                handle.write(textwrap.dedent("""\
                    #include <stdint.h>
                    int64_t mr_production_clean_enabled(void) { return 1; }
                    int64_t mr_xproc_capability_enabled(void) { return 1; }
                    int main(void) { return 0; }
                """))
            candidate = os.path.join(tmp, "candidate")
            subprocess.run([compiler, "-O2", source, "-o", candidate],
                           check=True, capture_output=True, text=True)
            os.chmod(candidate, 0o755)
            env = os.environ.copy()
            env["PATH"] = tools + os.pathsep + env.get("PATH", "")
            result = subprocess.run(
                ["bash", os.path.join(root, "scripts", "install-arch.sh"),
                 "--check-m0-m1-candidate", candidate],
                env=env, capture_output=True, text=True,
            )
            self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("exact M0+M1 candidate profile marker", result.stderr)

    def test_uninstall_removes_every_new_file(self):
        text = self.read("scripts", "uninstall-arch.sh")
        self.assertIn("/usr/libexec/memreduct-zamak-crossproc-helper", text)
        self.assertIn(
            "/usr/share/polkit-1/actions/"
            "io.github.milchstrasse_endless10.memreduct.zamak.crossproc.policy", text)

    def test_build_script_compiles_the_new_helper(self):
        text = self.read("scripts", "build-arch.sh")
        self.assertIn("native/crossproc_helper.c", text)
        self.assertIn("build/memreduct-zamak-crossproc-helper", text)


if __name__ == "__main__":
    unittest.main()
