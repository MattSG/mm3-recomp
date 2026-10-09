"""Regenerate MM3's translated C from game_files/default.xbe, natively.

    python scripts/generate_mm3.py [--out src/recomp/gen] [--skip-analysis]

Runs the whole recipe the build depends on, in order:
  1. analysis (disasm with mm3_runtime_function_seeds.json, func_id, abi)
  2. the batch: every function, minus recomp_manual.c's hand-written bodies,
     with the native tail constructors and the proven GUI boundary coalescing
  3. overlays the project wraps: memmove tails, alternate-SEH and ABI bodies
  4. runtime-proven entries (mm3_runtime_entry_repairs.json)
Needs a Python with capstone (scripts/setup.ps1 makes one).
"""
import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path

import io
import os
import tarfile

REPO = Path(__file__).resolve().parents[1]
# The generator is pinned separately from the runtime: this commit's lifter
# reproduces the translation the game was proven on (menus, races, AI).
# Newer lifters (upstream v0.13.x) translate ~3.6k functions differently and
# the race AI misdrives; see README "Regenerating". MM3_GENERATOR_DIR points
# at another checkout instead, for bisecting.
GENERATOR_COMMIT = '967ec30f362dee229183b4e112e928cd41830484'


def generator_dir():
    if os.environ.get('MM3_GENERATOR_DIR'):
        return Path(os.environ['MM3_GENERATOR_DIR']).resolve()
    out = REPO / 'tools/.generator' / GENERATOR_COMMIT[:12]
    if not (out / 'tools/recomp/__main__.py').exists():
        tar = subprocess.run(['git', '-C', str(REPO / 'tools/xboxrecomp'), 'archive', '--format=tar',
                              GENERATOR_COMMIT, 'tools'], check=True, capture_output=True).stdout
        with tarfile.open(fileobj=io.BytesIO(tar)) as t:
            t.extractall(out, filter='data')
    return out


TOOLKIT = generator_dir()
os.environ['MM3_GENERATOR_DIR'] = str(TOOLKIT)  # for the helper scripts
XBE = REPO / 'game_files/default.xbe'
DISASM = TOOLKIT / 'tools/disasm/output'
sys.path.insert(0, str(TOOLKIT))

# Computed reverse-copy entries share 0x93860's frame and end at its epilogue.
MEMMOVE_TAILS = (0x93B04, 0x93B58, 0x93B60, 0x93B70, 0x93B84)
MEMMOVE_END = 0x93B9D
# Bodies recomp_manual.c wraps (sub_X calls sub_X_gen): CRT functions on the
# alternate SEH prolog and FPO helpers whose callee-save contract is enforced.
SEH_OVERLAYS = (0x83FBB, 0x84020, 0x8427E, 0x842EA, 0x854CF, 0x858F3, 0x860AA,
                0x8629E, 0x96738, 0x97AFC, 0x9E85A)
ABI_OVERLAYS = (0x93C45, 0x1E7B8F, 0x2539A, 0x943ED, 0x9418C)
PRELUDE = '#define RECOMP_GENERATED_CODE\n{}#include "recomp_funcs.h"\n#include <math.h>\n\n'


def py(*args, **kw):
    return subprocess.run([sys.executable, *map(str, args)], cwd=TOOLKIT, check=True, **kw)


def write_tail_functions(path):
    functions = json.loads((DISASM / 'functions.json').read_text())
    by_start = {int(f['start'], 16): f for f in functions}
    for address in MEMMOVE_TAILS:
        f = by_start.get(address)
        if f is None:
            f = {'start': f'0x{address:08X}', 'name': f'sub_{address:08X}', 'section': '.text',
                 'confidence': 0.75, 'detection_method': 'vtable_thunk', 'num_instructions': 60,
                 'calls_to': [], 'called_by': []}
            functions.append(f)
        f.update(end=f'0x{MEMMOVE_END:08X}', size=MEMMOVE_END - address, has_prologue=False)
    path.write_text(json.dumps(functions, indent=1))


