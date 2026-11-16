"""The action record must say *when* the write happened.

Without a time a record says "a reclaim occurred" but cannot be lined up with
any measurement taken outside the process, so the one question the record
exists to answer - did the scope's counters move because of this write - cannot
be asked. The fields are monotonic-ms (for comparison) plus wall-clock seconds
(for a human reading the file later); both 0 means not observed, never
"at time zero".
"""

import os
import re
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BRIDGE_C = os.path.join(ROOT, "native", "balance_bridge.c")
BRIDGE_H = os.path.join(ROOT, "native", "balance_bridge.h")
RECLAIM_C = os.path.join(ROOT, "native", "balance_reclaim.c")
RECLAIM_H = os.path.join(ROOT, "native", "balance_reclaim.h")


def read(path):
    with open(path, encoding="utf-8") as handle:
        return handle.read()


class RecordTimestampTest(unittest.TestCase):
    def setUp(self):
        self.bridge = read(BRIDGE_C)
        self.header = read(BRIDGE_H)
        self.reclaim = read(RECLAIM_C)
        self.reclaim_h = read(RECLAIM_H)

    def test_the_record_carries_when_it_opened(self):
        self.assertIn("open_mono_ms", self.bridge)
        self.assertIn("open_wall_unix", self.bridge)
        self.assertIn("mr_bal_bridge_action_open_mono_ms", self.header)
        self.assertIn("mr_bal_bridge_action_open_wall_unix", self.header)

    def test_each_write_records_its_own_instant(self):
        """Per write, not per round: a round issues several."""
        self.assertIn("write_log_mono_ms", self.reclaim)
        self.assertIn("mr_bal_write_log_mono_ms", self.reclaim_h)
        # The push is the single place a write becomes an entry.
        push = self.reclaim[self.reclaim.index("static void write_log_push("):]
        push = push[:push.index("\n}\n") + 3]
        self.assertIn("mr_bal_reclaim_mono_ms()", push,
                      "the timestamp must be taken inside write_log_push so a "
                      "hook write and a real write record identically")

    def test_monotonic_not_wall_clock_for_comparison(self):
        self.assertIn("CLOCK_MONOTONIC", self.reclaim)
        self.assertIn("CLOCK_REALTIME", self.reclaim)

    def test_the_export_line_shows_the_times(self):
        export = self.bridge[self.bridge.index(
            "const char *mr_bal_bridge_action_export(void)"):]
        export = export[:export.index("\n}\n") + 3]
        self.assertIn("opened_mono_ms=", export)
        self.assertIn("t_mono_ms=", export)

    def test_unobserved_is_zero_not_a_guess(self):
        """A site that cannot read the clock reports 0, never a fabricated time."""
        fn = self.reclaim[self.reclaim.index("unsigned long long mr_bal_reclaim_mono_ms(void)"):]
        fn = fn[:fn.index("\n}\n") + 3]
        self.assertIn("return 0;", fn,
                      "an unreadable clock must return 0 so the record says "
                      "'not observed' instead of inventing a time")

    def test_the_export_buffer_fits_every_scope_and_timestamp(self):
        export = self.bridge[self.bridge.index(
            "const char *mr_bal_bridge_action_export(void)"):]
        capacity = int(re.search(r"static char line\[(\d+)\];", export).group(1))
        self.assertGreaterEqual(capacity, 4096,
                                "32 scopes plus per-write timestamps overflow a "
                                "smaller line and would truncate the record")


if __name__ == "__main__":
    unittest.main()
