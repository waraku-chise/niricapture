"""CLI fail-closed checks for invalid capture combinations."""

import os
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / "build" / "niricapture"


@unittest.skipUnless(BINARY.is_file(), "niricapture not built")
class FailClosedCli(unittest.TestCase):
    def run_cli(self, *args):
        env = os.environ | {"QT_QPA_PLATFORM": "offscreen"}
        return subprocess.run(
            [str(BINARY), *args],
            env=env,
            text=True,
            capture_output=True,
        )

    def test_empty_file_option_is_usage_error(self):
        result = self.run_cli("--file=", "--capture-fullscreen", "--copy")
        self.assertEqual(result.returncode, 2, result.stderr)

    def test_empty_pin_option_is_usage_error(self):
        result = self.run_cli("--pin=")
        self.assertEqual(result.returncode, 2, result.stderr)

    def test_empty_window_id_is_usage_error(self):
        result = self.run_cli("--window-id=")
        self.assertEqual(result.returncode, 2, result.stderr)

    def test_capture_modes_are_mutually_exclusive(self):
        result = self.run_cli("--capture-region", "--capture-all")
        self.assertEqual(result.returncode, 2, result.stderr)

    def test_output_requires_fullscreen_mode(self):
        result = self.run_cli("--capture-region", "--output", "DP-1")
        self.assertEqual(result.returncode, 2, result.stderr)

    def test_window_id_requires_window_mode(self):
        result = self.run_cli("--capture-region", "--window-id", "12")
        self.assertEqual(result.returncode, 2, result.stderr)


if __name__ == "__main__":
    unittest.main()
