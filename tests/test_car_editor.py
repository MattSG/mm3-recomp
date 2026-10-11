"""Headless editor checks: validation, safe saves and installer integration."""

import copy
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("mm3_editor_test", ROOT / "tools/car-mods/editor.py")
editor = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(editor)


def form(car):
    return {
        "ratings": {key: str(car["ratings"][key]) for key in editor.RATING_ORDER},
        "physics": {key: str(car["physics"].get(key, "")) for key in editor.prepare.PHYSICS},
        "gears": ", ".join(str(value) for value in car["physics"].get("GearRatioVector", [])),
        "colors": car["colors"],
        "speedometer": {key: str(car.get("speedometer", {}).get(key, "")) for key in ("max_kmh", "max_mph")},
    }


class CarEditorTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="mm3-editor-test-")
        self.path = Path(self.temp.name) / "settings with spaces.json"
        self.path.write_bytes((ROOT / "tools/car-mods/cars.json").read_bytes())
        self.model = editor.EditorModel(self.path)

    def tearDown(self):
        self.temp.cleanup()

    def test_roundtrip_every_registered_car_and_field(self):
        for index, car in enumerate(self.model.config["cars"]):
            edited = editor.edited_car(car, **form(car))
            self.assertEqual(edited, car)
            self.model.replace_car(index, edited)
        self.assertEqual(self.model.config, self.model.saved)

    def test_form_edit_roundtrip_saves_physics_gears_ratings_and_paint(self):
        original = self.model.config["cars"][0]
        fields = form(original)
        fields["ratings"]["TOPSPEED"] = "8"
        fields["physics"]["TorqueScale"] = "500"
        fields["physics"]["WheelDrive"] = "2 - All wheels"
        fields["gears"] = "[-3.3, 2.6, 1.8, 1.3, 1.0, 0.8, 0.7]"
        fields["colors"] = ["#FF8800", "#123456"]
        if hasattr(editor.prepare, "SPEEDOMETER"):
            fields["speedometer"] = {"max_kmh": "650", "max_mph": "404"}
        edited = editor.edited_car(original, **fields)
        self.model.replace_car(0, edited)
        self.model.save()
        saved = editor.prepare.load_config(self.path)["cars"][0]
        self.assertEqual(saved["physics"]["TorqueScale"], 500)
        self.assertEqual(saved["physics"]["WheelDrive"], 2)
        self.assertEqual(saved["physics"]["GearRatioVector"], [-3.3, 2.6, 1.8, 1.3, 1.0, 0.8, 0.7])
        self.assertEqual(saved["colors"], ["#ff8800", "#123456"])
        self.assertEqual(saved["ratings"]["TOPSPEED"], 8)
        self.assertEqual(saved["id"], original["id"])
        self.assertEqual(self.model.config, self.model.saved)

    def test_blanks_remove_overrides_without_inventing_donor_defaults(self):
        car = self.model.config["cars"][0]
        fields = form(car)
        fields["physics"]["TorqueScale"] = ""
        fields["gears"] = ""
        fields["speedometer"] = {"max_kmh": "", "max_mph": ""}
        edited = editor.edited_car(car, **fields)
        self.assertNotIn("TorqueScale", edited["physics"])
        self.assertNotIn("GearRatioVector", edited["physics"])
        self.assertNotIn("speedometer", edited)

    def test_invalid_edit_does_not_change_model_or_disk(self):
        before = self.path.read_bytes()
        original = copy.deepcopy(self.model.config)
        car = copy.deepcopy(original["cars"][0])
        for invalid in ([0.5, 2.0, 1.0], [-3.3, 1.0, 2.0], [-3.3, float("nan"), 1.0]):
            car["physics"]["GearRatioVector"] = invalid
            with self.assertRaises(ValueError):
                self.model.replace_car(0, car)
            self.assertEqual(self.model.config, original)
        self.assertEqual(self.path.read_bytes(), before)

    def test_shared_validation_rejects_bad_save_before_atomic_write(self):
        self.model.config["cars"][0]["ratings"]["TOPSPEED"] = 10
        with patch.object(editor.prepare, "atomic_write") as writer:
            with self.assertRaises(ValueError):
                self.model.save()
            writer.assert_not_called()

    def test_external_change_requires_reload_and_is_preserved(self):
        external = copy.deepcopy(self.model.config)
        external["cars"][0]["colors"][0] = "#abcdef"
        self.path.write_text(json.dumps(external), encoding="utf-8")
        before = self.path.read_bytes()
        with self.assertRaisesRegex(ValueError, "outside the editor"):
            self.model.save()
        self.assertEqual(self.path.read_bytes(), before)
        self.model.reload()
        self.assertEqual(self.model.config, external)
        self.model.save()

    def test_form_rejects_nonfinite_fractional_ratings_and_half_range(self):
        for text in ("nan", "inf", "1.5", "word"):
            with self.assertRaises(ValueError):
                editor.number(text, "Rating", True)
        fields = form(self.model.config["cars"][0])
        fields["speedometer"] = {"max_kmh": "600", "max_mph": ""}
        with self.assertRaisesRegex(ValueError, "both speedometer"):
            editor.edited_car(self.model.config["cars"][0], **fields)

    def test_apply_uses_existing_installer_with_quoted_path_arguments(self):
        game = Path(self.temp.name) / "game with spaces"
        command = self.model.apply_command(game)
        self.assertEqual(command[1], str(ROOT / "tools/car-mods/prepare.py"))
        self.assertEqual(command[2:], ["--config", str(self.path), "--game-dir", str(game.resolve()), "--apply"])
        self.assertNotIn("--restore", command)

    def test_running_game_check_parses_process_rows(self):
        with patch.object(editor.sys, "platform", "win32"), patch.object(editor.subprocess, "run") as run:
            run.return_value = subprocess.CompletedProcess([], 0, '"mm3_recomp.exe","1234","Console","1","10 K"\n', "")
            self.assertTrue(editor.game_is_running())
            run.return_value = subprocess.CompletedProcess([], 0, "INFO: No tasks are running which match the specified criteria.\n", "")
            self.assertFalse(editor.game_is_running())
            self.assertFalse(run.call_args.kwargs.get("shell", False))


if __name__ == "__main__":
    unittest.main()
