"""Headless editor checks: validation, safe saves and installer integration."""

import copy
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import Mock, patch

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
    def test_default_local_manifest_preserves_models_stats_and_apply_path(self):
        folder = Path(self.temp.name)
        tracked, local = folder / "cars.json", folder / "cars.local.json"
        baseline = copy.deepcopy(self.model.config)
        tracked.write_text(json.dumps(baseline), encoding="utf-8")
        custom = copy.deepcopy(baseline)
        custom["cars"][0]["physics"]["TorqueScale"] = 777
        custom["cars"][1]["model_package"] = "prepared models/Panoz"
        local.write_text(json.dumps(custom), encoding="utf-8")
        selector = editor.prepare.default_config_path
        with patch.object(editor.prepare, "default_config_path", lambda: selector(folder)):
            args = editor.parse_args([])
        model = editor.EditorModel(args.config)
        self.assertEqual(model.config, custom)
        model.replace_car(0, editor.edited_car(custom["cars"][0], **form(custom["cars"][0])))
        model.save()
        model.reload()
        self.assertEqual(model.config, custom)
        self.assertEqual(editor.prepare.load_config(tracked), baseline)
        self.assertEqual(model.apply_command(folder / "game")[3], str(local.resolve()))

    def test_editor_explicit_manifest_wins_and_no_local_file_uses_defaults(self):
        folder = Path(self.temp.name)
        tracked, local = folder / "cars.json", folder / "cars.local.json"
        tracked.write_bytes(self.path.read_bytes())
        selector = editor.prepare.default_config_path
        with patch.object(editor.prepare, "default_config_path", lambda: selector(folder)):
            self.assertEqual(editor.parse_args([]).config, tracked)
            local.write_bytes(self.path.read_bytes())
            self.assertEqual(editor.parse_args([]).config, local)
            args = editor.parse_args(["--config", str(self.path), "--car-id", "9011", "--tab", "model"])
        self.assertEqual(args.config, self.path)
        self.assertEqual(args.car_id, 9011)
        self.assertEqual(args.tab, "model")

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
        self.assertEqual(saved["speedometer"], {"max_kmh": 650, "max_mph": 404})
        self.assertEqual(saved["id"], original["id"])
        self.assertEqual(self.model.config, self.model.saved)

    def test_original_speedometer_edit_save_and_removal_preserve_car_stats(self):
        cars = copy.deepcopy(self.model.config["cars"])
        limits = copy.deepcopy(self.model.config["original_speedometers"])
        limits["Koenigsegg"] = editor.edited_speedometer({"max_kmh": "1000", "max_mph": "621"})
        limits["Corvette"] = editor.edited_speedometer({"max_kmh": "650", "max_mph": "404"})
        self.model.replace_original_speedometers(limits)
        self.model.save()
        self.model.reload()
        self.assertEqual(self.model.config["original_speedometers"], limits)
        self.assertEqual(self.model.config["cars"], cars)
        self.assertIsNone(editor.edited_speedometer({"max_kmh": "", "max_mph": " "}))
        self.model.replace_original_speedometers({})
        self.model.save()
        self.assertNotIn("original_speedometers", editor.prepare.load_config(self.path))
        self.assertEqual(self.model.config["cars"], cars)

    def test_invalid_original_speedometer_edit_does_not_change_model_or_disk(self):
        before, baseline = copy.deepcopy(self.model.config), self.path.read_bytes()
        for settings in ({"Koenigsegg": {"max_kmh": 1201, "max_mph": 500}},
                         {"UnknownCar": {"max_kmh": 800, "max_mph": 500}}):
            with self.subTest(settings=settings), self.assertRaises(ValueError):
                self.model.replace_original_speedometers(settings)
            self.assertEqual(self.model.config, before)
            self.assertEqual(self.path.read_bytes(), baseline)

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

    def test_model_package_path_survives_stats_edit_save_and_reload(self):
        original = self.model.config["cars"][1]
        fields = form(original)
        folder = "models with spaces/panoz"
        car = editor.edited_car(original, **fields, model_package=folder)
        self.model.replace_car(1, car)
        self.model.save()
        self.model.reload()
        car = self.model.config["cars"][1]
        self.assertEqual(car["model_package"], folder)
        fields = form(car)
        fields["physics"]["TorqueScale"] = "700"
        edited = editor.edited_car(car, **fields)
        self.assertEqual(edited["model_package"], folder)
        self.model.replace_car(1, edited)
        self.model.save()
        self.model.reload()
        self.assertEqual(self.model.config["cars"][1]["model_package"], folder)
        cleared = editor.edited_car(self.model.config["cars"][1], **form(self.model.config["cars"][1]), model_package=" ")
        self.assertNotIn("model_package", cleared)

    def test_restore_default_driving_removes_factory_gears_and_preserves_saved_customization(self):
        car = copy.deepcopy(self.model.config["cars"][1])
        car["model_package"] = "prepared models/Panoz"
        car["colors"] = ["#123456"]
        car = editor.factory.apply_preset(car)
        car["ratings"]["TOPSPEED"] = 3
        car["speedometer"] = {"max_kmh": 600, "max_mph": 400}
        self.model.replace_car(1, car)
        self.model.restore_default_driving(1)
        restored = self.model.config["cars"][1]
        self.assertEqual(restored["model_package"], car["model_package"])
        self.assertEqual(restored["colors"], car["colors"])
        self.assertEqual(restored["id"], car["id"])
        self.assertEqual(restored["ratings"], car["ratings"])
        self.assertEqual(restored["speedometer"], car["speedometer"])
        self.assertNotIn("engine_profile", restored)
        self.assertEqual(restored["physics"], {"Mass": 980, "TorqueScale": 750})
        self.model.save()
        self.model.reload()
        self.assertEqual(self.model.config["cars"][1], restored)

    def test_factory_engine_form_switch_is_explicit_and_preserved(self):
        car = editor.factory.apply_preset(self.model.config["cars"][0])
        self.assertEqual(editor.edited_car(car, **form(car))["engine_profile"], "factory")
        disabled = editor.edited_car(car, **form(car), factory_engine=False)
        self.assertNotIn("engine_profile", disabled)
        enabled = editor.edited_car(disabled, **form(disabled), factory_engine=True)
        self.assertEqual(enabled, car)

    def test_restore_button_recovers_invalid_tuning_and_keeps_draft_art(self):
        app = editor.CarEditor.__new__(editor.CarEditor)
        app.model, app.current = self.model, 0
        app.model.config["cars"][0]["physics"]["TorqueScale"] = float("nan")
        app.color_vars = [Mock(get=Mock(return_value="#ABCDEF"))]
        app.model_var = Mock(get=Mock(return_value="models with spaces/Mini"))
        app.rating_vars = {key: Mock(get=Mock(return_value="3")) for key in editor.RATING_ORDER}
        app.speed_vars = {"max_kmh": Mock(get=Mock(return_value="600")),
                          "max_mph": Mock(get=Mock(return_value="400"))}
        app.status, app._load_car, app._error = Mock(), Mock(), Mock()
        app._collect = Mock(side_effect=ValueError("Invalid in-progress gear edit"))
        app.restore_default_driving()
        app._collect.assert_not_called()
        app._error.assert_not_called()
        restored = app.model.config["cars"][0]
        self.assertEqual(restored["physics"], {"Mass": 1145, "TorqueScale": 380})
        self.assertEqual(restored["colors"], ["#abcdef"])
        self.assertEqual(restored["model_package"], "models with spaces/Mini")
        self.assertEqual(restored["ratings"], {key: 3 for key in editor.RATING_ORDER})
        self.assertEqual(restored["speedometer"], {"max_kmh": 600, "max_mph": 400})
        app._load_car.assert_called_once_with(0)

    def test_default_reset_replaces_single_speed_and_high_rpm_profiles(self):
        for index, car in enumerate(self.model.config["cars"]):
            realistic = editor.factory.apply_preset(car)
            restored = editor.defaults.apply_preset(realistic)
            self.assertEqual(restored["physics"], car["physics"])
            self.assertEqual(set(restored["physics"]), {"Mass", "TorqueScale"})
            self.assertNotIn("engine_profile", restored)
            self.model.replace_car(index, restored)
        self.assertEqual(editor.factory.factory_physics_settings(self.model.config).count(b"\n"), 1)

    def test_default_reset_does_not_read_previously_saved_custom_tuning_as_a_preset(self):
        self.model.config["cars"][0]["physics"] = {"Mass": 1800, "TorqueScale": 1900}
        self.model.save()
        # On a fresh checkout, Workshop can save directly to cars.json. Its
        # edits must not become the target of Restore default driving.
        folder = Path(self.temp.name)
        (folder / "cars.json").write_text(json.dumps(self.model.config), encoding="utf-8")
        with patch.object(editor, "HERE", folder):
            self.model.restore_default_driving(0)
        self.assertEqual(self.model.config["cars"][0]["physics"], {"Mass": 1145, "TorqueScale": 380})

    def test_specifications_tab_can_be_selected_on_launch(self):
        self.assertEqual(editor.parse_args([]).tab, "specs")
        self.assertEqual(editor.parse_args(["--car-id", "9006", "--tab", "specs"]).car_id, 9006)

    def test_five_new_identities_roundtrip_without_changing_existing_user_tuning(self):
        config = copy.deepcopy(self.model.config)
        config["cars"] = [car for car in config["cars"] if car["id"] <= 9009]
        config["cars"][0]["physics"]["TorqueScale"] = 777
        config["cars"][0]["speedometer"] = {"max_kmh": 300, "max_mph": 311}
        before = copy.deepcopy(config["cars"])
        template = next(car for car in config["cars"] if car["id"] == 9005)
        for ident in range(9010, 9015):
            alias, donor, name = editor.prepare.IDENTITIES[ident]
            car = copy.deepcopy(template)
            car.update(id=ident, alias=alias, donor=donor, display_name=name)
            # This regression exercises adding legacy donor-based entries,
            # not cloning another car's explicitly selected factory curve.
            car.pop("engine_profile", None)
            car["model_package"] = f"models/{alias}"
            config["cars"].append(car)
        self.path.write_text(json.dumps(config))
        self.model.reload()
        for index in range(len(before), len(config["cars"])):
            car = self.model.config["cars"][index]
            self.model.replace_car(index, editor.edited_car(car, **form(car)))
        self.model.save()
        self.model.reload()
        self.assertEqual(self.model.config["cars"][:len(before)], before)
        self.assertEqual(len(self.model.config["cars"]), 10)
        for ident, car in zip(range(9010, 9015), self.model.config["cars"][len(before):]):
            self.assertEqual(car["id"], ident)
            self.assertEqual(car["model_package"], f"models/{editor.prepare.IDENTITIES[ident][0]}")

    def test_running_game_check_parses_process_rows(self):
        with patch.object(editor.sys, "platform", "win32"), patch.object(editor.subprocess, "run") as run:
            run.return_value = subprocess.CompletedProcess([], 0, '"mm3_recomp.exe","1234","Console","1","10 K"\n', "")
            self.assertTrue(editor.game_is_running())
            run.return_value = subprocess.CompletedProcess([], 0, "INFO: No tasks are running which match the specified criteria.\n", "")
            self.assertFalse(editor.game_is_running())
            self.assertFalse(run.call_args.kwargs.get("shell", False))


if __name__ == "__main__":
    unittest.main()
