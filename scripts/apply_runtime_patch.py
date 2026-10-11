"""Apply this checkout's MM3 runtime fixes to the pinned xboxrecomp submodule."""
import argparse
from pathlib import Path
import subprocess
import sys


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--toolkit-dir', type=Path, default=root / 'tools/xboxrecomp')
    args = parser.parse_args()
    patch = root / 'patches/xboxrecomp-mm3.patch'
    toolkit = args.toolkit_dir.resolve()
    if not (toolkit / 'CMakeLists.txt').is_file():
        parser.error('Initialize tools/xboxrecomp with git submodule update --init --recursive.')

    def apply(*options):
        return subprocess.run(['git', '-C', str(toolkit), 'apply', *options, str(patch)],
                              text=True, capture_output=True)

    forward = apply('--check')
    if forward.returncode == 0:
        result = apply()
        if result.returncode:
            print(result.stderr, file=sys.stderr, end='')
            return result.returncode
        print('Applied MM3 Xbox runtime fixes.')
        return 0
    if apply('--reverse', '--check').returncode == 0:
        print('MM3 Xbox runtime fixes are already applied.')
        return 0
    print('The MM3 runtime patch does not match this toolkit checkout. '
          'Preserve local changes and restore the pinned submodule version before retrying.',
          file=sys.stderr)
    print(forward.stderr, file=sys.stderr, end='')
    return 1


if __name__ == '__main__':
    raise SystemExit(main())
