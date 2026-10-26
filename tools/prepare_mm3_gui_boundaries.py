"""Seed native menu tail entries not detected by the general analysis."""
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
output = Path(args.output)
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(functions, indent=2) + '\n')
