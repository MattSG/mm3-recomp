"""The Xbox Live import keeps console secrets (keys, EEPROM with the online key, accounts) owner-only."""
import importlib.util
import os
import stat
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("import_xemu_console", ROOT / "tools/xbl/import_xemu_console.py")
xbl = importlib.util.module_from_spec(spec)
spec.loader.exec_module(xbl)


@unittest.skipIf(os.name == "nt", "POSIX permission bits")
class PrivateFilesTest(unittest.TestCase):
    def test_keys_are_owner_only_even_under_a_permissive_umask(self):
        old = os.umask(0o022)
        try:
            with tempfile.TemporaryDirectory() as folder:
                path = Path(folder) / "console_keys.txt"
                xbl.write_private(path, "RECOMP_XBOX_HDKEY=00\n")
                self.assertEqual(stat.S_IMODE(path.stat().st_mode), 0o600)
                self.assertEqual(path.read_text(), "RECOMP_XBOX_HDKEY=00\n")
        finally:
            os.umask(old)

    def test_existing_world_readable_file_is_tightened(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "eeprom.bin"
            path.write_bytes(b"x")
            path.chmod(0o644)
            xbl.private(path)
            self.assertEqual(stat.S_IMODE(path.stat().st_mode), 0o600)

    def test_rewriting_keeps_owner_only(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "console_keys.txt"
            path.write_text("old")
            path.chmod(0o666)
            xbl.write_private(path, "new")
            self.assertEqual(stat.S_IMODE(path.stat().st_mode), 0o600)


if __name__ == "__main__":
    unittest.main()
