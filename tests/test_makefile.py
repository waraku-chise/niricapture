import subprocess
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class MakefileContractTest(unittest.TestCase):
    def test_required_targets_exist(self):
        result = subprocess.run(
            ["make", "-n", "check", "BUILD_DIR=build"],
            cwd=ROOT,
            capture_output=True,
            text=True,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        text = result.stdout
        self.assertIn("niricapture-smoke", text)
        self.assertIn("niricapture-desktop-smoke", text)
        self.assertIn("unittest discover", text)

    def test_uninstall_target_is_scoped(self):
        result = subprocess.run(
            ["make", "-n", "uninstall", "PREFIX=/tmp/niricapture-prefix"],
            cwd=ROOT,
            capture_output=True,
            text=True,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("packaging/uninstall-local.sh --prefix", result.stdout)
        self.assertIn("/tmp/niricapture-prefix", result.stdout)

    def test_no_quickshell_or_hyprland_dependency(self):
        text = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertNotIn("Quickshell", text)
        self.assertNotIn("Hyprland", text)


if __name__ == "__main__":
    unittest.main()