def write_runtime_entry_functions(path):
    functions = [f for f in json.loads((DISASM / 'functions.json').read_text())]
    entries = json.loads((REPO / 'mm3_runtime_entry_repairs.json').read_text())
    starts = {e['start'] for e in entries}
    functions = [f for f in functions if f['start'] not in starts]
    for e in entries:
        start, end = int(e['start'], 16), int(e['end'], 16)
        functions.append({'start': e['start'], 'end': e['end'], 'size': end - start,
                          'name': f'sub_{start:08X}', 'section': '.text', 'confidence': 1.0,
                          'detection_method': 'runtime_indirect_call',
                          'has_prologue': start == 0x104DD3, 'calls_to': [], 'called_by': [],
                          'num_instructions': 200})
    path.write_text(json.dumps(functions, indent=1))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--out', type=Path, default=REPO / 'src/recomp/gen')
    ap.add_argument('--skip-analysis', action='store_true', help='reuse tools/*/output')
    args = ap.parse_args()
    out = args.out.resolve()
    if not XBE.exists():
        sys.exit(f'{XBE} missing: run scripts/setup.ps1 -Iso <disc> first')
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)

    if not args.skip_analysis:
        # disasm reads the parsed header beside the XBE as <stem>_analysis.json.
        py('-m', 'tools.xbe_parser', XBE, '--json', XBE.with_name('default_analysis.json'))
        py('-m', 'tools.disasm', XBE, '--force', '--seed-functions', REPO / 'mm3_runtime_function_seeds.json')
        py('-m', 'tools.func_id', XBE)
        py('-m', 'tools.abi_analysis', XBE)

    gui = out / 'gui_boundary_functions.json'
    py(REPO / 'tools/prepare_mm3_gui_boundaries.py', '--output', gui)
    from tools.recomp.manual_scan import scan
    skip, wrap, _ = scan(str(REPO / 'src/recomp_manual.c'))
    manual = out / 'manual_functions.json'
    manual.write_text(json.dumps({hex(a): f'sub_{a:08X}' for a in skip | wrap}))
    py('-m', 'tools.recomp', XBE, '--all', '--split', 1000, '--gen-dir', out, '--functions', gui,
       '--manual-functions', manual, '--coalesce-functions', REPO / 'mm3_function_coalescences.json',
       '--skip-binary-check')

    tails = out / 'memmove_tail_functions.json'
    write_tail_functions(tails)
    for address in MEMMOVE_TAILS:
        name = f'sub_{address:08X}'
        body = py('-m', 'tools.recomp', XBE, '--function', hex(address), '--functions', tails,
                  '--seh-prolog', '0x00097AA4', '--skip-binary-check',
                  capture_output=True, text=True).stdout
        body = body.replace(f'void {name}(void)', f'void {name}_gen(void)')
        (out / f'recomp_{address:08x}_tail.c').write_text(PRELUDE.format('') + body)

    # Overlays keep the generated body under a _gen alias for the project wrapper.
    from tools.recomp import config
    from tools.recomp.translator import BatchTranslator
    config.configure_from_xbe(str(XBE))
    translator = BatchTranslator(
        xbe_path=str(XBE), func_json_path=str(DISASM / 'functions.json'),
        labels_json_path=str(DISASM / 'labels.json'),
        identified_json_path=str(TOOLKIT / 'tools/func_id/output/identified_functions.json'),
        abi_json_path=str(TOOLKIT / 'tools/abi_analysis/output/abi_functions.json'))
    for kind, addresses in (('seh', SEH_OVERLAYS), ('abi', ABI_OVERLAYS)):
        for address in addresses:
            name = f'sub_{address:08X}'
            body = translator.translate_single(address)
            if not body or f'void {name}(void)' not in body:
                sys.exit(f'generator did not return {name}')
            prelude = PRELUDE.format(f'#define {name} {name}_gen\n')
            (out / f'recomp_{address:08x}_{kind}.c').write_text(prelude + body)

    write_runtime_entry_functions(out / 'runtime_entry_functions.json')
    py(REPO / 'scripts/Generate-MM3RuntimeEntries.py', out)

    # Runtime semantics stay project-owned.
    shutil.copy(REPO / 'src/mm3_recomp_types.h', out / 'recomp_types.h')
    shutil.copy(REPO / 'tools/xboxrecomp/include/recomp_cpu.h', out / 'recomp_cpu.h')
    print(f'generated {len(list(out.glob("*.c")))} sources in {out}')


if __name__ == '__main__':
    main()
