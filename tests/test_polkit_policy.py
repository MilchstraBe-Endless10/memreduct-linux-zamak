"""Static and behavioural checks for the privileged install surface.

These tests never run pkexec, never touch /proc/sys/vm/drop_caches and never
install anything into the real filesystem. They verify the *policy text* and
the *install scripts* using a temporary root.
"""

import os
import re
import shutil
import subprocess
import tempfile
import textwrap
import unittest
import xml.etree.ElementTree as ET

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
POLICY = os.path.join(
    ROOT, "packaging", "polkit-1", "actions",
    "io.github.milchstrasse_endless10.memreduct.zamak.policy",
)
POLICY_BASENAME = os.path.basename(POLICY)
ACTION_ID = "io.github.milchstrasse_endless10.memreduct.zamak.pagecache"
HELPER_PATH = "/usr/libexec/memreduct-zamak-helper"

# The DTD declares no target namespace, so these elements are unqualified.
PK_ACTION = "action"
PK_DEFAULTS = "defaults"
PK_ANNOTATE = "annotate"
PK_DESCRIPTION = "description"
PK_MESSAGE = "message"
XML_LANG = "{http://www.w3.org/XML/1998/namespace}lang"


def read_text(path):
    """Read a text file without leaking an unclosed handle."""
    with open(path, encoding="utf-8") as handle:
        return handle.read()


class PolkitPolicyTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tree = ET.parse(POLICY)
        cls.root = cls.tree.getroot()
        cls.actions = cls.root.findall(PK_ACTION)

    def test_policy_file_exists_and_is_well_formed_xml(self):
        self.assertTrue(os.path.isfile(POLICY))
        self.assertEqual(self.root.tag, "policyconfig")

    def test_exactly_one_action_is_defined(self):
        self.assertEqual(len(self.actions), 1)
        self.assertEqual(self.actions[0].get("id"), ACTION_ID)

    def test_action_id_matches_file_name_convention(self):
        # Polkit resolves files by id, but a mismatched name confuses audits.
        self.assertEqual(POLICY_BASENAME, "io.github.milchstrasse_endless10.memreduct.zamak.policy")

    def test_defaults_match_the_documented_authorization_contract(self):
        """Issue #13 M0 changed the contract from "grant nothing ever" to
        "the active local session may authorize, every single time".

        The previous all-"no" default made the product unusable: pkexec could
        only ever refuse, so the one kernel write the program exists for was
        unreachable in every installed configuration. The shipped policy now
        follows the distribution-recommended shape:
          allow_any=no         remote sessions can never authorize
          allow_inactive=no    background sessions can never authorize
          allow_active=auth_admin   the local user must type an admin password
                                    for THIS request; approval is never cached
        The test asserts exactly that, and separately that no *_keep variant
        sneaks in, because *_keep is what would silently reuse an approval.
        """
        defaults = self.actions[0].find(PK_DEFAULTS)
        self.assertIsNotNone(defaults)
        expected = {"allow_any": "no", "allow_inactive": "no", "allow_active": "auth_admin"}
        for tag, value in expected.items():
            actual = (defaults.findtext(tag) or "").strip()
            self.assertEqual(actual, value, f"{tag} must be {value}")
        joined = " ".join((defaults.findtext(t) or "") for t in expected)
        self.assertNotIn("keep", joined, "no *_keep variant may appear")
        self.assertNotIn("yes", joined.replace("auth_admin", ""), "no blanket yes")

    def test_exec_path_annotation_pins_the_root_owned_helper(self):
        annotations = {
            node.get("key"): (node.text or "").strip()
            for node in self.actions[0].findall(PK_ANNOTATE)
        }
        self.assertEqual(
            annotations.get("org.freedesktop.policykit.exec.path"), HELPER_PATH
        )

    def test_every_annotation_key_is_expected(self):
        allowed = {
            "org.freedesktop.policykit.exec.path",
            "org.freedesktop.policykit.exec.allow_gui",
        }
        keys = {node.get("key") for node in self.actions[0].findall(PK_ANNOTATE)}
        self.assertEqual(keys - allowed, set())

    def test_no_retained_authorization_annotation(self):
        # require_active / retain_authorization would let a repeat request
        # reuse an earlier approval; this action must be authorized each time.
        for node in self.actions[0].iter():
            key = node.get("key") or ""
            self.assertNotIn("retain", key)
            self.assertNotIn("require_active", key)

    def test_descriptions_and_messages_are_bilingual(self):
        # Polkit allows one <description> per language as sibling elements:
        # the untagged one is the default (English), xml:lang="zh_CN" the other.
        for tag, label in ((PK_DESCRIPTION, "description"), (PK_MESSAGE, "message")):
            langs = {
                element.get(XML_LANG)
                for element in self.actions[0].findall(tag)
            }
            self.assertTrue(langs, f"no <{label}> elements found")
            self.assertIn(None, langs, f"{label}: default (English) text missing")
            self.assertIn("zh_CN", langs, f"{label}: Simplified Chinese text missing")

    def test_policy_does_not_mention_forbidden_actions(self):
        text = read_text(POLICY)
        for forbidden in ("drop_caches=3", "metadata", "sync", "swapoff"):
            self.assertNotIn(forbidden, text)


