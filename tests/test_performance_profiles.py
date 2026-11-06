"""Native profiles and Python tools share data, not independent defaults."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("performance_profiles", ROOT / "scripts/performance_profiles.py")
profiles = importlib.util.module_from_spec(spec)
spec.loader.exec_module(profiles)


class PerformanceProfilesTest(unittest.TestCase):
    def test_native_rows_keep_existing_tool_defaults(self):
        values = profiles.load_profiles()
        self.assertEqual(set(values), {"Balanced", "Quality", "LowPower"})
        self.assertEqual(values["Balanced"], dict(render_scale="2.0", msaa="4", draw_distance="2", aniso="16", fps_limit="60"))
        self.assertEqual(values["Quality"]["render_scale"], "3.0")
        self.assertEqual(values["LowPower"]["fps_limit"], "30")

    def test_malformed_or_duplicate_rows_are_rejected(self):
        row = 'MM3_PROFILE("Balanced", "2.0", "4", "2", "16", "60")\n'
        for text in ("arbitrary syntax", 'MM3_PROFILE("Balanced", 2, "4", "2", "16", "60")', row + row):
            with self.subTest(text=text), tempfile.TemporaryDirectory() as temporary:
                path = Path(temporary) / "profiles.def"
                path.write_text(text)
                with self.assertRaises(ValueError):
                    profiles.load_profiles(path)
