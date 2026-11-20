"""The result text must not imply that a reclaim keeps memory lower.

A 33.5 MB file-page reclaim on Chrome grew its file pages back past the
starting point inside 60 s, with refault moving only 0.03 MB - the workload
wrote new cache; the dropped pages were never read back. So "freed" means
"freed until the application touches that file again", and the panel has to say
so rather than let a user conclude the tool holds memory down.
"""

import os
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAIN = os.path.join(ROOT, "src", "main.zm")


class CopyTest(unittest.TestCase):
    def setUp(self):
        with open(MAIN, encoding="utf-8") as handle:
            self.text = handle.read()

    def test_the_success_text_states_the_cache_can_grow_back(self):
        self.assertIn("按需回收所选应用的部分文件缓存", self.text)
        self.assertIn("后续使用可能使缓存重新增长", self.text)

    def test_the_english_copy_says_the_same_thing(self):
        self.assertIn("reclaimed on demand", self.text)
        self.assertIn("grow that cache back", self.text)

    def test_the_copy_carries_no_retention_promise(self):
        """Guard the phrasing that would turn an observation into a promise."""
        banned = ("内存将持续", "持续保持低位", "permanently lower",
                  "stays lower", "keeps memory low", "释放了 1 GiB")
        for phrase in banned:
            self.assertNotIn(phrase, self.text,
                             "the panel must not promise sustained lower "
                             "occupancy: %r" % phrase)


if __name__ == "__main__":
    unittest.main()
