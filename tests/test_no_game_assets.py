"""Guard: no game assets, disc images or code translated from them may be tracked.

CI must never carry copyrighted game data. This fails if a tracked file looks like one.
"""
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FORBIDDEN_SUFFIXES = {".xbe", ".iso", ".xiso", ".xbx", ".xpr", ".xmv", ".xwb", ".xsb", ".bik", ".cmp", ".omb", ".cdds"}
FORBIDDEN_DIRS = ("game_files/", "game_files_dlc/", "src/recomp/gen/", "games/", "TDATA/", "UDATA/")
MAX_BYTES = 8 * 1024 * 1024


def tracked():
    out = subprocess.run(["git", "ls-files", "-z"], cwd=ROOT, capture_output=True, check=True).stdout
    return [p for p in out.decode().split("\0") if p]


class NoGameAssets(unittest.TestCase):
    def test_tracked_files(self):
        bad = []
        for rel in tracked():
            path = ROOT / rel
            if path.suffix.lower() in FORBIDDEN_SUFFIXES or rel.startswith(FORBIDDEN_DIRS):
                bad.append(rel)
            elif path.is_file() and path.stat().st_size > MAX_BYTES:
                bad.append(f"{rel} ({path.stat().st_size} bytes)")
        self.assertEqual(bad, [], "game assets or oversized files are tracked")

    def test_ignored_locations(self):
        for rel in ("game_files/x", "src/recomp/gen/x.c", "build/x"):
            r = subprocess.run(["git", "check-ignore", "-q", rel], cwd=ROOT)
            self.assertEqual(r.returncode, 0, f"{rel} is not git-ignored")


if __name__ == "__main__":
    unittest.main()
