"""Regenerate an MM3 CRT overlay with its real SEH frame helpers.

Keep the translated body unmodified. The preprocessor alias gives the body
the _gen symbol expected by the project's callee-saved register wrapper.
"""
import argparse
import pathlib
import re
import sys

parser = argparse.ArgumentParser()
selection = parser.add_mutually_exclusive_group(required=True)
selection.add_argument("--function", action="append", type=lambda s: int(s, 0))
selection.add_argument("--all-existing", action="store_true")
parser.add_argument("--gen-dir", required=True, type=pathlib.Path)
args = parser.parse_args()
root = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / "tools/xboxrecomp"))
from tools.recomp import config
from tools.recomp.__main__ import find_data_files
from tools.recomp.translator import BatchTranslator

if args.all_existing:
    targets = [(int(m[1], 16), p) for p in sorted(args.gen_dir.glob("*.c"))
               if (m := re.fullmatch(r"recomp_([0-9a-fA-F]{8})_(?:abi|seh)\.c", p.name))]
    if not targets:
        raise RuntimeError("No existing CRT overlays found")
else:
    targets = [(addr, args.gen_dir / f"recomp_{addr:08x}_abi.c")
               for addr in args.function]
xbe = root / "game_files/default.xbe"
config.configure_from_xbe(str(xbe))
data = find_data_files()
translator = BatchTranslator(
    xbe_path=str(xbe), func_json_path=data["functions"],
    labels_json_path=data.get("labels"), identified_json_path=data.get("identified"),
    abi_json_path=data.get("abi"),
)
# Prepare every body before replacing any overlay. Detect both MM3 SEH helper
# families from the XBE, including the alternate prolog at 0x00097AA4.
outputs = []
for addr, output in targets:
    body = translator.translate_single(addr)
    symbol = f"sub_{addr:08X}"
    if not body or f"void {symbol}(void)" not in body:
        raise RuntimeError(f"Generator did not return {symbol}")
    prefix = (f'#define RECOMP_GENERATED_CODE\n#define {symbol} {symbol}_gen\n'
              '#include "recomp_funcs.h"\n#include <math.h>\n\n')
    outputs.append((output, prefix + body))
for output, body in outputs:
    output.write_text(body, encoding="utf-8")
    print(output, flush=True)
