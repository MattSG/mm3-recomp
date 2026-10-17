"""fix_flag_joins: rewrites je/jne reads of the never-set `_flags` fallback.

Imports the real pass from scripts/generate_mm3.py and runs it on a synthetic
generated file. No generator checkout, XBE or disassembly is needed.

    python tests/mm3_flag_join_contract.py
"""
import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

spec = importlib.util.spec_from_file_location('generate_mm3', ROOT / 'scripts/generate_mm3.py')
generate_mm3 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generate_mm3)

DECL = '    int _flags = 0; /* fallback flag var */'

# sub_00001000: a sub result reaches a je through a fall-through join.
# sub_00002000: a call (flags undefined afterwards) reaches a jne join.
# sub_00003000: a je whose target label is not in the function.
SOURCE = f'''void sub_00001000(void)
{{
{DECL}
    _fa = (uint32_t)(eax) & 0xFFFFFFFFu; _fas = (int32_t)(_fa); /* sub result */
    if (_cf) goto loc_00001010;

loc_00001008: ;
    if (_flags /* je: equal / zero */) goto loc_00001020;
    eax = 1;
loc_00001010: ;
    eax = 2;
loc_00001020: ;
    return;
}}

void sub_00002000(void)
{{
{DECL}
    sub_00003000();
loc_00002008: ;
    if (_flags /* jne: not equal / not zero */) goto loc_00002020;
    eax = 3;
loc_00002020: ;
    return;
}}

void sub_00003000(void)
{{
{DECL}
    _fa = (uint32_t)(eax) & 0xFFFFFFFFu; _fb = (uint32_t)(ecx) & 0xFFFFFFFFu; /* cmp eax, ecx */
    _fas = (int32_t)(_fa); _fbs = (int32_t)(_fb); /* cmp eax, ecx (32-bit) */
loc_00003008: ;
    if (_flags /* je: equal / zero */) goto loc_00003020;
    if (_flags /* jne: not equal / not zero */) (void)0; /* goto loc_00003999 - dead code, label not in function */
    eax = 4;
loc_00003020: ;
    return;
}}
'''


class FlagJoinContract(unittest.TestCase):
    def run_pass(self, text):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'recomp_0000.c'
            path.write_text(text)
            fixed, unfixed = generate_mm3.fix_flag_joins(Path(tmp))
            return fixed, unfixed, path.read_text()

    def test_rewrites_fixable_join_and_reports_the_rest(self):
        fixed, unfixed, out = self.run_pass(SOURCE)
        # Both fixable joins are materialized: each producer stores ZF and its branch reads it.
        self.assertEqual(fixed, 2)
        self.assertIn('    if (_zf) goto loc_00003020;', out)
        self.assertIn('_fas = (int32_t)(_fa); /* sub result */\n    _zf = (_fa == 0);\n', out)
        self.assertIn('    if (_zf) goto loc_00001020;', out)
        self.assertIn(f'{DECL} int _zf = 0;', out)
        self.assertNotIn('if (_flags /* je: equal / zero */) goto loc_00001020;', out)

        # A call before the jne join leaves the flags undefined: the branch stays on _flags.
        self.assertIn('    if (_flags /* jne: not equal / not zero */) goto loc_00002020;', out)
        reasons = {(f, label, line): reason for f, _, label, line, reason in unfixed}
        call_line = SOURCE.splitlines().index('    sub_00003000();') + 1
        self.assertIn(('recomp_0000.c', 'loc_00002008', call_line + 2), reasons)
        blocked = reasons[('recomp_0000.c', 'loc_00002008', call_line + 2)]
        self.assertIn(f'blocked at recomp_0000.c:{call_line}: sub_00003000();', blocked)

        # The jump whose label is outside the function is reported, not silently kept.
        self.assertTrue(any('outside this function' in r for r in reasons.values()))
        self.assertEqual(len(unfixed), 2)
        self.assertEqual(sorted({f for f, *_ in unfixed}), ['recomp_0000.c'])

    def test_reports_function_names(self):
        _, unfixed, _ = self.run_pass(SOURCE)
        funcs = sorted({func for _, func, *_ in unfixed})
        self.assertEqual(funcs, ['sub_00002000', 'sub_00003000'])

    def test_second_run_is_stable(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'recomp_0000.c'
            path.write_text(SOURCE)
            generate_mm3.fix_flag_joins(Path(tmp))
            once = path.read_text()
            generate_mm3.fix_flag_joins(Path(tmp))
            self.assertEqual(path.read_text(), once)


if __name__ == '__main__':
    unittest.main(verbosity=2)
