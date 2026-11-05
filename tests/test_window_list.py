"""Niri window-list contract using a synthetic niri CLI."""

import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / "build" / "niricapture"


@unittest.skipUnless(BINARY.is_file(), "niricapture not built")
class WindowListCli(unittest.TestCase):
    def run_with_niri(self, body):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            niri = root / "niri"
            niri.write_text("#!/usr/bin/env python3\n" + body)
            niri.chmod(0o755)
            runtime = root / "runtime"
            runtime.mkdir(mode=0o700)
            env = os.environ | {
                "QT_QPA_PLATFORM": "offscreen",
                "PATH": f"{root}:" + os.environ["PATH"],
                "XDG_RUNTIME_DIR": str(runtime),
                "HOME": str(root),
            }
            return subprocess.run(
                [str(BINARY), "--list-windows"],
                env=env,
                text=True,
                capture_output=True,
            )

    def test_window_list_is_machine_readable(self):
        result = self.run_with_niri(
            "import json, sys\n"
            "if sys.argv[1:] != ['msg', '--json', 'windows']:\n"
            "    sys.exit(2)\n"
            "print(json.dumps([{ 'id': 7, 'title': 'Editor',"
            " 'app_id': 'dev.editor', 'workspace_id': 2, 'is_focused': False, 'is_floating': True, 'layout': {'window_size': [600, 400]} }]))\n"
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        windows = json.loads(result.stdout)
        self.assertEqual(
            windows,
            [{
                "id": "7",
                "title": "Editor",
                "app_id": "dev.editor",
                "workspace_id": 2,
                "is_focused": False,
                "is_floating": True,
                "width": 600,
                "height": 400,
            }],
        )

    def test_niri_failure_is_reported(self):
        result = self.run_with_niri("import sys\nsys.exit(9)\n")
        self.assertEqual(result.returncode, 1)
        self.assertIn("Could not list Niri windows", result.stderr)


if __name__ == "__main__":
    unittest.main()
