"""The picker must read its row count from the scanned state, not a local.

A regression guard, written after the fact: both places that open the balanced
picker used to call mr_bal_bridge_scan_scopes() and keep the result in a local
`count` used only by the enrollment loop. s.bal_scope_count stayed at its
initial 0, so the panel drew "No application scope is reclaimable right now."
on a host where the same C code had just returned six scopes and four eligible
ones - the GUI and the library disagreed, and the GUI was wrong.

These tests read the source, because the defect is a missing assignment rather
than a wrong value: no runtime input would have made the old code pass.
"""

import os
import re
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAIN = os.path.join(ROOT, "src", "main.zm")


def read_main():
    with open(MAIN, encoding="utf-8") as handle:
        return handle.read()


class PickerScopeCountTest(unittest.TestCase):
    def setUp(self):
        self.text = read_main()

    def test_every_picker_opening_scans_into_the_window_state(self):
        """Each `s.bal_view = 1` must be preceded by bal_rescan(s)."""
        lines = self.text.split("\n")
        openers = [i for i, line in enumerate(lines)
                   if line.strip() == "s.bal_view = 1"]
        self.assertTrue(openers, "no picker opening found; test is stale")
        for index in openers:
            # The rescan follows the flag, because the flag is what switches the
            # panel on; reading either side is fine, so take a span around it.
            window = "\n".join(lines[max(0, index - 2):index + 10])
            self.assertIn("bal_rescan(s)", window,
                          "picker opened at line %d without bal_rescan(s); "
                          "s.bal_scope_count would stay at 0 and the panel "
                          "would claim nothing is reclaimable" % (index + 1))

    def test_the_enrollment_loop_reads_the_window_state(self):
        """Inside the picker-opening blocks, enrollment must read the field.

        A bare `let count = mr_bal_bridge_scan_scopes()` is legitimate elsewhere
        (bal_scope_index_by_name, and the CI run hook, neither of which feeds
        the picker), so the check is scoped to the blocks that do.
        """
        lines = self.text.split("\n")
        openers = [i for i, line in enumerate(lines)
                   if line.strip() == "s.bal_view = 1"]
        self.assertTrue(openers)
        for index in openers:
            block = "\n".join(lines[index:index + 12])
            self.assertNotIn(
                "let count = mr_bal_bridge_scan_scopes()", block,
                "the picker block at line %d scans into a local; the panel "
                "reads s.bal_scope_count and would stay at 0" % (index + 1))
            self.assertIn("let count = s.bal_scope_count", block)

    def test_bal_rescan_writes_the_field_it_is_responsible_for(self):
        lines = self.text.split("\n")
        start = next(i for i, line in enumerate(lines)
                     if line.startswith("fn bal_rescan("))
        body = []
        for line in lines[start + 1:]:
            if line.startswith("fn "):
                break
            body.append(line)
        joined = "\n".join(body)
        self.assertIn("s.bal_scope_count = mr_bal_bridge_scan_scopes()", joined,
                      "bal_rescan must write the field the picker reads")

    def test_the_empty_message_is_gated_on_the_scanned_count(self):
        """The message must sit directly behind the field bal_rescan writes."""
        needle = 'if s.bal_scope_count == 0:'
        self.assertIn(needle, self.text)
        for start in range(len(self.text)):
            start = self.text.find(needle, start)
            if start < 0:
                break
            following = self.text[start:start + 300]
            if "reclaimable" in following:
                return
        self.fail("the empty-state text is not behind s.bal_scope_count == 0")


if __name__ == "__main__":
    unittest.main()
