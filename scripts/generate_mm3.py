"""Regenerate MM3's translated C from game_files/default.xbe, natively.

    python scripts/generate_mm3.py [--out src/recomp/gen] [--skip-analysis]

Runs the whole recipe the build depends on, in order:
  1. analysis (disasm with mm3_runtime_function_seeds.json, func_id, abi)
  2. the batch: every function, minus recomp_manual.c's hand-written bodies,
     with the native tail constructors and the proven GUI boundary coalescing
  3. overlays the project wraps: memmove tails, alternate-SEH and ABI bodies
  4. runtime-proven entries (mm3_runtime_entry_repairs.json)
Tested with Capstone 5.0.7; scripts/setup.ps1 installs that disassembler version.
"""
import argparse
import json
import re
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
    out = REPO / 'tools/.generator' / (GENERATOR_COMMIT[:12] + '-fp80')
    if not (out / 'tools/recomp/__main__.py').exists():
        tar = subprocess.run(['git', '-C', str(REPO / 'tools/xboxrecomp'), 'archive', '--format=tar',
                              GENERATOR_COMMIT, 'tools'], check=True, capture_output=True).stdout
        with tarfile.open(fileobj=io.BytesIO(tar)) as t:
            t.extractall(out, filter='data')
        # Toolkit cd491b1 (after the pin): fld tbyte fell through to MEMF, a
        # float read of the significand's low bytes. The CRT pow() range check
        # (32767.5 at 0x393816) then read 0.0 and every pow() overflowed,
        # which capped car throttle and gears.
        lifter = out / 'tools/recomp/lifter.py'
        old = '''                    return [f"fp_push(MEMF({_fmt_mem(ops[0])})); /* fld */"]'''
        new = ('''                    elif ops[0].mem_size == 10:
                        return [f"fp_push(MEMFP80({_fmt_mem(ops[0])})); /* fld tbyte */"]
''' + old)
        text = lifter.read_text()
        if text.count(old) != 1:
            sys.exit('pinned lifter changed: fld tbyte patch does not apply')
        lifter.write_text(text.replace(old, new))
    return out


XBE = REPO / 'game_files/default.xbe'
TOOLKIT = None  # set by setup_toolkit(); importing this module has no side effects
DISASM = None


def setup_toolkit():
    global TOOLKIT, DISASM
    TOOLKIT = generator_dir()
    os.environ['MM3_GENERATOR_DIR'] = str(TOOLKIT)  # for the helper scripts
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


# Statements that leave the flags as they were, so a walk back through them is exact.
FLAG_NEUTRAL = re.compile(
    r'^\s*(?:$|/\*.*\*/$|\(void\)|_(?:cf|zf) = [^;]*;(?: /\*.*\*/)?$'
    r'|if \(.*\) (?:goto loc_[0-9A-F]+;|\{ .*return; \}|\(void\)0; /\*.*\*/)(?: /\*.*\*/)?$)')
FLAG_LABEL = re.compile(r'^(loc_[0-9A-F]+): ;$')
FLAG_JUMP_OUT = re.compile(r'^\s*(goto |return\b|esp \+= \d+; return)')
FLAG_FUNC = re.compile(r'^\w[^;]*\b(sub_[0-9A-F]+)\(void\)\s*$')


def func_name(lines, i):
    """The sub_XXXX whose definition is the nearest one above line i."""
    return next((lines[k].split('(')[0].split()[-1] for k in range(i, -1, -1)
                 if FLAG_FUNC.match(lines[k])), '?')


def flag_setter_zf(text):
    """The C statement that sets _zf from a flag-producing line, or None.

    Each form matches the lifter's own je/jne condition for that setter: cmp and
    test keep their operands in _fa/_fb, and the result-based setters keep the
    result in _fa (see _make_condition in tools/recomp/lifter.py).
    """
    if '/* cmp ' in text:
        return '_zf = (_fa == _fb);'
    if '/* test ' in text:
        return '_zf = ((_fa & _fb) == 0);'
    if re.search(r'/\* (\w+ )?(sub|dec|inc|add|and|or|xor|neg) result', text):
        return '_zf = (_fa == 0);'
    return None


