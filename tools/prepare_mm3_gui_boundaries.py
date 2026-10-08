"""Seed native menu tail entries and repair proven GUI entry boundaries."""
import argparse
import json
import os
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', required=True)
args = parser.parse_args()
repo = Path(__file__).resolve().parents[1]
toolkit = Path(os.environ.get('MM3_GENERATOR_DIR', repo / 'tools/xboxrecomp'))
functions = json.loads((toolkit / 'tools/disasm/output/functions.json').read_text())
by_start = {int(item['start'], 16): item for item in functions}
for entry in json.loads((repo / 'mm3_tail_constructor_functions.json').read_text()):
    address = int(entry['start'], 16)
    if address in by_start:
        if int(by_start[address]['end'], 16) != int(entry['end'], 16):
            raise RuntimeError(f'Native constructor bounds changed for {address:08X}')
    else:
        functions.append(entry)
        by_start[address] = entry
# F7FCF is the store following F7FC8's allocation call, reached by fallthrough.
# Its apparent data pointer at 366EBC belongs to a numeric table, not a vtable.
# F8000 is similarly the store following F7FFB's call. Immediate/data matches
# alone do not make either address an independently callable native function.
# Keep the upstream coalescer's CFG, external-call and callback validation.
for address, method in ((0xF7FCF, 'tail_jump_alias'), (0xF8000, 'imm_ref_target')):
    item = by_start[address]
    if item['detection_method'] != method or item.get('called_by') or item.get('has_prologue'):
        raise RuntimeError(f'Entry evidence changed for {address:08X}; re-investigate')
    item['detection_method'] = 'mm3_verified_interior'
output = Path(args.output)
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(functions, indent=2) + '\n')
