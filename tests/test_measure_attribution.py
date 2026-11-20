"""Q14: the attribution harness must refuse, not improvise.

The measurement script samples the machine and, in its default mode, performs
no cleanup at all. The property that matters for safety is not "it can clean"
but "it cannot clean unless a human has already decided to": there is no
default command, the confirmation is a separate variable, and a relative path
is refused. These tests pin those refusals and the CSV shape that makes the
output analysable.

Nothing here writes to /sys, /proc/sys, or any cgroup. Every subprocess call
uses --no-reexec so the suite never starts a systemd unit.

Run: python3 -m unittest tests.test_measure_attribution
"""

import os
import subprocess
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCRIPT = os.path.join(ROOT, "scripts", "ci", "measure-click-attribution.sh")


def read(path):
    with open(path, encoding="utf-8") as handle:
        return handle.read()


def run(args, env_extra=None, timeout=30):
    env = dict(os.environ)
    env.pop("MR_CONFIRM_REAL_CLEAN", None)
    env.pop("MR_CLEAN_CMD", None)
    env.pop("INVOCATION_ID", None)
    if env_extra:
        env.update(env_extra)
    return subprocess.run(
        ["bash", SCRIPT] + args,
        capture_output=True,
        text=True,
        timeout=timeout,
        env=env,
        cwd=ROOT,
    )


class SyntaxTest(unittest.TestCase):
    def test_script_parses(self):
        result = subprocess.run(
            ["bash", "-n", SCRIPT], capture_output=True, text=True, timeout=30
        )
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_shebang_and_strict_mode(self):
        text = read(SCRIPT)
        self.assertTrue(text.startswith("#!/usr/bin/env bash"))
        self.assertIn("set -uo pipefail", text)


class CleanModeRefusalTest(unittest.TestCase):
    """Cleanup is opt-in twice over, and the second opt-in is a command."""

    def test_clean_mode_without_confirmation_exits_3(self):
        result = run(["--mode", "clean", "--no-reexec"])
        self.assertEqual(result.returncode, 3, result.stdout + result.stderr)
        self.assertIn("MR_CONFIRM_REAL_CLEAN=1", result.stderr)

    def test_clean_mode_with_confirmation_but_no_command_exits_3(self):
        result = run(
            ["--mode", "clean", "--no-reexec"],
            {"MR_CONFIRM_REAL_CLEAN": "1"},
        )
        self.assertEqual(result.returncode, 3, result.stdout + result.stderr)
        self.assertIn("MR_CLEAN_CMD", result.stderr)

    def test_relative_command_path_is_refused(self):
        result = run(
            ["--mode", "clean", "--no-reexec"],
            {"MR_CONFIRM_REAL_CLEAN": "1", "MR_CLEAN_CMD": "drop-caches"},
        )
        self.assertEqual(result.returncode, 3, result.stdout + result.stderr)
        self.assertIn("absolute path", result.stderr)

    def test_there_is_no_default_clean_command(self):
        """A default command would make the harness the thing that decides."""
        text = read(SCRIPT)
        self.assertNotIn("drop_caches", text)
        self.assertNotIn("/proc/sys/vm", text)
        # MR_CLEAN_CMD must only ever be read, never assigned a literal.
        for line in text.splitlines():
            stripped = line.strip()
            if stripped.startswith("#"):
                continue
            self.assertFalse(
                stripped.startswith("MR_CLEAN_CMD="),
                f"script assigns a default clean command: {stripped}",
            )

    def test_control_mode_needs_no_confirmation(self):
        """Control mode must never be gated behind a cleanup variable."""
        text = read(SCRIPT)
        control_gate = text.split("Control mode")[0]
        self.assertIn("control", text)
        # The refusal block sits inside the clean branch only.
        self.assertIn('if [[ "$MODE" == "clean" ]]', text)
        self.assertNotIn('if [[ "$MODE" == "control" ]]', text)


class UsageTest(unittest.TestCase):
    def test_unknown_mode_exits_2(self):
        result = run(["--mode", "bogus", "--no-reexec"])
        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)

    def test_missing_interface_exits_4(self):
        """A missing interface must abort, not produce a CSV full of zeros."""
        text = read(SCRIPT)
        self.assertIn("fail_missing", text)
        self.assertIn("exit 4", text)
        for path in ("/proc/meminfo", "/proc/pressure/memory", "/proc/vmstat",
                     "/proc/diskstats"):
            self.assertIn(path, text)


class ScopeDocumentationTest(unittest.TestCase):
    """The script's own comments must not claim a scope it does not have."""

    def test_session_and_slice_are_both_sampled(self):
        text = read(SCRIPT)
        self.assertIn("user@${USER_UID}.service", text)
        self.assertIn("session.slice", text)


class CsvShapeTest(unittest.TestCase):
    """samples.csv and samples.kv must be derived from one string."""

    def test_both_files_are_written_from_the_same_line(self):
        text = read(SCRIPT)
        self.assertIn('printf \'%s\\n\' "$line" >>"$CSV_KV"', text)
        self.assertIn('emit_csv_row "$line"', text)

    def test_header_names_come_from_the_values_line(self):
        text = read(SCRIPT)
        self.assertIn('names+=("${kv%%=*}")', text)
        self.assertIn('values+=("${kv#*=}")', text)


if __name__ == "__main__":
    unittest.main()
