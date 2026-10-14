"""Reference presets must remain opt-in, honest about unknowns, and reversible."""
import copy
import importlib.util
import json
import math
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("factory_prepare", ROOT / "tools/car-mods/prepare.py")
PREPARE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PREPARE)
FACTORY = PREPARE.factory_specs


class FactorySpecsTest(unittest.TestCase):
    def setUp(self):
        self.config = json.loads((ROOT / "tools/car-mods/cars.json").read_text())

    def validate(self, config):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "cars.json"
            path.write_text(json.dumps(config), encoding="utf-8")
            return PREPARE.load_config(path)

    def test_apply_preserves_models_paints_identity_original_gauges_and_input(self):
        self.config["cars"][0]["model_package"] = "my models/Panoz"
        self.config["cars"][0]["colors"] = ["#012345"]
        before = copy.deepcopy(self.config)
        result = FACTORY.apply_presets(self.config)
        self.assertEqual(before, self.config)
        self.assertEqual(result["original_speedometers"], before["original_speedometers"])
        for old, new in zip(before["cars"], result["cars"]):
            for key in set(old) - {"physics", "ratings", "speedometer", "engine_profile"}:
                self.assertEqual(old[key], new[key])
            self.assertEqual(new["engine_profile"], "factory")
        result["cars"][0]["colors"][0] = "#ffffff"
        result["cars"][0]["physics"]["TorqueScale"] = 1
        self.assertEqual(before, self.config)
        self.assertNotEqual(FACTORY.load_specs()["Panoz"]["physics"]["TorqueScale"], 1)

    def test_removed_slots_are_not_resurrected_and_unknowns_fail_clearly(self):
        self.config["cars"] = self.config["cars"][:1]
        self.assertEqual(len(FACTORY.apply_presets(self.config)["cars"]), 1)
        with self.assertRaisesRegex(ValueError, "No factory reference"):
            FACTORY.apply_preset({"alias": "RemovedCar"})

    def test_all_nine_presets_validate_and_serialize_native_gear_counts(self):
        config = FACTORY.apply_presets(self.config)
        self.assertEqual(self.validate(config), config)
        self.assertEqual(set(FACTORY.load_specs()), {car["alias"] for car in config["cars"]})
        counts = set()
        template = b"Begin(CarInfo)\nEnd(CarInfo)\nBegin(CarBody)\nEnd(CarBody)\nBegin(CarCamera)\nEnd(CarCamera)\n"
        for car in config["cars"]:
            count = len(car["physics"]["GearRatioVector"]) - 1
            counts.add(count)
            text = PREPARE.tweak_variant(template, car).decode()
            self.assertIn(f"NGears\t\t{count}\r\n", text)
            self.assertLess(text.index("NGears"), text.index("GearRatioVector"))
            self.assertIn("TurboSize\t\t0\r\n", text)
            self.assertNotIn("engine_profile", text)
        self.assertEqual(counts, {1, 6, 7})

    def test_legacy_alias_does_not_enable_factory_torque_or_high_rpm(self):
        for car in self.config["cars"]:
            car.pop("engine_profile", None)
        self.assertEqual(FACTORY.factory_physics_settings(self.config).count(b"\n"), 1)
        car = next(c for c in self.config["cars"] if c["alias"] == "Form1")
        car["physics"].update(FACTORY.load_specs()["Form1"]["physics"])
        with self.assertRaisesRegex(ValueError, "stock torque curve"):
            self.validate(self.config)
        car["engine_profile"] = "factory"
        self.validate(self.config)
        car["engine_profile"] = True
        with self.assertRaisesRegex(ValueError, "engine_profile"):
            self.validate(self.config)

    def test_sidecar_contains_only_explicit_profiles_and_bounded_normalized_curves(self):
        config = FACTORY.apply_presets(self.config)
        del config["cars"][0]["engine_profile"]
        lines = FACTORY.factory_physics_settings(config).decode().splitlines()[1:]
        self.assertEqual(len(lines), 8)
        self.assertNotIn("Panoz", {line.split()[0] for line in lines})
        for line in lines:
            alias, shift, radius, count, *points = line.split()
            count = int(count)
            self.assertEqual(len(points), count * 2)
            self.assertGreater(float(shift), 0)
            self.assertTrue(.1 <= float(radius) <= 2)
            curve = list(zip(map(float, points[::2]), map(float, points[1::2])))
            self.assertEqual(curve[0][0], 0)
            self.assertEqual(max(p[1] for p in curve), 1)
            self.assertTrue(all(a[0] < b[0] for a, b in zip(curve, curve[1:])))
            self.assertLessEqual(count, 16)

    def test_verified_power_matches_curve_without_promoting_estimates_to_facts(self):
        specs = FACTORY.load_specs()
        for alias in ("McLaren_F1", "Ford_GTcar", "Senna_Road", "Senna_GTR1", "AventadorX"):
            entry = specs[alias]
            fact, cal = entry["specification"], entry["calibration"]
            rpm = fact["power_rpm"]
            for a, b in zip(cal["torque_curve"], cal["torque_curve"][1:]):
                if a[0] <= rpm <= b[0]:
                    fraction = a[1] + (b[1] - a[1]) * (rpm - a[0]) / (b[0] - a[0])
                    power = cal["peak_torque_nm"] * fraction * rpm * 2 * math.pi / 60000
                    self.assertAlmostEqual(power / fact["power_kw"], 1, delta=.025, msg=alias)
                    break
            else:
                self.fail(f"Published power RPM missing from {alias}'s curve")
        for alias in ("Panoz", "Form1", "FXXK_Racer", "Regera_GT1", "Senna_GTR1"):
            self.assertIsNone(specs[alias]["specification"]["top_speed_kmh"])
        self.assertIsNone(specs["McLaren_F1"]["specification"]["torque_nm"])
        self.assertIsNone(specs["FXXK_Racer"]["specification"]["mass_kg"])
        self.assertTrue(all(entry["notes"] and entry["sources"] for entry in specs.values()))

    def test_curve_interpolation_never_exceeds_verified_rated_power(self):
        # Between two torque samples, power is quadratic in RPM. Test the
        # interior extremum as well as endpoints: checking samples alone misses
        # a spurious power bump above the manufacturer's published rating.
        for alias, entry in FACTORY.load_specs().items():
            limit = entry["specification"]["power_kw"]
            if limit is None:
                continue  # No invented hard cap for unpublished race output.
            cal = entry["calibration"]
            peak = 0
            for (r0, t0), (r1, t1) in zip(cal["torque_curve"], cal["torque_curve"][1:]):
                slope = (t1 - t0) / (r1 - r0)
                intercept = t0 - slope * r0
                candidates = [r0, r1]
                if slope < 0 and r0 < -intercept / (2 * slope) < r1:
                    candidates.append(-intercept / (2 * slope))
                peak = max(peak, *(rpm * (slope * rpm + intercept)
                    * cal["peak_torque_nm"] * 2 * math.pi / 60000 for rpm in candidates))
            self.assertLessEqual(peak, limit, alias)


if __name__ == "__main__":
    unittest.main()
