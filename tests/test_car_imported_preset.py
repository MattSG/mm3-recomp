"""The optional visual preset must not fork tuning or require local assets."""
import copy
import importlib.util
import json
from pathlib import Path, PurePosixPath
import unittest

ROOT = Path(__file__).resolve().parents[1]
MODS = ROOT / "tools/car-mods"
SPEC = importlib.util.spec_from_file_location("imported_prepare", MODS / "prepare.py")
PREPARE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PREPARE)


class ImportedPresetTest(unittest.TestCase):
    def setUp(self):
        self.defaults = json.loads((MODS / "cars.json").read_text(encoding="utf-8"))
        self.imported = json.loads((MODS / "cars.imported.json").read_text(encoding="utf-8"))

    def test_only_model_selections_differ_from_tracked_defaults(self):
        stripped = copy.deepcopy(self.imported)
        for car in stripped["cars"]:
            car.pop("model_package", None)
        # Includes identity, order, paint, tuning, profile and both gauge units.
        self.assertEqual(stripped, self.defaults)

    def test_all_nine_paths_resolve_portably_and_removed_mini_is_absent(self):
        expected = {
            9004: "Panoz", 9005: "McLaren_F1", 9006: "Form1",
            9009: "Ford_GTcar", 9010: "Senna_Road", 9011: "Senna_GTR1",
            9012: "FXXK_Racer", 9013: "Regera_GT1", 9014: "AventadorX",
        }
        selected = {car["id"]: car for car in self.imported["cars"] if "model_package" in car}
        self.assertEqual(set(selected), set(expected))
        for car_id, alias in expected.items():
            car = selected[car_id]
            value = car["model_package"]
            self.assertEqual(car["alias"], alias)
            self.assertEqual(value, f"../../out/car-models/packages/{alias}")
            self.assertFalse(PurePosixPath(value).is_absolute())
            self.assertNotIn("\\", value)
            self.assertEqual((MODS / value).resolve(), ROOT / "out/car-models/packages" / alias)
        self.assertEqual(len(self.imported["cars"]), 9)
        self.assertNotIn(9001, {car["id"] for car in self.imported["cars"]})

    def test_configuration_validation_does_not_need_downloaded_models(self):
        # The installer validates asset existence separately when making a plan.
        self.assertEqual(PREPARE.load_config(MODS / "cars.imported.json"), self.imported)


if __name__ == "__main__":
    unittest.main()
