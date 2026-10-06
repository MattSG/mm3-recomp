"""Generate original PAL ground-placement/reset routines for the offline fixture."""
import argparse
import hashlib
import sys
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--xbe', type=Path, required=True)
parser.add_argument('--functions', type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parent.parent
xbe = args.xbe.resolve()
expected = '2b04b66c43e7f37bbcebbfb5b72ccb96a2aa99cc2c60c53eb7d3530bcc2a3d79'
if hashlib.sha256(xbe.read_bytes()).hexdigest() != expected:
    parser.error('The native fixture requires the verified PAL default.xbe.')
output = root / 'conformance_tmp'
output.mkdir(exist_ok=True)
addresses = ['001CDDA3', '001CD866', '001D65BC', '00012BFA',
             '0001AAB9', '0001AD31', '00019115', '0001944A',
             '0021ADF3', '0021848C', '001540C5', '001D578C',
             '00013A34', '00013A63', '000197E1', '000198B4']
sys.path.insert(0, str(root / 'tools/xboxrecomp'))
import json
from tools.recomp.translator import FunctionTranslator
from tools.recomp import config
from tools.rtti.rtti import Image
functions = {int(f['start'], 16): f for f in json.loads(args.functions.read_text())}
for function in functions.values():
    function['end'] = int(function['end'], 16)
config.configure_from_xbe(str(xbe))
translator = FunctionTranslator(xbe.read_bytes(), functions, seh_prolog=0x00097AA4)
# Include direct math/reset dependencies, but fixture the service registry.
for address in addresses:
    for target in functions[int(address, 16)].get('calls_to', []):
        value = int(target, 16)
        name = f'{value:08X}'
        if value in functions and name not in addresses and name not in ('001C032D', '001C0107'):
            addresses.append(name)
parts = ['''#define RECOMP_GENERATED_CODE
#include "recomp_funcs.h"
#include <math.h>
extern void mm3_test_icall(uint32_t);
#undef RECOMP_ICALL_SAFE_AT
#define RECOMP_ICALL_SAFE_AT(va, saved, site) mm3_test_icall(va)
''']
for address in addresses:
    code = translator.translate_function(int(address, 16), functions[int(address, 16)])
    if not code or 'sub_' + address not in code:
        raise RuntimeError('Missing original function ' + address + ': ' + repr(code)[:1000])
    parts.append(code)
    print('Generated', address, flush=True)
(output / 'native_teleport_functions.c').write_text('\n'.join(parts))
image = Image(str(xbe))
memory = bytearray(8 * 1024 * 1024)
for va, raw, length, name in image.secs:
    if name == '.rdata':
        memory[va:va+length] = image.d[raw:raw+length]
(output / 'native_teleport_memory.bin').write_bytes(memory)
