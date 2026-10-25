"""Apply patches/xboxrecomp/*.patch to the upstream xboxrecomp submodule.

The submodule stays on the upstream commit this repository pins; the patches
(generic runtime changes this port needs, kept here until they are upstream)
go into its working tree. Running it again is a no-op once they are applied.

  python scripts/patch_xboxrecomp.py           apply if needed
  python scripts/patch_xboxrecomp.py --check   exit 1 unless applied

To change a patch: edit in tools/xboxrecomp, commit there on top of the pin,
then regenerate the series with
  git -C tools/xboxrecomp format-patch --binary --zero-commit --no-signature \
      -o ../../patches/xboxrecomp <pin>..HEAD
"""
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
SUB = REPO / "tools/xboxrecomp"
PATCHES = sorted((REPO / "patches/xboxrecomp").glob("*.patch"))


def git_apply(*args):
    return subprocess.run(["git", "-C", str(SUB), "apply", "--whitespace=nowarn", *args],
                          capture_output=True, text=True)


def main():
    if not PATCHES:
        return 0
    # The last patch reverses cleanly only on top of the whole series.
    if git_apply("--reverse", "--check", str(PATCHES[-1])).returncode == 0:
        return 0                                   # already applied
    if "--check" in sys.argv:
        print("xboxrecomp patches are not applied", file=sys.stderr)
        return 1
    result = git_apply(*map(str, PATCHES))
    if result.returncode:
        sys.stderr.write(result.stderr)
        print("xboxrecomp patches do not apply: is tools/xboxrecomp on the pinned "
              "upstream commit with no other changes?", file=sys.stderr)
        return 1
    print(f"applied {len(PATCHES)} xboxrecomp patches")
    return 0


if __name__ == "__main__":
    sys.exit(main())