def fix_flag_joins(out):
    """Materialize ZF where a je/jne reads the never-set `_flags` fallback.

    The pinned lifter keeps flags lazily: a cmp leaves its operands in _fa/_fb, an
    arithmetic op leaves its result in _fa, a test leaves both operands. When a
    je/jne has no flag state it falls back to `_flags`, which nothing ever assigns,
    so the branch is never taken. Lua's code generator hit this at 0x89689
    (`x ~= y` compiled as `x ~= nil`), which kept System Link hosts from spawning a car.

    The fix walks back from each such branch through the statements that leave the
    flags alone (labels, conditional jumps, _cf writes) to every flag producer that
    reaches it, across labels and their gotos. When all of them are one of the
    forms above, each producer stores ZF in a per-function _zf and the branch tests
    that. Anything else is left as it was and listed, with the line that blocked it,
    so a silent miscompile is visible in the generator output.

    Returns (fixed, unfixed): the branch count rewritten, and a list of
    (file, function, label, line, reason) for every je/jne still on `_flags`.
    """
    jcc = re.compile(r'^(\s*)if \(_flags /\* (je|jne): [^*]*\*/\) (.*)$')
    fixed = 0
    unfixed = []
    for path in sorted(out.glob('recomp_*.c')):
        lines = path.read_text().splitlines()
        targets = {}
        for i, line in enumerate(lines):
            for m in re.finditer(r'goto (loc_[0-9A-F]+);', line):
                targets.setdefault(m.group(1), []).append(i)
        blocked = []

        def sources(j, seen=frozenset()):
            """Lines that set the flags in effect after line j, or None when one is unknown."""
            while j >= 0 and FLAG_NEUTRAL.match(lines[j]):
                j -= 1
            if j < 0:
                blocked.append(None)  # function entry: flags are undefined
                return None
            label = FLAG_LABEL.match(lines[j])
            if label:
                if label.group(1) in seen:
                    return []
                seen = seen | {label.group(1)}
                found = []
                for g in targets.get(label.group(1), []):
                    s = sources(g - 1, seen)
                    if s is None:
                        return None
                    found += s
                prev = j - 1
                while prev >= 0 and not lines[prev].strip():
                    prev -= 1
                if prev >= 0 and not FLAG_JUMP_OUT.match(lines[prev]):
                    s = sources(prev, seen)
                    if s is None:
                        return None
                    found += s
                return found
            if flag_setter_zf(lines[j]) is None:
                blocked.append(j)
                return None
            return [j]

        inserts = {}   # setter line -> _zf statement
        rewrites = {}  # branch line -> replacement
        decls = set()
        for i, line in enumerate(lines):
            m = jcc.match(line)
            if not m:
                continue
            indent, cond, rest = m.groups()
            if '(void)0; /* goto' in rest or 'RECOMP_DEAD_BRANCH(' in rest:
                # translator.py comments out a goto whose label is not in the function
                # (mark_dead_branches rewrites that to RECOMP_DEAD_BRANCH). The branch
                # has no ZF to materialize.
                unfixed.append((path.name, func_name(lines, i), '-', i + 1,
                                'target label outside this function; translator dropped the jump'))
                continue
            func = func_name(lines, i)
            prev = next((k for k in range(i - 1, -1, -1) if lines[k].strip()), None)
            label = FLAG_LABEL.match(lines[prev]) if prev is not None else None
            blocked.clear()
            srcs = sources(i - 1)
            decl = next((k for k in range(i, -1, -1)
                         if 'int _flags = 0; /* fallback flag var */' in lines[k]), None)
            if srcs is None or not srcs or decl is None:
                if srcs is not None and not srcs:
                    reason = 'no flag producer reaches it'
                elif decl is None:
                    reason = 'no _flags declaration'
                else:
                    at = blocked[-1] if blocked else None
                    reason = ('function entry' if at is None else
                              f'blocked at {path.name}:{at + 1}: {lines[at].strip()[:90]}')
                unfixed.append((path.name, func, label.group(1) if label else '-', i + 1, reason))
                continue
            for j in srcs:
                indent_j = lines[j][:len(lines[j]) - len(lines[j].lstrip())]
                inserts[j] = f'{indent_j}{flag_setter_zf(lines[j])}'
            decls.add(decl)
            rewrites[i] = f'{indent}if ({"" if cond == "je" else "!"}_zf) {rest}'
            fixed += 1
        if not rewrites:
            continue
        for d in decls:
            if '_zf' not in lines[d]:
                lines[d] += ' int _zf = 0;'
        new = []
        for k, line in enumerate(lines):
            new.append(rewrites.get(k, line))
            if k in inserts:
                new.append(inserts[k])
        path.write_text('\n'.join(new) + '\n')
    print(f'flag joins materialized: {fixed}')
    print(f'je/jne left on the _flags fallback: {len(unfixed)}')
    for name, func, label, line, reason in unfixed:
        print(f'  {name}:{line} {func} {label}: {reason}')
    return fixed, unfixed


DEAD_BRANCH = re.compile(r'\(void\)0; /\* goto (loc_([0-9A-Fa-f]{8})) - dead code, label not in function \*/')


def mark_dead_branches(out):
    """Turn translator.py's dead-branch no-op into a logged RECOMP_DEAD_BRANCH.

    translator.py replaces a conditional `goto loc_X;` whose label is not in its
    function with `(void)0; /* goto loc_X - dead code ... */`, so a branch that is
    reached would silently fall through. This pass rewrites each such site to
    `RECOMP_DEAD_BRANCH(0xXXXXXXXXu);`, which src/mm3_recomp_types.h defines to log
    the target and stops execution. Current translation recovers overlapping
    direct-target streams; this pass handles legacy missing-label markers.

    Returns the number of sites rewritten.
    """
    marked = 0
    for path in sorted(out.glob('recomp_*.c')):
        text = path.read_text()
        text, n = DEAD_BRANCH.subn(
            lambda m: f'RECOMP_DEAD_BRANCH(0x{m.group(2)}u); /* goto {m.group(1)} - label not in function */',
            text)
        if n:
            path.write_text(text)
            marked += n
    print(f'dead branches marked: {marked}')
    return marked


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--out', type=Path, default=REPO / 'src/recomp/gen')
    ap.add_argument('--skip-analysis', action='store_true', help='reuse tools/*/output')
    ap.add_argument('--fix-flag-joins', type=Path, metavar='GEN_DIR',
                    help='run only the je/jne flag join pass over an existing generated directory')
    ap.add_argument('--mark-dead-branches', type=Path, metavar='GEN_DIR',
                    help='run only the dead-branch marking pass over an existing generated directory')
    args = ap.parse_args()
    if args.fix_flag_joins:
        fix_flag_joins(args.fix_flag_joins.resolve())
        return
    if args.mark_dead_branches:
        mark_dead_branches(args.mark_dead_branches.resolve())
        return
    setup_toolkit()
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
    fix_flag_joins(out)
    mark_dead_branches(out)

    # Runtime semantics stay project-owned.
    shutil.copy(REPO / 'src/mm3_recomp_types.h', out / 'recomp_types.h')
    shutil.copy(REPO / 'tools/xboxrecomp/include/recomp_cpu.h', out / 'recomp_cpu.h')
    print(f'generated {len(list(out.glob("*.c")))} sources in {out}')


if __name__ == '__main__':
    main()
