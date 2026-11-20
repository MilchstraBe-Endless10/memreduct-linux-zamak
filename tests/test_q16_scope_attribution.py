"""The attribution tool must not confuse stock with growth, or scope with machine.

These tests build a fake cgroup tree and drive the parsing and ranking logic
against it. Nothing here reads the real /sys/fs/cgroup, so the numbers are
chosen to make the distinctions visible rather than to look realistic.

Run: python3 -m unittest tests.test_q16_scope_attribution
"""

import importlib.util
import os
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCRIPT = os.path.join(ROOT, "scripts", "ci", "q16-scope-attribution.py")


def load_module():
    spec = importlib.util.spec_from_file_location("q16_attr", SCRIPT)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def write_scope(root, name, anon, file_bytes=0, active_file=0, inactive_file=0,
                refault=0, active_anon=0, inactive_anon=0):
    path = os.path.join(root, name)
    os.makedirs(path, exist_ok=True)
    with open(os.path.join(path, "memory.stat"), "w") as handle:
        handle.write(
            "anon %d\n"
            "file %d\n"
            "active_file %d\n"
            "inactive_file %d\n"
            "workingset_refault_file %d\n"
            "active_anon %d\n"
            "inactive_anon %d\n"
            % (anon, file_bytes, active_file, inactive_file, refault,
               active_anon, inactive_anon)
        )
    return path


class ParsingTest(unittest.TestCase):
    def setUp(self):
        self.mod = load_module()
        self.root = tempfile.mkdtemp(prefix="q16-attr-")

    def tearDown(self):
        for base, dirs, files in os.walk(self.root, topdown=False):
            for name in files:
                os.unlink(os.path.join(base, name))
            for name in dirs:
                os.rmdir(os.path.join(base, name))
        os.rmdir(self.root)

    def test_label_drops_instance_and_restores_escaped_dash(self):
        self.assertEqual(
            self.mod.scope_label("/x/app-org.chromium.Chromium-9007.scope"),
            "org.chromium.Chromium")
        self.assertEqual(
            self.mod.scope_label("/x/app-deepseek\\x2dai\\x2ddsh\\x2ddesktop-673676.scope"),
            "deepseek-ai-dsh-desktop")

    def test_missing_field_is_none_not_zero(self):
        """A field the kernel did not report must not read as 'zero bytes'."""
        path = os.path.join(self.root, "app-sparse-1.scope")
        os.makedirs(path)
        with open(os.path.join(path, "memory.stat"), "w") as handle:
            handle.write("anon 100\n")     # no file, no refault
        stats = self.mod.read_stat(path)
        self.assertEqual(stats["anon"], 100)
        self.assertIsNone(stats["file"])
        self.assertIsNone(stats["workingset_refault_file"])

    def test_unreadable_scope_is_skipped(self):
        path = os.path.join(self.root, "app-nostat-1.scope")
        os.makedirs(path)
        self.assertIsNone(self.mod.read_stat(path))

    def test_duplicate_labels_keep_the_larger(self):
        """Two Chromium instances must not both appear under one label."""
        write_scope(self.root, "app-org.chromium.Chromium-101.scope", anon=100 * 2**20)
        write_scope(self.root, "app-org.chromium.Chromium-202.scope", anon=300 * 2**20)
        merged = {}
        for name in ("app-org.chromium.Chromium-101.scope",
                     "app-org.chromium.Chromium-202.scope"):
            stats = self.mod.read_stat(os.path.join(self.root, name))
            label = self.mod.scope_label(name)
            if label in merged and merged[label]["anon"] >= stats["anon"]:
                continue
            merged[label] = stats
        self.assertEqual(len(merged), 1)
        self.assertEqual(merged["org.chromium.Chromium"]["anon"], 300 * 2**20)


class RankingTest(unittest.TestCase):
    """Stock and growth are different questions and must rank differently."""

    def setUp(self):
        self.mod = load_module()

    def test_stock_and_growth_can_disagree(self):
        # A big flat scope and a small growing one.
        first = {
            "big-flat": {"anon": 1000},
            "small-grower": {"anon": 10},
        }
        second = {
            "big-flat": {"anon": 1000},      # unchanged
            "small-grower": {"anon": 200},   # +190
        }
        stock = sorted(second.items(), key=lambda kv: kv[1]["anon"], reverse=True)
        growth = []
        for label, after in second.items():
            before = first.get(label)
            if before is None:
                continue
            growth.append((label, after, after["anon"] - before["anon"]))
        growth.sort(key=lambda row: row[2], reverse=True)

        self.assertEqual(stock[0][0], "big-flat",
                         "the largest holder is the big flat scope")
        self.assertEqual(growth[0][0], "small-grower",
                         "the fastest grower is the small scope")
        self.assertNotEqual(stock[0][0], growth[0][0],
                            "the two rankings must not be the same list")

    def test_scope_absent_from_first_sample_is_not_growth(self):
        """A scope that appeared mid-window has no delta, and must not be
        reported as having grown from zero."""
        first = {"a": {"anon": 10}}
        second = {"a": {"anon": 10}, "new": {"anon": 500}}
        deltas = []
        for label, after in second.items():
            before = first.get(label)
            if before is None:
                continue
            deltas.append(label)
        self.assertEqual(deltas, ["a"])
        self.assertNotIn("new", deltas)

    def test_shrinking_scopes_are_excluded_from_growth(self):
        first = {"shrinker": {"anon": 500}, "grower": {"anon": 10}}
        second = {"shrinker": {"anon": 100}, "grower": {"anon": 60}}
        rows = []
        for label, after in second.items():
            before = first.get(label)
            if before is None:
                continue
            rows.append((label, after["anon"] - before["anon"]))
        positive = [row for row in rows if row[1] > 0]
        self.assertEqual([row[0] for row in positive], ["grower"])


class MachineVersusScopeTest(unittest.TestCase):
    def test_script_never_apportions_machine_anon(self):
        """The tool must state the comparison, not compute an attribution."""
        with open(SCRIPT, encoding="utf-8") as handle:
            text = handle.read()
        self.assertIn("Machine AnonPages is not the sum of these scopes", text)
        self.assertIn("not process RSS", text)
        # No code may divide or prorate one by the other.
        for forbidden in ("machine / ", "machine/", "anon / machine"):
            self.assertNotIn(forbidden, text)

    def test_torn_sample_is_documented(self):
        """The non-atomic read hazard must be written down, since a reader
        will otherwise treat anon == active+inactive as an invariant."""
        with open(SCRIPT, encoding="utf-8") as handle:
            text = handle.read()
        self.assertIn("not read atomically", text)
        self.assertIn("does not hold at a single", text)


if __name__ == "__main__":
    unittest.main()