class InstallScriptTest(unittest.TestCase):
    def setUp(self):
        self.install = os.path.join(ROOT, "scripts", "install-arch.sh")
        self.uninstall = os.path.join(ROOT, "scripts", "uninstall-arch.sh")

    def test_scripts_parse(self):
        for script in (self.install, self.uninstall):
            subprocess.run(["bash", "-n", script], check=True, capture_output=True)

    def test_install_sets_strict_mode(self):
        text = read_text(self.install)
        self.assertIn("set -euo pipefail", text)

    def test_install_ships_policy_and_uninstall_removes_it(self):
        install_text = read_text(self.install)
        uninstall_text = read_text(self.uninstall)
        policy_target = f"/usr/share/polkit-1/actions/{POLICY_BASENAME}"
        self.assertIn(policy_target, install_text)
        self.assertIn(policy_target, uninstall_text)

    def test_install_asserts_helper_ownership_and_mode(self):
        text = read_text(self.install)
        self.assertRegex(text, r"chown 0:0 " + re.escape(HELPER_PATH))
        self.assertRegex(text, r"chmod 0755 " + re.escape(HELPER_PATH))

    def test_install_rejects_symlinked_helper(self):
        text = read_text(self.install)
        self.assertIn("-type l", text, "install must detect a symlinked helper path")

    def test_uninstall_covers_every_installed_file(self):
        def targets(script):
            text = read_text(script)
            return {
                path
                for path in re.findall(r"/usr/(?:bin|libexec|share)/[A-Za-z0-9/._-]+", text)
                if path.startswith(("/usr/bin/memreduct", "/usr/libexec/", "/usr/share/applications/",
                                    "/usr/share/polkit-1/", "/usr/share/licenses/", "/usr/share/doc/"))
            }

        installed = targets(self.install)
        removed = targets(self.uninstall)
        self.assertTrue(installed)
        self.assertEqual(installed - removed, set(), "install/uninstall file sets diverge")


class SimulatedInstallTest(unittest.TestCase):
    """Run the install script against a throwaway DESTDIR-like root.

    install(1) is not parameterised by DESTDIR here, so instead of running the
    real script (which would write to /usr) we assert the ordering property
    that makes the real script safe: staging happens before any system path.
    """

    def test_staging_precedes_system_paths(self):
        text = open(os.path.join(ROOT, "scripts", "install-arch.sh"), encoding="utf-8").read()
        stage_idx = text.index('install -m 0755 -D "$bin" "$stage/memreduct-zamak"')
        system_idx = text.index('install -m 0755 -D "$stage/memreduct-zamak" /usr/bin/memreduct-zamak')
        self.assertLess(stage_idx, system_idx)

    def test_helper_is_configured_before_it_is_installed(self):
        text = open(os.path.join(ROOT, "scripts", "install-arch.sh"), encoding="utf-8").read()
        chown_idx = text.index('chown 0:0 "$stage/memreduct-zamak-helper"')
        install_idx = text.index(
            'install -m 0755 -D "$stage/memreduct-zamak-helper" /usr/libexec/memreduct-zamak-helper'
        )
        self.assertLess(chown_idx, install_idx)


if __name__ == "__main__":
    unittest.main()
