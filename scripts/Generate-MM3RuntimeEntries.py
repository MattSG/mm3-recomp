"""Lift runtime-proven MM3 entries through the repository's translator."""
import json
import os
import pathlib
import sys

repo = pathlib.Path(__file__).resolve().parent.parent
toolkit = pathlib.Path(os.environ.get('MM3_GENERATOR_DIR', repo / 'tools/xboxrecomp'))
sys.path.insert(0, str(toolkit))
from tools.recomp import config
from tools.recomp.translator import BatchTranslator
from tools.recomp.unwind_patch import unwind_after_call

out = pathlib.Path(sys.argv[1])
entries = json.loads((repo / 'mm3_runtime_entry_repairs.json').read_text())
tool = toolkit / 'tools'
xbe = repo / 'game_files/default.xbe'
config.configure_from_xbe(str(xbe))
translator = BatchTranslator(
    xbe_path=str(xbe), func_json_path=str(out / 'runtime_entry_functions.json'),
    labels_json_path=str(tool / 'disasm/output/labels.json'),
    identified_json_path=str(tool / 'func_id/output/identified_functions.json'),
    abi_json_path=str(tool / 'abi_analysis/output/abi_functions.json'),
    seh_prolog=0x00097AA4)
code = ['#define RECOMP_GENERATED_CODE', '#include "recomp_funcs.h"', '#include <math.h>']
for entry in entries:
    code.append(f'void sub_{int(entry["start"], 16):08X}(void);')
for entry in entries:
    body = translator.translate_single(int(entry['start'], 16))
    if not body:
        raise RuntimeError(f'Generation failed: {entry["start"]}')
    if entry.get('x87_helper_unwinds_caller'):
        # 9BC04 discards 9BC52's return address and returns from this dispatch
        # entry directly; the runtime's rewrite makes the caller return with it.
        try:
            body = unwind_after_call(body, 0x0009BC52)
        except ValueError:
            raise RuntimeError(f'Missing unwind helper: {entry["start"]}')
    code.append(body)
code += ['typedef void (*mm3_recovered_func_t)(void);',
         'mm3_recovered_func_t mm3_lookup_recovered_entry(uint32_t va)',
         '{', '    switch (va) {']
for entry in entries:
    address = int(entry['start'], 16)
    code.append(f'    case 0x{address:08X}u: return sub_{address:08X};')
code += ['    default: return (mm3_recovered_func_t)0;', '    }', '}']
(out / 'recomp_runtime_entries.c').write_text('\n'.join(code), encoding='utf-8')
