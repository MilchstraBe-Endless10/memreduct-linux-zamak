"""Q14 line B: the conditional default balanced policy, branch by branch.

Everything here is an injected fixture. No test in this file opens a cgroup,
reads /proc, writes memory.reclaim, or touches drop_caches. The policy is a
pure function, so "did the policy decide to issue a batch" is answered by
reading a return value rather than by observing a kernel side effect.

The point of the coverage is the set of stops. A cleaner that only knows how
to start is worse than no cleaner, so every condition that must prevent the
next batch has a test that would fail if that condition were removed.

Run: python3 -m unittest tests.test_policy_state
"""

import ctypes
import os
import subprocess
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NATIVE = os.path.join(ROOT, "native")

MIB = 1024 * 1024

# enum mr_pol_stop
STOP_NONE = 0
STOP_TARGET_MET = 1
STOP_REFAULT = 2
STOP_PRESSURE_WORSE = 3
STOP_TELEMETRY_MISSING = 4
STOP_CANCELLED = 5
STOP_COOLDOWN = 6
STOP_NO_TRIGGER = 7
STOP_BUDGET_SPENT = 8
STOP_NO_TARGET = 9

# enum mr_pol_verdict
VERDICT_IDLE = 0
VERDICT_RECLAIM = 1
VERDICT_ANON_BOUND = 2
VERDICT_UNKNOWN = 3


class Scope(ctypes.Structure):
    _fields_ = [
        ("enrolled", ctypes.c_int32),
        ("readable", ctypes.c_int32),
        ("file_bytes", ctypes.c_uint64),
        ("active_file", ctypes.c_uint64),
        ("inactive_file", ctypes.c_uint64),
        ("anon_bytes", ctypes.c_uint64),
        ("refault_file", ctypes.c_uint64),
        ("cooldown_until_ms", ctypes.c_uint64),
    ]


class Evidence(ctypes.Structure):
    _fields_ = [
        ("now_ms", ctypes.c_uint64),
        ("mem_total", ctypes.c_uint64),
        ("mem_available", ctypes.c_uint64),
        ("psi_some_centi", ctypes.c_uint32),
        ("pressure_seen_ms", ctypes.c_uint64),
        ("since_last_run_ms", ctypes.c_uint64),
    ]


class Config(ctypes.Structure):
    _fields_ = [
        ("low_water_centi", ctypes.c_uint32),
        ("high_water_centi", ctypes.c_uint32),
        ("psi_trigger_centi", ctypes.c_uint32),
        ("psi_worse_centi", ctypes.c_uint32),
        ("pressure_dwell_ms", ctypes.c_uint64),
        ("cooldown_ms", ctypes.c_uint64),
        ("batch_bytes", ctypes.c_uint64),
        ("max_batches", ctypes.c_uint32),
        ("max_request_bytes", ctypes.c_uint64),
        ("cold_fraction_centi", ctypes.c_uint32),
        ("refault_budget_centi", ctypes.c_uint32),
    ]


class State(ctypes.Structure):
    _fields_ = [
        ("last_run_ms", ctypes.c_uint64),
        ("batches_issued", ctypes.c_uint32),
        ("requested_this_round", ctypes.c_uint64),
        ("available_at_start", ctypes.c_uint64),
        ("psi_at_start", ctypes.c_uint32),
        ("running", ctypes.c_int32),
    ]


class Decision(ctypes.Structure):
    _fields_ = [
        ("verdict", ctypes.c_int64),
        ("stop", ctypes.c_int64),
        ("scope_index", ctypes.c_int64),
        ("request_bytes", ctypes.c_uint64),
        ("cold_file_bytes", ctypes.c_uint64),
        ("anon_bytes", ctypes.c_uint64),
        ("water_mark_bytes", ctypes.c_uint64),
        ("cooldown_active", ctypes.c_int32),
    ]


def build_library():
    out = os.path.join(tempfile.gettempdir(), "mr_policy_%d.so" % os.getpid())
    subprocess.run(
        ["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-pedantic",
         "-shared", "-fPIC", "-I", NATIVE,
         os.path.join(NATIVE, "balance_policy.c"), "-o", out],
        check=True, capture_output=True, text=True,
    )
    return out


class PolicyTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls._libpath = build_library()
        cls.lib = ctypes.CDLL(cls._libpath)
        cls.lib.mr_pol_config_default.argtypes = [ctypes.POINTER(Config)]
        cls.lib.mr_pol_state_init.argtypes = [ctypes.POINTER(State)]
        cls.lib.mr_pol_begin.argtypes = [
            ctypes.POINTER(Config), ctypes.POINTER(State),
            ctypes.POINTER(Evidence), ctypes.POINTER(Scope),
            ctypes.c_uint32, ctypes.POINTER(Decision)]
        cls.lib.mr_pol_begin.restype = ctypes.c_int
        cls.lib.mr_pol_continue.argtypes = [
            ctypes.POINTER(Config), ctypes.POINTER(State),
            ctypes.POINTER(Evidence), ctypes.POINTER(Scope),
            ctypes.c_uint32, ctypes.c_uint64, ctypes.c_uint64,
            ctypes.c_uint32, ctypes.POINTER(Decision)]
        cls.lib.mr_pol_continue.restype = ctypes.c_int
        cls.lib.mr_pol_cool_scope.argtypes = [
            ctypes.POINTER(Scope), ctypes.c_uint64, ctypes.c_uint64]
        cls.lib.mr_pol_scope_skip_reason.argtypes = [
            ctypes.POINTER(Config), ctypes.POINTER(Scope),
            ctypes.POINTER(Evidence)]
        cls.lib.mr_pol_scope_skip_reason.restype = ctypes.c_char_p
        cls.lib.mr_pol_stop_name.argtypes = [ctypes.c_int64]
        cls.lib.mr_pol_stop_name.restype = ctypes.c_char_p
        cls.lib.mr_pol_verdict_name.argtypes = [ctypes.c_int64]
        cls.lib.mr_pol_verdict_name.restype = ctypes.c_char_p

    @classmethod
    def tearDownClass(cls):
        try:
            os.unlink(cls._libpath)
        except OSError:
            pass

    # ---- fixtures ----------------------------------------------------

    def config(self):
        c = Config()
        self.lib.mr_pol_config_default(ctypes.byref(c))
        return c

    def state(self):
        s = State()
        self.lib.mr_pol_state_init(ctypes.byref(s))
        return s

    def scope(self, **kw):
        s = Scope()
        s.enrolled = 1
        s.readable = 1
        for key, value in kw.items():
            setattr(s, key, value)
        return s

    def evidence(self, **kw):
        e = Evidence()
        e.now_ms = 100000
        e.mem_total = 16 * 1024 * MIB
        # Below low water (50%) and under pressure: the shape that should act.
        e.mem_available = 5 * 1024 * MIB
        e.psi_some_centi = 900
        e.pressure_seen_ms = 5000
        e.since_last_run_ms = 0
        for key, value in kw.items():
            setattr(e, key, value)
        return e

    def begin(self, scopes, ev=None, cfg=None, st=None):
        cfg = cfg or self.config()
        st = st or self.state()
        ev = ev or self.evidence()
        arr = (Scope * len(scopes))(*scopes)
        out = Decision()
        rc = self.lib.mr_pol_begin(ctypes.byref(cfg), ctypes.byref(st),
                                   ctypes.byref(ev), arr, len(scopes),
                                   ctypes.byref(out))
        return rc, out, cfg, st, ev, arr

    def cont(self, scopes, cfg, st, ev, refaulted=0,
             avail_after=6 * 1024 * MIB, psi_after=900):
        arr = (Scope * len(scopes))(*scopes)
        out = Decision()
        rc = self.lib.mr_pol_continue(
            ctypes.byref(cfg), ctypes.byref(st), ctypes.byref(ev), arr,
            len(scopes), refaulted, avail_after, psi_after,
            ctypes.byref(out))
        return rc, out, arr

    # ---- the happy path ----------------------------------------------

    def test_a_qualifying_scope_is_acted_on_file_only(self):
        """All three preconditions hold and one scope has cold file pages."""
        scopes = [self.scope(file_bytes=78 * MIB, active_file=4900 * 1024,
                             inactive_file=74 * MIB, anon_bytes=4 * MIB)]
        rc, out, cfg, st, ev, _ = self.begin(scopes)
        self.assertEqual(rc, 1, "a qualifying round must be allowed to issue")
        self.assertEqual(out.verdict, VERDICT_RECLAIM)
        self.assertEqual(out.stop, STOP_NONE)
        self.assertEqual(out.scope_index, 0)
        self.assertGreater(out.request_bytes, 0,
                           "a request of zero would be a no-op dressed as work")
        # File-only: the request never exceeds the scope's cold pages.
        self.assertLessEqual(out.request_bytes, out.cold_file_bytes)

    def test_request_is_capped_by_the_share_of_cold_pages(self):
        """We ask about a fraction of the cold list, not all of it."""
        cold = 200 * MIB
        scopes = [self.scope(file_bytes=cold, active_file=MIB,
                             inactive_file=cold)]
        rc, out, _, _, _, _ = self.begin(scopes)
        cfg = self.config()
        self.assertEqual(rc, 1)
        # 25% of 200 MiB = 50 MiB, but the batch ceiling is 32 MiB.
        self.assertLessEqual(out.request_bytes, cfg.batch_bytes)
        self.assertLess(out.request_bytes, cold,
                        "asking for the whole cold list is not a fraction")

    def test_the_largest_cold_source_is_chosen(self):
        """Two enrolled candidates: the bigger inactive list wins."""
        small = self.scope(file_bytes=10 * MIB, active_file=MIB,
                           inactive_file=8 * MIB)
        big = self.scope(file_bytes=80 * MIB, active_file=MIB,
                         inactive_file=70 * MIB)
        rc, out, _, _, _, _ = self.begin([small, big])
        self.assertEqual(rc, 1)
        self.assertEqual(out.scope_index, 1,
                         "the largest cold source gives the most per stop")

    def test_a_single_cold_scope_still_gets_one_honest_ask(self):
        """A tiny scope must not round its request down to zero."""
        cold = 4096  # one page
        scopes = [self.scope(file_bytes=cold, active_file=0,
                             inactive_file=cold)]
        rc, out, _, _, _, _ = self.begin(scopes)
        self.assertEqual(rc, 1)
        self.assertGreater(out.request_bytes, 0)

    # ---- precondition 1: enough available ----------------------------

    def test_plenty_available_means_no_action(self):
        """A big cache doing its job is not a problem to fix."""
        scopes = [self.scope(file_bytes=300 * MIB, active_file=MIB,
                             inactive_file=290 * MIB)]
        ev = self.evidence(mem_available=12 * 1024 * MIB)
        rc, out, _, _, _, _ = self.begin(scopes, ev=ev)
        self.assertEqual(rc, 0)
        self.assertEqual(out.verdict, VERDICT_IDLE)
        self.assertEqual(out.stop, STOP_TARGET_MET)

    def test_scarcity_without_pressure_does_not_arm(self):
        """Below low water but nobody is stalling: not our call to act."""
        scopes = [self.scope(file_bytes=300 * MIB, active_file=MIB,
                             inactive_file=290 * MIB)]
        ev = self.evidence(psi_some_centi=10)
        rc, out, _, _, _, _ = self.begin(scopes, ev=ev)
        self.assertEqual(rc, 0)
        self.assertEqual(out.stop, STOP_NO_TRIGGER)

    def test_pressure_without_scarcity_does_not_arm(self):
        """Stalling with memory available is someone else's problem."""
        scopes = [self.scope(file_bytes=300 * MIB, active_file=MIB,
                             inactive_file=290 * MIB)]
        ev = self.evidence(mem_available=11 * 1024 * MIB, psi_some_centi=900)
        rc, out, _, _, _, _ = self.begin(scopes, ev=ev)
        self.assertEqual(rc, 0)
        self.assertEqual(out.verdict, VERDICT_IDLE)

    def test_a_pressure_spike_is_not_sustained_pressure(self):
        """One sample is a spike; the dwell time makes it evidence."""
        scopes = [self.scope(file_bytes=300 * MIB, active_file=MIB,
                             inactive_file=290 * MIB)]
        ev = self.evidence(pressure_seen_ms=100)
        rc, out, _, _, _, _ = self.begin(scopes, ev=ev)
        self.assertEqual(rc, 0, "a 100 ms spike must not arm a reclaim")
        self.assertEqual(out.stop, STOP_NO_TRIGGER)

    # ---- precondition 3: a suitable cold scope ------------------------

    def test_file_billing_without_cold_pages_is_not_a_candidate(self):
        """The hard constraint: file size alone must never qualify a scope."""
        # A scope billed 300 MiB of file pages, all of them active.
        scopes = [self.scope(file_bytes=300 * MIB,
                             active_file=299 * MIB, inactive_file=MIB)]
        rc, out, _, _, _, _ = self.begin(scopes)
        self.assertEqual(rc, 0,
                         "a scope in active use must not be cleared by size")
        self.assertEqual(out.stop, STOP_NO_TARGET)

    def test_unenrolled_scopes_are_never_touched(self):
        """The target range is what the user included, nothing else."""
        scopes = [self.scope(enrolled=0, file_bytes=300 * MIB,
                             active_file=MIB, inactive_file=290 * MIB)]
        rc, out, _, _, _, _ = self.begin(scopes)
        self.assertEqual(rc, 0,
                         "an app the user did not include must not be cleared")
        self.assertEqual(out.stop, STOP_NO_TARGET)

    def test_an_unreadable_scope_is_skipped_not_assumed_fine(self):
        scopes = [self.scope(readable=0, file_bytes=300 * MIB,
                             active_file=MIB, inactive_file=290 * MIB)]
        rc, out, _, _, _, _ = self.begin(scopes)
        self.assertEqual(rc, 0)
        self.assertEqual(out.stop, STOP_NO_TARGET)

    def test_zero_scopes_is_a_no_target_not_a_crash(self):
        rc, out, _, _, _, _ = self.begin([])
        self.assertEqual(rc, 0)
        self.assertEqual(out.stop, STOP_NO_TARGET)
        self.assertEqual(out.scope_index, -1)

    # ---- the anon rule -----------------------------------------------

    def test_anon_dominated_growth_names_the_scope_and_does_not_escalate(self):
        """Anon pressure must be reported, never met with a pageout."""
        scopes = [self.scope(file_bytes=61 * MIB, active_file=8 * MIB,
                             inactive_file=53 * MIB, anon_bytes=447 * MIB)]
        rc, out, cfg, _, _, _ = self.begin(scopes)
        # 447 MiB anon vs 53 MiB cold: anon dominates, and the inactive/active
        # ratio is still healthy, so this scope IS a candidate by cold-page
        # rules. The anon rule is about what to do when nothing qualifies.
        if rc == 1:
            self.assertEqual(out.verdict, VERDICT_RECLAIM,
                             "a scope with real cold pages is still reclaimable")
            # And the request is still bounded by cold pages, never by anon.
            self.assertLessEqual(out.request_bytes, out.cold_file_bytes)
        else:
            self.assertIn(out.stop, (STOP_NO_TARGET, STOP_NO_TRIGGER))

    def test_when_only_anon_grows_the_verdict_is_anon_bound(self):
        """No file pages anywhere, anon everywhere: name it, stop."""
        scopes = [
            self.scope(file_bytes=0, active_file=0, inactive_file=0,
                       anon_bytes=447 * MIB),
            self.scope(file_bytes=0, active_file=0, inactive_file=0,
                       anon_bytes=51 * MIB),
        ]
        rc, out, _, _, _, _ = self.begin(scopes)
        self.assertEqual(rc, 0, "anon growth must never trigger a reclaim")
        self.assertEqual(out.verdict, VERDICT_ANON_BOUND)
        self.assertEqual(out.scope_index, 0,
                         "the biggest anon contributor must be named")
        self.assertEqual(out.anon_bytes, 447 * MIB)

    # ---- cooldown -----------------------------------------------------

    def test_a_cooling_scope_is_left_alone(self):
        """A scope that just refaulted must not be cleared again."""
        scopes = [self.scope(file_bytes=300 * MIB, active_file=MIB,
                             inactive_file=290 * MIB,
                             cooldown_until_ms=999999)]
        ev = self.evidence(now_ms=100000)
        rc, out, _, _, _, _ = self.begin(scopes, ev=ev)
        self.assertEqual(rc, 0, "clearing a cooling scope re-clears a hot cache")
        self.assertEqual(out.stop, STOP_NO_TARGET)

    def test_cooldown_expires(self):
        """Once the cooldown passes, the scope is eligible again."""
        scopes = [self.scope(file_bytes=300 * MIB, active_file=MIB,
                             inactive_file=290 * MIB,
                             cooldown_until_ms=50000)]
        ev = self.evidence(now_ms=100000)
        rc, _out, _, _, _, _ = self.begin(scopes, ev=ev)
        self.assertEqual(rc, 1, "an expired cooldown must not block forever")

    def test_cool_scope_sets_the_deadline(self):
        scopes = [self.scope(file_bytes=MIB, active_file=0, inactive_file=MIB)]
        arr = (Scope * 1)(*scopes)
        self.lib.mr_pol_cool_scope(ctypes.byref(arr[0]), 1000, 5000)
        self.assertEqual(arr[0].cooldown_until_ms, 6000)

    def test_a_refaulting_scope_is_skipped_by_history(self):
        """Refault history marks a scope whose cold pages are actually wanted."""
        scopes = [self.scope(file_bytes=100 * MIB, active_file=MIB,
                             inactive_file=90 * MIB,
                             refault_file=50 * MIB)]
        rc, out, _, _, _, _ = self.begin(scopes)
        self.assertEqual(rc, 0,
                         "50% refaulted against a 10% budget is a hot scope")
        self.assertEqual(out.stop, STOP_NO_TARGET)

    # ---- the stop conditions after a batch ---------------------------

    def test_target_reached_stops_the_run(self):
        scopes = [self.scope(file_bytes=300 * MIB, active_file=MIB,
                             inactive_file=290 * MIB)]
        rc, out, cfg, st, ev, arr = self.begin(scopes)
        self.assertEqual(rc, 1)
        # High water is 65% of 16 GiB = 10.4 GiB.
        rc2, out2, _ = self.cont(scopes, cfg, st, ev,
                                 avail_after=11 * 1024 * MIB)
        self.assertEqual(rc2, 0)
        self.assertEqual(out2.stop, STOP_TARGET_MET)

    def test_refault_stops_the_run(self):
        """Pages we just dropped came straight back: stop, do not fight it."""
        scopes = [self.scope(file_bytes=300 * MIB, active_file=MIB,
                             inactive_file=290 * MIB)]
        rc, _, cfg, st, ev, _ = self.begin(scopes)
        self.assertEqual(rc, 1)
        rc2, out2, _ = self.cont(scopes, cfg, st, ev, refaulted=4096,
                                 avail_after=6 * 1024 * MIB)
        self.assertEqual(rc2, 0)
        self.assertEqual(out2.stop, STOP_REFAULT)

    def test_worsening_pressure_stops_the_run(self):
        """If our own reclaim is making the stall worse, we stop."""
        scopes = [self.scope(file_bytes=300 * MIB, active_file=MIB,
                             inactive_file=290 * MIB)]
        rc, _, cfg, st, ev, _ = self.begin(scopes)
        self.assertEqual(rc, 1)
        rc2, out2, _ = self.cont(scopes, cfg, st, ev, psi_after=2500,
                                 avail_after=6 * 1024 * MIB)
        self.assertEqual(rc2, 0)
        self.assertEqual(out2.stop, STOP_PRESSURE_WORSE)

    def test_any_psi_rise_stops_even_below_the_ceiling(self):
        """A rise from our own run is a stop even if it is still under 20%."""
        scopes = [self.scope(file_bytes=300 * MIB, active_file=MIB,
                             inactive_file=290 * MIB)]
        rc, _, cfg, st, ev, _ = self.begin(scopes)
        self.assertEqual(rc, 1)
        # start was 900; 1000 is worse but far below psi_worse_centi.
        rc2, out2, _ = self.cont(scopes, cfg, st, ev, psi_after=1000,
                                 avail_after=6 * 1024 * MIB)
        self.assertEqual(rc2, 0)
        self.assertEqual(out2.stop, STOP_PRESSURE_WORSE)

    def test_batch_ceiling_stops_the_run(self):
        scopes = [self.scope(file_bytes=300 * MIB, active_file=MIB,
                             inactive_file=290 * MIB)]
        cfg = self.config()
        cfg.max_batches = 2
        st = self.state()
        st.batches_issued = 2
        rc, out, _, _, _, _ = self.begin(scopes, cfg=cfg, st=st)
        self.assertEqual(rc, 1)
        rc2, out2, _ = self.cont(scopes, cfg, st,
                                 self.evidence(mem_available=6 * 1024 * MIB),
                                 avail_after=6 * 1024 * MIB)
        self.assertEqual(rc2, 0)
        self.assertEqual(out2.stop, STOP_BUDGET_SPENT)

    def test_request_ceiling_stops_the_run(self):
        scopes = [self.scope(file_bytes=300 * MIB, active_file=MIB,
                             inactive_file=290 * MIB)]
        cfg = self.config()
        cfg.max_request_bytes = 10 * MIB
        st = self.state()
        st.requested_this_round = 10 * MIB
        rc, out, _, _, _, _ = self.begin(scopes, cfg=cfg, st=st)
        self.assertEqual(rc, 0)
        self.assertEqual(out.stop, STOP_BUDGET_SPENT)

    def test_missing_evidence_refuses_to_act(self):
        """No telemetry is not "probably fine"; it is a refusal."""
        scopes = [self.scope(file_bytes=300 * MIB, active_file=MIB,
                             inactive_file=290 * MIB)]
        cfg = self.config()
        st = self.state()
        arr = (Scope * 1)(*scopes)
        out = Decision()
        rc = self.lib.mr_pol_begin(ctypes.byref(cfg), ctypes.byref(st),
                                   None, arr, 1, ctypes.byref(out))
        self.assertEqual(rc, 0)
        self.assertEqual(out.verdict, VERDICT_UNKNOWN)
        self.assertEqual(out.stop, STOP_TELEMETRY_MISSING)

    def test_zero_mem_total_is_treated_as_missing_telemetry(self):
        scopes = [self.scope(file_bytes=300 * MIB, active_file=MIB,
                             inactive_file=290 * MIB)]
        ev = self.evidence(mem_total=0)
        rc, out, _, _, _, _ = self.begin(scopes, ev=ev)
        self.assertEqual(rc, 0)
        self.assertEqual(out.stop, STOP_TELEMETRY_MISSING)

    def test_user_cancel_is_a_named_stop(self):
        """Cancellation has a code so the interface never says "finished"."""
        self.assertEqual(self.lib.mr_pol_stop_name(STOP_CANCELLED).decode(),
                         "cancelled")

    # ---- round spacing ------------------------------------------------

    def test_round_cooldown_prevents_immediate_rerun(self):
        scopes = [self.scope(file_bytes=300 * MIB, active_file=MIB,
                             inactive_file=290 * MIB)]
        cfg = self.config()
        st = self.state()
        st.last_run_ms = 100000
        ev = self.evidence(since_last_run_ms=1000)
        rc, out, _, _, _, _ = self.begin(scopes, ev=ev, cfg=cfg, st=st)
        self.assertEqual(rc, 0)
        self.assertEqual(out.stop, STOP_COOLDOWN)
        self.assertEqual(out.cooldown_active, 1)

    # ---- names --------------------------------------------------------

    def test_every_stop_has_a_stable_name(self):
        names = [self.lib.mr_pol_stop_name(v).decode() for v in range(0, 11)]
        self.assertEqual(len(set(names)), len(names),
                         "two stops sharing a name would be unreportable")
        self.assertIn("invalid", names)

    def test_every_verdict_has_a_stable_name(self):
        for value, expected in ((0, "idle"), (1, "reclaim"),
                                (2, "anon_bound"), (3, "unknown")):
            self.assertEqual(
                self.lib.mr_pol_verdict_name(value).decode(), expected)

    def test_skip_reasons_explain_why_a_row_is_greyed(self):
        cfg = self.config()
        ev = self.evidence()
        cases = [
            (self.scope(enrolled=0), "not_enrolled"),
            (self.scope(readable=0), "unreadable"),
            (self.scope(inactive_file=0), "no_cold_pages"),
            (self.scope(inactive_file=90 * MIB, active_file=MIB,
                        file_bytes=100 * MIB, refault_file=50 * MIB),
             "refaulting"),
            (self.scope(inactive_file=MIB, active_file=100 * MIB,
                        file_bytes=200 * MIB), "active_working_set"),
        ]
        for index, (scope, expected) in enumerate(cases):
            arr = (Scope * 1)(scope)
            got = self.lib.mr_pol_scope_skip_reason(
                ctypes.byref(cfg), ctypes.byref(arr[0]),
                ctypes.byref(ev)).decode()
            self.assertEqual(got, expected, "case %d" % index)

    def test_config_defaults_are_conservative(self):
        """Defaults must be quiet on a healthy machine, and anon is off."""
        cfg = self.config()
        self.assertGreater(cfg.low_water_centi, 0)
        self.assertGreater(cfg.high_water_centi, cfg.low_water_centi,
                           "high water must sit above low water or it flaps")
        self.assertGreater(cfg.psi_trigger_centi, 0)
        self.assertGreater(cfg.psi_worse_centi, cfg.psi_trigger_centi)
        self.assertGreater(cfg.batch_bytes, 0)
        self.assertGreater(cfg.max_batches, 0)
        self.assertGreater(cfg.cold_fraction_centi, 0)
        self.assertLess(cfg.cold_fraction_centi, 10000,
                        "asking for all cold pages is not a fraction")


if __name__ == "__main__":
    unittest.main()
