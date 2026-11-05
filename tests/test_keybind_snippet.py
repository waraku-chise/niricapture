import re
import unittest
from pathlib import Path


SNIPPET = Path(__file__).resolve().parents[1] / "packaging/niricapture-keybinds.kdl"


class KeybindSnippetTest(unittest.TestCase):
    def test_snippet_is_balanced_and_launches_all_modes(self):
        text = SNIPPET.read_text(encoding="utf-8")
        self.assertEqual(text.count("binds {"), 1)
        self.assertEqual(text.count("{"), text.count("}"))
        mode_args = set()
        for line in text.splitlines():
            if 'spawn "niricapture"' not in line:
                continue
            args = re.findall(r'"([^"]*)"', line)
            self.assertEqual(args[0], "niricapture")
            mode_args.add(tuple(args[1:]))
        self.assertEqual(
            mode_args,
            {
                ("--capture-region",),
                ("--capture-window",),
                ("--capture-all",),
                ("--capture-fullscreen",),
            },
        )

    def test_installer_does_not_edit_niri_config(self):
        repo = Path(__file__).resolve().parents[1]
        installer = (repo / "packaging/install-local.sh").read_text(encoding="utf-8")
        self.assertNotIn("config.kdl", installer)
        self.assertIn("Will not edit Niri or DMS configuration", installer)


if __name__ == "__main__":
    unittest.main()
