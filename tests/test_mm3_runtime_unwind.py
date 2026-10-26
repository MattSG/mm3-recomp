import unittest

from scripts.mm3_runtime_unwind import unwind_after_call


class UnwindAfterCallTest(unittest.TestCase):
    def test_wraps_the_matching_call_and_keeps_the_rest(self):
        body = "a();\nRECOMP_ABI_CALL(0x0009BC52u, sub_0009BC52);\nb();"
        out = unwind_after_call(body, 0x9BC52)
        self.assertIn("uint32_t math_sp = esp;", out)
        self.assertIn("if (esp == math_sp + 4u) return; }", out)
        self.assertTrue(out.startswith("a();"))
        self.assertTrue(out.endswith("b();"))

    def test_missing_call_is_an_error(self):
        with self.assertRaises(ValueError):
            unwind_after_call("a();", 0x9BC52)


if __name__ == "__main__":
    unittest.main()
