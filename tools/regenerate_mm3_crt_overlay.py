"""Regenerate an MM3 CRT overlay with its real SEH frame helpers.

Keep the translated body unmodified. The preprocessor alias gives the body
the _gen symbol expected by the project's callee-saved register wrapper.
"""
import argparse
import pathlib
import subprocess
import sys

parser = argparse.ArgumentParser()
parser.add_argument("--function", required=True, type=lambda s: int(s, 0))
parser.add_argument("--gen-dir", required=True, type=pathlib.Path)
args = parser.parse_args()
root = pathlib.Path(__file__).resolve().parents[1]
body = subprocess.check_output(
    [sys.executable, "-m", "tools.recomp", str(root / "game_files/default.xbe"),
     "--function", hex(args.function), "--seh-prolog", "0x00094FC0",
     "--seh-epilog", "0x00094FFB", "--skip-binary-check"],
    cwd=root / "tools/xboxrecomp", text=True,
)
symbol = f"sub_{args.function:08X}"
if f"void {symbol}(void)" not in body:
    raise RuntimeError("Generator did not return the requested function")
prefix = (f'#define RECOMP_GENERATED_CODE\n#define {symbol} {symbol}_gen\n'
          '#include "recomp_funcs.h"\n#include <math.h>\n\n')
output = args.gen_dir / f"recomp_{args.function:08x}_abi.c"
output.write_text(prefix + body, encoding="utf-8")
print(output)
