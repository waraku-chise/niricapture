"""Installer/uninstaller preflight contracts (no writes, no sudo)."""

import os
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INSTALL = ROOT / "packaging/install-local.sh"
UNINSTALL = ROOT / "packaging/uninstall-local.sh"


@unittest.skipUnless(Path("/etc/arch-release").is_file(), "Arch Linux only")
class InstallerPreflight(unittest.TestCase):
    def run_script(self, script, *args, prefix):
        env = os.environ | {"NIRICAPTURE_PREFIX": str(prefix)}
        return subprocess.run(
            ["bash", str(script), *args],
            env=env,
            text=True,
            capture_output=True,
        )

    def test_install_check_makes_no_writes(self):
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory) / "prefix"
            result = self.run_script(INSTALL, "--check", prefix=prefix)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("Preflight passed", result.stdout)
            self.assertIn("Will not edit Niri or DMS configuration", result.stdout)
            self.assertFalse(prefix.exists())

    def test_install_refuses_to_overwrite(self):
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory) / "prefix"
            target = prefix / "bin/niricapture"
            target.parent.mkdir(parents=True)
            target.write_text("existing")
            result = self.run_script(INSTALL, "--check", prefix=prefix)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("Refusing to overwrite", result.stderr)
            self.assertEqual(target.read_text(), "existing")

    def test_uninstall_check_only_lists_known_files(self):
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory) / "prefix"
            binary = prefix / "bin/niricapture"
            binary.parent.mkdir(parents=True)
            binary.write_text("binary")
            config = Path(directory) / "config.kdl"
            config.write_text("keep me")
            result = self.run_script(UNINSTALL, "--check", prefix=prefix)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("Would remove", result.stdout)
            self.assertTrue(binary.exists())
            self.assertTrue(config.exists())

    def test_uninstall_refuses_unscoped_prefixes(self):
        result = self.run_script(UNINSTALL, "--check", prefix=Path("/"))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Refusing", result.stderr)


if __name__ == "__main__":
    unittest.main()
