"""mark_dead_branches: the translator's dead-branch marker becomes RECOMP_DEAD_BRANCH.

Imports the real pass from scripts/generate_mm3.py and runs it on a synthetic
generated file, then checks the project header and definition agree with it.
No generator checkout, XBE or disassembly is needed.

    python tests/mm3_dead_branch_contract.py
"""
import importlib.util
import re
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

spec = importlib.util.spec_from_file_location('generate_mm3', ROOT / 'scripts/generate_mm3.py')
generate_mm3 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generate_mm3)

# The translator's exact output for a conditional goto whose label is not in
# the function (tools/xboxrecomp/tools/recomp/translator.py, the lone-label check).
SOURCE = '''void sub_00001000(void)
{
    _fa = (uint32_t)(eax) & 0xFFFFFFFFu; _fas = (int32_t)(_fa); /* or result */
    if (_flags /* jo: overflow */) (void)0; /* goto loc_0009398F - dead code, label not in function */
    if ((_fas < 0)) (void)0; /* goto loc_000D7B7E - dead code, label not in function */ /* js: sign (negative) */
    if (0) (void)0; /* goto loc_000D7B7E - dead code, label not in function */
    if (_cf /* jb: below (unsigned <) */) goto loc_00001010;
loc_00001010: ;
    return;
}
'''

EXPECTED = '''void sub_00001000(void)
{
    _fa = (uint32_t)(eax) & 0xFFFFFFFFu; _fas = (int32_t)(_fa); /* or result */
    if (_flags /* jo: overflow */) RECOMP_DEAD_BRANCH(0x0009398Fu); /* goto loc_0009398F - label not in function */
    if ((_fas < 0)) RECOMP_DEAD_BRANCH(0x000D7B7Eu); /* goto loc_000D7B7E - label not in function */ /* js: sign (negative) */
    if (0) RECOMP_DEAD_BRANCH(0x000D7B7Eu); /* goto loc_000D7B7E - label not in function */
    if (_cf /* jb: below (unsigned <) */) goto loc_00001010;
loc_00001010: ;
    return;
}
'''


class DeadBranchContract(unittest.TestCase):
    def run_pass(self, text):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'recomp_0000.c'
            path.write_text(text)
            marked = generate_mm3.mark_dead_branches(Path(tmp))
            return marked, path.read_text()

    def test_rewrites_marker_and_keeps_the_rest(self):
        marked, out = self.run_pass(SOURCE)
        self.assertEqual(marked, 3)
        self.assertEqual(out, EXPECTED)

    def test_second_run_is_stable(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'recomp_0000.c'
            path.write_text(SOURCE)
            generate_mm3.mark_dead_branches(Path(tmp))
            once = path.read_text()
            self.assertEqual(generate_mm3.mark_dead_branches(Path(tmp)), 0)
            self.assertEqual(path.read_text(), once)

    def test_header_defines_macro_once_and_definition_exists(self):
        header = (ROOT / 'src/mm3_recomp_types.h').read_text()
        self.assertEqual(len(re.findall(r'#define RECOMP_DEAD_BRANCH\(', header)), 1)
        self.assertIn('int mm3_dead_branch_hit(uint32_t target);', header)
        stubs = (ROOT / 'src/mm3_unresolved_stubs.c').read_text()
        self.assertEqual(len(re.findall(r'^int mm3_dead_branch_hit\(uint32_t target\)', stubs, re.M)), 1)


if __name__ == '__main__':
    unittest.main(verbosity=2)
