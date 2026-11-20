"""Presentation contract for the balanced clean entry point.

Pure functions only: no window, no kernel, no scope. These tests exist because
the wording is a product requirement, not decoration: a budget must never be
called "freed", a skip must never be silent, and an empty run must never be
presented as a successful clean.
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

ENTRY = {"hidden": 0, "disabled": 1, "ready": 2}
LINE = {"action": 0, "why": 1, "budget": 2, "skips": 3}
RESULT = {"headline": 0, "counters": 1, "stop": 2, "observed": 3}


class Availability(ctypes.Structure):
    _fields_ = [
        ("built_in", ctypes.c_int32),
        ("gate_enabled", ctypes.c_int32),
        ("helper_ready", ctypes.c_int32),
        ("busy", ctypes.c_int32),
        ("no_target", ctypes.c_int32),
    ]


class GuiTextTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="memreduct-gui-")
        cls.lib_path = os.path.join(cls.tmp, "libgui.so")
        subprocess.run(
            ["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-pedantic",
             "-shared", "-fPIC", "-I", NATIVE,
             os.path.join(NATIVE, "balance_gui.c"), "-o", cls.lib_path],
            check=True, capture_output=True, text=True,
        )
        cls.lib = ctypes.CDLL(cls.lib_path)
        cls.lib.mr_bal_gui_entry.argtypes = [ctypes.POINTER(Availability)]
        cls.lib.mr_bal_gui_entry.restype = ctypes.c_int64
        cls.lib.mr_bal_gui_plan_line.argtypes = [
            ctypes.c_int64, ctypes.c_int32, ctypes.c_int64, ctypes.c_int64,
            ctypes.c_uint64, ctypes.c_int32,
        ]
        cls.lib.mr_bal_gui_plan_line.restype = ctypes.c_char_p
        cls.lib.mr_bal_gui_result_line.argtypes = [
            ctypes.c_int64, ctypes.c_int32, ctypes.c_int64, ctypes.c_int64,
            ctypes.c_uint64, ctypes.c_uint64, ctypes.c_uint32, ctypes.c_uint32,
            ctypes.c_int64,
        ]
        cls.lib.mr_bal_gui_result_line.restype = ctypes.c_char_p
        cls.lib.mr_bal_gui_result_is_meaningful.argtypes = [ctypes.c_int64, ctypes.c_uint64]
        cls.lib.mr_bal_gui_result_is_meaningful.restype = ctypes.c_int32

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def entry(self, **kwargs):
        a = Availability()
        a.built_in = 1
        a.gate_enabled = 1
        a.helper_ready = 1
        for key, value in kwargs.items():
            setattr(a, key, value)
        return self.lib.mr_bal_gui_entry(ctypes.byref(a))

    def plan(self, line, zh=0, state=3, action=1, budget=256 * MIB, evidence=1):
        return self.lib.mr_bal_gui_plan_line(line, zh, state, action, budget,
                                             evidence).decode()

    def result(self, line, zh=0, status=0, stop=0, requested=256 * MIB,
               accepted=128 * MIB, ok=2, rejected=0, delta=100 * MIB):
        return self.lib.mr_bal_gui_result_line(line, zh, status, stop, requested,
                                               accepted, ok, rejected, delta).decode()

    # -- entry ------------------------------------------------------------
    def test_entry_hidden_when_not_built_in(self):
        self.assertEqual(self.entry(built_in=0), ENTRY["hidden"])

    def test_entry_hidden_when_gate_closed(self):
        self.assertEqual(self.entry(gate_enabled=0), ENTRY["hidden"])

    def test_entry_disabled_while_busy(self):
        self.assertEqual(self.entry(busy=1), ENTRY["disabled"])

    def test_entry_disabled_without_helper(self):
        self.assertEqual(self.entry(helper_ready=0), ENTRY["disabled"])

    def test_entry_disabled_without_a_target(self):
        """No eligible scope must not be papered over with a fake success."""
        self.assertEqual(self.entry(no_target=1), ENTRY["disabled"])

    def test_entry_ready_when_everything_holds(self):
        self.assertEqual(self.entry(), ENTRY["ready"])

    # -- plan wording -----------------------------------------------------
    def test_budget_line_never_claims_an_amount_was_freed(self):
        """The word "freed" may appear only inside an explicit negation.

        A budget is a request the kernel may refuse; presenting it as memory
        that was released is the fake-success failure this product forbids.
        """
        for zh, must in ((0, "256.0 MiB"), (1, "256.0 MiB")):
            text = self.plan(LINE["budget"], zh=zh)
            self.assertIn(must, text)
            lowered = text.lower()
            if "freed" in lowered:
                self.assertIn("not an amount freed", lowered,
                              "the only permitted use is the negation: %r" % text)
            if "释放" in text:
                self.assertIn("不是释放量", text)

    def test_budget_line_is_zero_when_nothing_will_run(self):
        for zh in (0, 1):
            text = self.plan(LINE["budget"], zh=zh, action=0)
            self.assertIn("0", text)
            self.assertNotIn("MiB", text)

    def test_why_line_names_the_dominant_side(self):
        en = self.plan(LINE["why"], state=3)
        self.assertIn("file pages", en)
        en_anon = self.plan(LINE["why"], state=4)
        self.assertIn("anonymous", en_anon)
        zh = self.plan(LINE["why"], zh=1, state=3)
        self.assertIn("文件页", zh)

    def test_why_line_refuses_to_guess_without_evidence(self):
        for zh in (0, 1):
            text = self.plan(LINE["why"], zh=zh, evidence=0)
            self.assertTrue("No pressure evidence" in text or "缺少压力证据" in text)

    def test_thrashing_is_described_as_making_it_worse(self):
        self.assertIn("worse", self.plan(LINE["why"], state=5))
        self.assertIn("更糟", self.plan(LINE["why"], zh=1, state=5))

    def test_skips_line_promises_no_silent_omission(self):
        for zh in (0, 1):
            text = self.plan(LINE["skips"], zh=zh)
            self.assertTrue("silently" in text or "静默" in text)

    def test_action_line_differs_per_action(self):
        file_first = self.plan(LINE["action"], action=1)
        anon = self.plan(LINE["action"], action=2)
        none = self.plan(LINE["action"], action=0)
        self.assertNotEqual(file_first, anon)
        self.assertNotEqual(file_first, none)
        self.assertIn("file pages", file_first)

    # -- result wording ---------------------------------------------------
    def test_counters_line_reports_both_numbers_separately(self):
        """A single rotating buffer used to make both sizes print the same."""
        text = self.result(RESULT["counters"], requested=256 * MIB, accepted=128 * MIB)
        self.assertIn("256.0 MiB", text)
        self.assertIn("128.0 MiB", text)

    def test_counters_line_counts_batches(self):
        text = self.result(RESULT["counters"], ok=3, rejected=1)
        self.assertIn("3", text)
        self.assertIn("1", text)

    def test_headline_distinguishes_partial_from_complete(self):
        ok = self.result(RESULT["headline"], status=0)
        partial = self.result(RESULT["headline"], status=1)
        stopped = self.result(RESULT["headline"], status=2)
        unsupported = self.result(RESULT["headline"], status=3)
        for text in (ok, partial, stopped, unsupported):
            self.assertTrue(text)
        self.assertNotEqual(ok, partial)
        self.assertNotEqual(ok, stopped)

    def test_stop_line_names_the_reason(self):
        self.assertIn("no eligible file pages",
                      self.result(RESULT["stop"], stop=1))
        self.assertIn("不" if False else "冷文件页", self.result(RESULT["stop"], zh=1, stop=1))

    def test_observed_line_labels_the_delta_as_an_observation(self):
        text = self.result(RESULT["observed"], delta=100 * MIB)
        self.assertIn("observation", text)
        self.assertIn("not an amount freed", text.lower(),
                      "the delta must be explicitly disclaimed, not merely unlabelled")

    def test_observed_line_handles_no_change(self):
        text = self.result(RESULT["observed"], delta=0)
        self.assertIn("did not change", text)

    def test_observed_line_handles_a_decrease(self):
        text = self.result(RESULT["observed"], delta=-64 * MIB)
        self.assertIn("-", text)
        self.assertIn("64.0 MiB", text)

    # -- meaningfulness ---------------------------------------------------
    def test_zero_accepted_is_not_a_meaningful_result(self):
        self.assertEqual(self.lib.mr_bal_gui_result_is_meaningful(2, 0), 0)

    def test_unsupported_is_never_meaningful(self):
        self.assertEqual(self.lib.mr_bal_gui_result_is_meaningful(3, 128 * MIB), 0)

    def test_accepted_bytes_make_it_meaningful(self):
        self.assertEqual(self.lib.mr_bal_gui_result_is_meaningful(0, 128 * MIB), 1)


if __name__ == "__main__":
    unittest.main()
