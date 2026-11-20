"""Balanced reclaim planner: classification, water marks, hysteresis, cooldown.

Every test drives the real C code through a fixture /proc and /sys tree, so the
parsers and the policy are both exercised. No test touches the host kernel.
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

STATE = {
    "unknown": 0, "healthy": 1, "healthy_cache": 2,
    "cold_file": 3, "anon_pressure": 4, "thrashing": 5,
}
ACTION = {"none": 0, "file_first": 1, "anon_pageout": 2}


class SwapDevice(ctypes.Structure):
    _fields_ = [
        ("name", ctypes.c_char * 256),
        ("size", ctypes.c_uint64),
        ("used", ctypes.c_uint64),
        ("is_zram", ctypes.c_int32),
        ("zram_orig_data_size", ctypes.c_uint64),
        ("zram_mem_used", ctypes.c_uint64),
    ]


class Snapshot(ctypes.Structure):
    _fields_ = [
        ("total", ctypes.c_uint64),
        ("free_ram", ctypes.c_uint64),
        ("available", ctypes.c_uint64),
        ("cached", ctypes.c_uint64),
        ("sreclaimable", ctypes.c_uint64),
        ("active_anon", ctypes.c_uint64),
        ("inactive_anon", ctypes.c_uint64),
        ("active_file", ctypes.c_uint64),
        ("inactive_file", ctypes.c_uint64),
        ("dirty", ctypes.c_uint64),
        ("writeback", ctypes.c_uint64),
        ("swap_total", ctypes.c_uint64),
        ("swap_free", ctypes.c_uint64),
        ("psi_some_total_us", ctypes.c_uint64),
        ("psi_full_total_us", ctypes.c_uint64),
        ("psi_some_avg10_centi", ctypes.c_uint32),
        ("psi_full_avg10_centi", ctypes.c_uint32),
        ("pswpin", ctypes.c_uint64),
        ("pswpout", ctypes.c_uint64),
        ("pgmajfault", ctypes.c_uint64),
        ("refault_anon", ctypes.c_uint64),
        ("refault_file", ctypes.c_uint64),
        ("pgscan_direct", ctypes.c_uint64),
        ("pgsteal_direct", ctypes.c_uint64),
        ("swap_devices", ctypes.c_uint32),
        ("swap", SwapDevice * 8),
        ("missing", ctypes.c_uint32),
    ]


class Delta(ctypes.Structure):
    _fields_ = [
        ("elapsed_ms", ctypes.c_uint64),
        ("psi_some_us", ctypes.c_uint64),
        ("psi_full_us", ctypes.c_uint64),
        ("pswpin", ctypes.c_uint64),
        ("pswpout", ctypes.c_uint64),
        ("refault_anon", ctypes.c_uint64),
        ("refault_file", ctypes.c_uint64),
        ("available", ctypes.c_int64),
    ]


class Config(ctypes.Structure):
    _fields_ = [
        ("low_water_centi", ctypes.c_uint32),
        ("high_water_centi", ctypes.c_uint32),
        ("psi_stop_centi", ctypes.c_uint32),
        ("psi_trigger_centi", ctypes.c_uint32),
        ("cooldown_ms", ctypes.c_uint64),
        ("recover_ms", ctypes.c_uint64),
        ("max_request_bytes", ctypes.c_uint64),
        ("file_fraction_centi", ctypes.c_uint32),
        ("anon_enabled", ctypes.c_int32),
    ]


class Planner(ctypes.Structure):
    _fields_ = [
        ("last_run_ms", ctypes.c_uint64),
        ("above_high_since_ms", ctypes.c_uint64),
        ("armed", ctypes.c_int32),
        ("last_action", ctypes.c_int32),
        ("last_state", ctypes.c_uint32),
        ("reserved", ctypes.c_uint32),
    ]


class Plan(ctypes.Structure):
    _fields_ = [
        ("state", ctypes.c_int64),
        ("action", ctypes.c_int64),
        ("reason", ctypes.c_int64),
        ("request_bytes", ctypes.c_uint64),
        ("available", ctypes.c_uint64),
        ("cold_file_bytes", ctypes.c_uint64),
        ("anon_inactive_bytes", ctypes.c_uint64),
        ("psi_some_centi", ctypes.c_uint32),
        ("psi_full_centi", ctypes.c_uint32),
        ("swap_total", ctypes.c_uint64),
        ("swap_used", ctypes.c_uint64),
        ("swap_free", ctypes.c_uint64),
        ("zram_device_count", ctypes.c_int32),
        ("zram_full", ctypes.c_int32),
        ("cooldown_active", ctypes.c_int32),
    ]


def _statfs_bytes():
    return 0


class PlannerTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="memreduct-balance-")
        cls.lib = os.path.join(cls.tmp, "libbalance.so")
        subprocess.run(
            ["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-pedantic",
             "-shared", "-fPIC", "-I", NATIVE,
             os.path.join(NATIVE, "balance_snapshot.c"),
             os.path.join(NATIVE, "balance_plan.c"),
             "-o", cls.lib],
            check=True, capture_output=True, text=True,
        )
        cls.library = ctypes.CDLL(cls.lib)
        lib = cls.library
        lib.mr_bal_snapshot_read.argtypes = [ctypes.c_char_p, ctypes.c_char_p,
                                             ctypes.POINTER(Snapshot)]
        lib.mr_bal_snapshot_read.restype = ctypes.c_int
        lib.mr_bal_delta_between.argtypes = [ctypes.POINTER(Snapshot),
                                            ctypes.POINTER(Snapshot),
                                            ctypes.c_uint64,
                                            ctypes.POINTER(Delta)]
        lib.mr_bal_delta_between.restype = ctypes.c_int
        lib.mr_bal_psi_some_rate_centi.argtypes = [ctypes.POINTER(Delta)]
        lib.mr_bal_psi_some_rate_centi.restype = ctypes.c_uint64
        lib.mr_bal_config_default.argtypes = [ctypes.POINTER(Config)]
        lib.mr_bal_config_default.restype = None
        lib.mr_bal_planner_init.argtypes = [ctypes.POINTER(Planner)]
        lib.mr_bal_planner_init.restype = None
        lib.mr_bal_plan_build.argtypes = [ctypes.POINTER(Config), ctypes.POINTER(Planner),
                                          ctypes.POINTER(Snapshot), ctypes.POINTER(Delta),
                                          ctypes.c_uint64, ctypes.POINTER(Plan)]
        lib.mr_bal_plan_build.restype = ctypes.c_int
        lib.mr_bal_planner_note_run.argtypes = [ctypes.POINTER(Planner), ctypes.c_uint64,
                                                ctypes.c_int64]
        lib.mr_bal_planner_note_run.restype = None
        for name in ("mr_bal_state_name", "mr_bal_action_name", "mr_bal_reason_name"):
            getattr(lib, name).argtypes = [ctypes.c_int64]
            getattr(lib, name).restype = ctypes.c_char_p

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    # -- fixture tree -----------------------------------------------------
    def setUp(self):
        self.proc = tempfile.mkdtemp(prefix="bal-proc-", dir=self.tmp)
        self.sys = tempfile.mkdtemp(prefix="bal-sys-", dir=self.tmp)
        os.makedirs(os.path.join(self.proc, "pressure"), exist_ok=True)
        os.makedirs(os.path.join(self.sys, "block"), exist_ok=True)
        self.write_meminfo(total_kib=16 * 1024 * 1024, avail_kib=8 * 1024 * 1024,
                           inactive_file_kib=2 * 1024 * 1024)
        self.write_psi(some_centi=0, full_centi=0)
        self.write_vmstat()
        self.write_swaps([])

    def tearDown(self):
        shutil.rmtree(self.proc, ignore_errors=True)
        shutil.rmtree(self.sys, ignore_errors=True)

    def write_meminfo(self, total_kib, avail_kib, inactive_file_kib=0,
                      inactive_anon_kib=0, sreclaimable_kib=0, dirty_kib=0,
                      swap_total_kib=0, swap_free_kib=0):
        lines = [
            f"MemTotal:       {total_kib} kB",
            f"MemFree:        {avail_kib // 2} kB",
            f"MemAvailable:   {avail_kib} kB",
            f"Cached:         {inactive_file_kib} kB",
            f"SReclaimable:   {sreclaimable_kib} kB",
            "Active(anon):   0 kB",
            f"Inactive(anon): {inactive_anon_kib} kB",
            "Active(file):   0 kB",
            f"Inactive(file): {inactive_file_kib} kB",
            f"Dirty:          {dirty_kib} kB",
            "Writeback:      0 kB",
            f"SwapTotal:      {swap_total_kib} kB",
            f"SwapFree:       {swap_free_kib} kB",
        ]
        with open(os.path.join(self.proc, "meminfo"), "w", encoding="ascii") as fh:
            fh.write("\n".join(lines) + "\n")

    def write_psi(self, some_centi, full_centi, some_total_us=0, full_total_us=0):
        def fmt(c):
            return f"{c // 100}.{c % 100:02d}"
        with open(os.path.join(self.proc, "pressure", "memory"), "w", encoding="ascii") as fh:
            fh.write(f"some avg10={fmt(some_centi)} avg60=0.00 avg300=0.00 total={some_total_us}\n")
            fh.write(f"full avg10={fmt(full_centi)} avg60=0.00 avg300=0.00 total={full_total_us}\n")

    def write_vmstat(self, pswpin=0, pswpout=0, refault_anon=0, refault_file=0):
        with open(os.path.join(self.proc, "vmstat"), "w", encoding="ascii") as fh:
            fh.write(f"pswpin {pswpin}\npswpout {pswpout}\npgmajfault 0\n")
            fh.write(f"workingset_refault_anon {refault_anon}\n")
            fh.write(f"workingset_refault_file {refault_file}\n")
            fh.write("pgscan_direct 0\npgsteal_direct 0\n")

    def write_swaps(self, devices):
        lines = ["Filename                                Type            Size            Used    Priority"]
        for name, size_kib, used_kib in devices:
            lines.append(f"{name:<40}{'partition':<16}{size_kib:<15}{used_kib:<8}-2")
        with open(os.path.join(self.proc, "swaps"), "w", encoding="ascii") as fh:
            fh.write("\n".join(lines) + "\n")

    def read(self):
        snap = Snapshot()
        rc = self.library.mr_bal_snapshot_read(self.proc.encode(), self.sys.encode(),
                                               ctypes.byref(snap))
        self.assertEqual(rc, 0)
        return snap

    def default_config(self):
        cfg = Config()
        self.library.mr_bal_config_default(ctypes.byref(cfg))
        return cfg

    def build(self, snap, cfg=None, planner=None, delta=None, now_ms=1_000_000):
        cfg = cfg or self.default_config()
        planner = planner if planner is not None else Planner()
        if planner.last_state == 0 and planner.armed == 0 and planner.last_run_ms == 0:
            self.library.mr_bal_planner_init(ctypes.byref(planner))
        plan = Plan()
        rc = self.library.mr_bal_plan_build(ctypes.byref(cfg), ctypes.byref(planner),
                                            ctypes.byref(snap),
                                            ctypes.byref(delta) if delta else None,
                                            now_ms, ctypes.byref(plan))
        self.assertEqual(rc, 0)
        return plan, planner

    def test_ctypes_layout_matches_the_c_struct(self):
        """A wrong ctypes layout reads garbage and can segfault the test run.

        Pin the three offsets that a mismatch would move, so the fixture never
        silently disagrees with the C definition it drives.
        """
        self.assertEqual(ctypes.sizeof(SwapDevice), 296)
        self.assertEqual(ctypes.sizeof(Snapshot), 2568)
        self.assertEqual(Snapshot.swap_devices.offset, 184)
        self.assertEqual(Snapshot.swap.offset, 192)
        self.assertEqual(Snapshot.missing.offset, 2560)

    # -- parsing ----------------------------------------------------------
    def test_meminfo_values_are_bytes(self):
        snap = self.read()
        self.assertEqual(snap.total, 16 * 1024 * MIB)
        self.assertEqual(snap.available, 8 * 1024 * MIB)
        self.assertEqual(snap.inactive_file, 2 * 1024 * MIB)
        self.assertEqual(snap.missing, 0)

    def test_missing_meminfo_is_reported_not_zeroed(self):
        os.unlink(os.path.join(self.proc, "meminfo"))
        snap = self.read()
        self.assertTrue(snap.missing & 1)

    def test_psi_average_is_parsed_as_hundredths(self):
        self.write_psi(some_centi=1234, full_centi=56)
        snap = self.read()
        self.assertEqual(snap.psi_some_avg10_centi, 1234)
        self.assertEqual(snap.psi_full_avg10_centi, 56)

    def test_swaps_parsed_with_zram_detection(self):
        self.write_swaps([("/dev/zram0", 4 * 1024 * 1024, 1024 * 1024),
                          ("/dev/nvme0n1p3", 8 * 1024 * 1024, 0)])
        snap = self.read()
        self.assertEqual(snap.swap_devices, 2)
        self.assertEqual(snap.swap[0].is_zram, 1)
        self.assertEqual(snap.swap[0].size, 4 * 1024 * MIB)
        self.assertEqual(snap.swap[1].is_zram, 0)

    def test_delta_rates_are_integer_and_sane(self):
        before = Snapshot()
        before.psi_some_total_us = 1000
        after = Snapshot()
        after.psi_some_total_us = 1000 + 5_000_000  # 5 s stalled
        delta = Delta()
        self.assertEqual(self.library.mr_bal_delta_between(
            ctypes.byref(before), ctypes.byref(after), 10_000, ctypes.byref(delta)), 0)
        self.assertEqual(delta.psi_some_us, 5_000_000)
        # 5 s of stall over 10 s of wall time = 50.00%
        self.assertEqual(self.library.mr_bal_psi_some_rate_centi(ctypes.byref(delta)), 5000)

    # -- policy -----------------------------------------------------------
    def test_healthy_machine_is_left_alone(self):
        snap = self.read()
        plan, _ = self.build(snap)
        self.assertEqual(plan.state, STATE["healthy"])
        self.assertEqual(plan.action, ACTION["none"])

    def test_low_available_but_all_reclaimable_cache_is_not_reclaimed(self):
        # 5% available of 16 GiB, but the deficit is covered by reclaimable file.
        # Cold (inactive) file pages are absent; the deficit is covered by
        # reclaimable slab instead, which the planner may not drop selectively.
        self.write_meminfo(total_kib=16 * 1024 * 1024, avail_kib=800 * 1024,
                           inactive_file_kib=0,
                           sreclaimable_kib=9 * 1024 * 1024)
        snap = self.read()
        plan, _ = self.build(snap)
        self.assertEqual(plan.state, STATE["healthy_cache"])
        self.assertEqual(plan.action, ACTION["none"])
        self.assertEqual(
            self.library.mr_bal_reason_name(plan.reason).decode(), "no_cold_pages")

    def test_cold_file_pressure_produces_file_first_budget(self):
        self.write_psi(some_centi=800, full_centi=200)
        self.write_meminfo(total_kib=16 * 1024 * 1024, avail_kib=4 * 1024 * 1024,
                           inactive_file_kib=4 * 1024 * 1024)
        snap = self.read()
        plan, _ = self.build(snap)
        self.assertEqual(plan.state, STATE["cold_file"])
        self.assertEqual(plan.action, ACTION["file_first"])
        # 25% of 4 GiB cold file = 1 GiB, clamped to the 512 MiB ceiling.
        self.assertEqual(plan.request_bytes, 512 * MIB)

    def test_stall_above_trigger_with_cold_file_does_act(self):
        # Available is at low water rather than below it, but the machine is
        # stalling: that is exactly the case the trigger threshold exists for.
        self.write_psi(some_centi=600, full_centi=10)
        snap = self.read()
        plan, _ = self.build(snap)
        self.assertEqual(plan.state, STATE["cold_file"])
        self.assertEqual(plan.action, ACTION["file_first"])

    def test_stall_below_trigger_does_not_act(self):
        # Same machine, under the trigger: a little stall is not a mandate.
        self.write_psi(some_centi=100, full_centi=10)
        snap = self.read()
        plan, _ = self.build(snap)
        self.assertEqual(plan.action, ACTION["none"])

    def test_thrashing_never_acts(self):
        self.write_psi(some_centi=2500, full_centi=1500)
        self.write_meminfo(total_kib=16 * 1024 * 1024, avail_kib=1024 * 1024,
                           inactive_file_kib=8 * 1024 * 1024)
        snap = self.read()
        plan, _ = self.build(snap)
        self.assertEqual(plan.state, STATE["thrashing"])
        self.assertEqual(plan.action, ACTION["none"])
        self.assertEqual(self.library.mr_bal_reason_name(plan.reason).decode(), "stall_rising")

    def test_dirty_backlog_blocks_reclaim(self):
        self.write_psi(some_centi=800, full_centi=100)
        self.write_meminfo(total_kib=16 * 1024 * 1024, avail_kib=4 * 1024 * 1024,
                           inactive_file_kib=4 * 1024 * 1024, dirty_kib=1024 * 1024)
        snap = self.read()
        plan, _ = self.build(snap)
        self.assertEqual(self.library.mr_bal_reason_name(plan.reason).decode(), "dirty_backlog")
        self.assertEqual(plan.action, ACTION["none"])

    def test_anon_pressure_is_disabled_by_default(self):
        self.write_psi(some_centi=800, full_centi=300)
        self.write_meminfo(total_kib=16 * 1024 * 1024, avail_kib=1024 * 1024,
                           inactive_file_kib=0, inactive_anon_kib=6 * 1024 * 1024,
                           swap_total_kib=8 * 1024 * 1024, swap_free_kib=4 * 1024 * 1024)
        snap = self.read()
        plan, _ = self.build(snap)
        self.assertEqual(plan.state, STATE["anon_pressure"])
        self.assertEqual(plan.action, ACTION["none"])
        self.assertEqual(self.library.mr_bal_reason_name(plan.reason).decode(), "anon_disabled")

    def test_anon_pageout_needs_free_swap_even_when_enabled(self):
        cfg = self.default_config()
        cfg.anon_enabled = 1
        self.write_psi(some_centi=800, full_centi=300)
        self.write_meminfo(total_kib=16 * 1024 * 1024, avail_kib=1024 * 1024,
                           inactive_anon_kib=6 * 1024 * 1024,
                           swap_total_kib=8 * 1024 * 1024, swap_free_kib=0)
        snap = self.read()
        plan, _ = self.build(snap, cfg=cfg)
        self.assertEqual(plan.action, ACTION["none"])
        self.assertEqual(self.library.mr_bal_reason_name(plan.reason).decode(), "swap_full")

    def test_cooldown_blocks_a_second_run(self):
        self.write_psi(some_centi=800, full_centi=100)
        self.write_meminfo(total_kib=16 * 1024 * 1024, avail_kib=4 * 1024 * 1024,
                           inactive_file_kib=4 * 1024 * 1024)
        snap = self.read()
        plan, planner = self.build(snap, now_ms=1_000_000)
        self.assertEqual(plan.action, ACTION["file_first"])
        self.library.mr_bal_planner_note_run(ctypes.byref(planner), 1_000_000, plan.action)
        # Same evidence 5 s later: still within the 30 s cooldown.
        plan2, planner2 = self.build(snap, planner=planner, now_ms=1_005_000)
        self.assertEqual(plan2.action, ACTION["none"])
        self.assertEqual(plan2.cooldown_active, 1)

    def test_after_a_run_the_planner_needs_recovery_to_rearm(self):
        self.write_psi(some_centi=800, full_centi=100)
        self.write_meminfo(total_kib=16 * 1024 * 1024, avail_kib=4 * 1024 * 1024,
                           inactive_file_kib=4 * 1024 * 1024)
        snap = self.read()
        plan, planner = self.build(snap, now_ms=1_000_000)
        self.library.mr_bal_planner_note_run(ctypes.byref(planner), 1_000_000, plan.action)

        # Well past cooldown, but memory never recovered above high water.
        plan2, planner2 = self.build(snap, planner=planner, now_ms=2_000_000)
        self.assertEqual(plan2.action, ACTION["none"])
        self.assertEqual(self.library.mr_bal_reason_name(plan2.reason).decode(),
                         "above_high_water")

        # Recovered above 65% and stayed there for the 60 s recover window.
        self.write_meminfo(total_kib=16 * 1024 * 1024, avail_kib=12 * 1024 * 1024,
                           inactive_file_kib=2 * 1024 * 1024)
        healthy = self.read()
        self.build(healthy, planner=planner, now_ms=2_100_000)
        self.build(healthy, planner=planner, now_ms=2_200_000)
        # Pressure returns after recovery: the planner is armed again.
        self.write_psi(some_centi=800, full_centi=100)
        self.write_meminfo(total_kib=16 * 1024 * 1024, avail_kib=4 * 1024 * 1024,
                           inactive_file_kib=4 * 1024 * 1024)
        snap3 = self.read()
        plan3, _ = self.build(snap3, planner=planner, now_ms=2_300_000)
        self.assertEqual(plan3.action, ACTION["file_first"])

    def test_zram_full_is_reported_separately_from_disk_swap(self):
        self.write_swaps([("/dev/zram0", 1024 * 1024, 1024 * 1024),
                          ("/dev/nvme0n1p3", 8 * 1024 * 1024, 0)])
        self.write_meminfo(total_kib=16 * 1024 * 1024, avail_kib=8 * 1024 * 1024,
                           inactive_file_kib=2 * 1024 * 1024,
                           swap_total_kib=9 * 1024 * 1024, swap_free_kib=8 * 1024 * 1024)
        snap = self.read()
        plan, _ = self.build(snap)
        self.assertEqual(plan.zram_device_count, 1)
        self.assertEqual(plan.zram_full, 1)
        # Disk swap still empty: that is not "swap exhausted".
        self.assertEqual(plan.swap_free, 8 * 1024 * MIB)

    def test_state_and_action_never_contradict_each_other(self):
        """The host this was written on is anonymous-heavy with a nearly full
        zram. That combination used to report anon_pressure while proposing a
        file-first run; the two must agree."""
        self.write_psi(some_centi=10, full_centi=10)
        self.write_meminfo(total_kib=16 * 1024 * 1024, avail_kib=1600 * 1024,
                           inactive_file_kib=1550 * 1024,
                           inactive_anon_kib=2000 * 1024,
                           swap_total_kib=32 * 1024 * 1024,
                           swap_free_kib=15 * 1024 * 1024)
        snap = self.read()
        plan, _ = self.build(snap)
        self.assertEqual(plan.state, STATE["anon_pressure"])
        self.assertEqual(plan.action, ACTION["none"],
                         "anonymous pressure must not be answered with file-first")

    def test_cold_file_dominant_still_produces_file_first(self):
        self.write_psi(some_centi=10, full_centi=10)
        self.write_meminfo(total_kib=16 * 1024 * 1024, avail_kib=1600 * 1024,
                           inactive_file_kib=3000 * 1024,
                           inactive_anon_kib=500 * 1024,
                           swap_total_kib=8 * 1024 * 1024,
                           swap_free_kib=4 * 1024 * 1024)
        snap = self.read()
        plan, _ = self.build(snap)
        self.assertEqual(plan.state, STATE["cold_file"])
        self.assertEqual(plan.action, ACTION["file_first"])

    def test_missing_evidence_refuses_to_act(self):
        os.unlink(os.path.join(self.proc, "meminfo"))
        snap = self.read()
        plan, _ = self.build(snap)
        self.assertEqual(plan.state, STATE["unknown"])
        self.assertEqual(plan.action, ACTION["none"])
        self.assertEqual(self.library.mr_bal_reason_name(plan.reason).decode(),
                         "evidence_missing")

    def test_names_are_stable(self):
        for name, value in STATE.items():
            self.assertEqual(self.library.mr_bal_state_name(value).decode(), name)
        for name, value in ACTION.items():
            self.assertEqual(self.library.mr_bal_action_name(value).decode(), name)
        self.assertEqual(self.library.mr_bal_state_name(99).decode(), "invalid")


if __name__ == "__main__":
    unittest.main()
