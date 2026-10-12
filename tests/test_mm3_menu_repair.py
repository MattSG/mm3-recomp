"""Selection-art repair keeps model assets intact and rolls package writes back."""
from contextlib import contextmanager, ExitStack
import copy
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/car-models"))
import repair_menu as repair
import build_package as builder


def file_bytes(folder):
    return {path.relative_to(folder).as_posix(): path.read_bytes()
            for path in folder.rglob("*") if path.is_file()}


def package_fixture(root, *, existing_background=False):
    folder = root / "Panoz"
    folder.mkdir()
    source_dir = root / "source"
    source_dir.mkdir()
    selected = ["BODY_H", "BREAK7_H", "FNDR1_H", "TRIM_H"]
    names = selected + list(repair.mm3_model.WHEEL_NODES.values())
    source = dict(materials=[], meshes=[dict(name=name) for name in
                  names + ["SHADOW_H", "DRIVER_H", "UNSELECTED_H"]])
    source_path = source_dir / "model.json"
    source_path.write_text(json.dumps(source), encoding="utf-8")
    entries = [
        ("loose", "Data/Shared/Cars/Panoz/body.cmp", b"near-model-original"),
        ("loose", "Data/Shared/Cars/Panoz_lod1/body.cmp", b"far-model-original"),
        ("Data_hd.zip", "Cars/Panoz/materialSet00.omb", b"material-original"),
        ("Data_dvd.zip", "Cars/Panoz/imported_00.cdds", b"body-texture-original"),
        ("Data_dvd.zip", "Menu/Gfx/Vehicles/Panoz.cdds", b"menu-original"),
        ("Data_dvd.zip", "Menu/Gfx/Vehicles/Panoz_Small.cdds", b"small-original"),
    ]
    if existing_background:
        entries.append(("loose", "Data/Shared/Cars/Panoz/bg.cdds", b"background-original"))
    files = []
    for archive, target, data in entries:
        relative = "assets/" + archive.replace(".zip", "") + "/" + target
        path = folder / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        files.append(dict(archive=archive, path=target, source=relative))
    package = dict(format="mm3-car-model-v1", alias="Panoz", car_id=9004, files=files)
    report = dict(source=str(source_path), body_names=selected, car="Panoz GTR-1",
                  models={"near": {"unchanged": True}, "_lod1": {"unchanged": True}})
    (folder / "package.json").write_text(json.dumps(package), encoding="utf-8")
    (folder / "conversion-report.json").write_text(json.dumps(report), encoding="utf-8")
    return folder, source_dir, names


@contextmanager
def cheap_renderers():
    """Exercise package transactions without rasterizing a downloaded car."""
    with ExitStack() as stack:
        stack.enter_context(patch.object(repair.preview, "textures", return_value={}))
        icons = stack.enter_context(patch.object(repair.preview, "render_menu_icons", return_value={
            "Menu": Image.new("RGBA", (128, 64)), "Small": Image.new("RGBA", (64, 64))}))
        background = stack.enter_context(patch.object(repair.preview, "render_selection_background",
            return_value=Image.new("RGB", (640, 480))))
        stack.enter_context(patch.object(repair.mm3_textures, "image_to_menu_cdds",
            side_effect=lambda path: b"new-" + path.stem.encode("ascii")))
        stack.enter_context(patch.object(repair.mm3_textures, "image_to_background_cdds",
            return_value=b"new-Background"))
        stack.enter_context(patch("sys.stdout", new=io.StringIO()))
        yield icons, background


class MenuRepairTest(unittest.TestCase):
    def test_repair_changes_only_three_ui_assets_and_uses_selected_meshes(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            folder, source_dir, names = package_fixture(root)
            before, source_before = file_bytes(folder), file_bytes(source_dir)
            with cheap_renderers() as (icons, background):
                updates = repair.prepare(folder, root / "previews")
                self.assertEqual(file_bytes(folder), before)  # Planning is read-only.
                for renderer in (icons, background):
                    rendered = renderer.call_args.args[0]
                    self.assertEqual([mesh["name"] for mesh in rendered], names)
                self.assertEqual(len(updates), 5)  # Three UI assets, manifest and report.
                asset_updates = [(path, old, new) for path, old, new in updates
                                 if path.suffix == ".cdds"]
                self.assertEqual(len(asset_updates), 3)
                self.assertEqual({path.name for path, _, _ in asset_updates},
                                 {"Panoz.cdds", "Panoz_Small.cdds", "bg.cdds"})
                backup = root / "backup"
                repair.install([(folder, updates)], backup)
            after = file_bytes(folder)
            changed = {name for name in before if before[name] != after[name]}
            self.assertEqual(changed, {"package.json", "conversion-report.json",
                "assets/Data_dvd/Menu/Gfx/Vehicles/Panoz.cdds",
                "assets/Data_dvd/Menu/Gfx/Vehicles/Panoz_Small.cdds"})
            new_background = "assets/loose/Data/Shared/Cars/Panoz/bg.cdds"
            self.assertEqual(set(after) - set(before), {new_background})
            self.assertEqual(after[new_background], b"new-Background")
            self.assertEqual(file_bytes(source_dir), source_before)
            package = json.loads((folder / "package.json").read_text())
            self.assertEqual(len(package["files"]), 7)
            self.assertEqual(sum(item["path"].endswith("/bg.cdds") for item in package["files"]), 1)
            report = json.loads((folder / "conversion-report.json").read_text())
            self.assertEqual(report["menu_artwork"]["meshes"], sorted(names))
            self.assertEqual(report["models"], {"near": {"unchanged": True}, "_lod1": {"unchanged": True}})
            for name, blob in before.items():
                backup_path = backup / folder.name / name
                if name in changed:
                    self.assertEqual(backup_path.read_bytes(), blob)
                else:
                    self.assertFalse(backup_path.exists())
            self.assertEqual(json.loads((backup / folder.name / "created-files.json").read_text()),
                             [new_background])

    def test_existing_background_is_backed_up_without_duplicate_manifest_entry(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            folder, _, _ = package_fixture(root, existing_background=True)
            before = file_bytes(folder)
            with cheap_renderers():
                updates = repair.prepare(folder, root / "previews")
                repair.install([(folder, updates)], root / "backup")
            relative = "assets/loose/Data/Shared/Cars/Panoz/bg.cdds"
            self.assertEqual((root / "backup" / folder.name / relative).read_bytes(), before[relative])
            package = json.loads((folder / "package.json").read_text())
            self.assertEqual(len(package["files"]), 7)
            self.assertEqual(sum(item["path"].endswith("/bg.cdds") for item in package["files"]), 1)
            self.assertEqual(json.loads((root / "backup" / folder.name / "created-files.json").read_text()), [])

    def test_legacy_report_without_body_names_uses_standard_complete_exterior(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            folder, _, names = package_fixture(root)
            report_path = folder / "conversion-report.json"
            report = json.loads(report_path.read_text())
            del report["body_names"]
            report_path.write_text(json.dumps(report))
            with cheap_renderers() as (icons, background):
                repair.prepare(folder, root / "previews")
                for renderer in (icons, background):
                    self.assertEqual([mesh["name"] for mesh in renderer.call_args.args[0]],
                                     [name for name in names if name != "TRIM_H"])

    def test_icons_only_preserves_background_bytes_and_skips_background_render(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            folder, _, _ = package_fixture(root, existing_background=True)
            backdrop = folder / "assets/loose/Data/Shared/Cars/Panoz/bg.cdds"
            original = backdrop.read_bytes()
            with cheap_renderers() as (_, background):
                updates = repair.prepare(folder, root / "previews", icons_only=True)
                background.assert_not_called()
                self.assertEqual({path.name for path, _, _ in updates if path.suffix == ".cdds"},
                                 {"Panoz.cdds", "Panoz_Small.cdds"})
                repair.install([(folder, updates)], root / "backup")
            self.assertEqual(backdrop.read_bytes(), original)
            self.assertFalse((root / "previews/Background.png").exists())

    def test_failure_restores_original_bytes_and_removes_new_background(self):
        for existing in (False, True):
            with self.subTest(existing_background=existing), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                folder, source_dir, _ = package_fixture(root, existing_background=existing)
                before, source_before = file_bytes(folder), file_bytes(source_dir)
                with cheap_renderers():
                    updates = repair.prepare(folder, root / "previews")
                    original_write = repair.atomic_write
                    failed = False

                    def fail_final_write(path, data):
                        nonlocal failed
                        if path.name == "conversion-report.json" and not failed:
                            failed = True
                            raise OSError("Simulated final write failure")
                        original_write(path, data)

                    with patch.object(repair, "atomic_write", side_effect=fail_final_write):
                        with self.assertRaisesRegex(OSError, "Simulated final write"):
                            repair.install([(folder, updates)], root / "backup")
                self.assertEqual(file_bytes(folder), before)
                self.assertEqual(file_bytes(source_dir), source_before)
                self.assertTrue((root / "backup" / folder.name / "package.json").is_file())

    def test_missing_selected_mesh_rejects_package_before_install(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            folder, source_dir, _ = package_fixture(root)
            source_path = source_dir / "model.json"
            source = json.loads(source_path.read_text())
            source["meshes"] = [mesh for mesh in source["meshes"] if mesh["name"] != "BREAK7_H"]
            source_path.write_text(json.dumps(source))
            before = file_bytes(folder)
            with cheap_renderers():
                with self.assertRaisesRegex(ValueError, "missing selected exterior meshes"):
                    repair.prepare(folder, root / "previews")
            self.assertEqual(file_bytes(folder), before)
            self.assertFalse((root / "previews").exists())

    def test_existing_backup_is_preserved_and_nothing_is_installed(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            folder, _, _ = package_fixture(root)
            before = file_bytes(folder)
            backup = root / "backup"
            backup.mkdir()
            (backup / "sentinel.bin").write_bytes(b"existing backup")
            with cheap_renderers():
                updates = repair.prepare(folder, root / "previews")
                with self.assertRaisesRegex(ValueError, "existing backups are preserved"):
                    repair.install([(folder, updates)], backup)
            self.assertEqual(file_bytes(folder), before)
            self.assertEqual(file_bytes(backup), {"sentinel.bin": b"existing backup"})

    def test_shared_icon_and_geometry_source_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            folder, _, _ = package_fixture(root)
            manifest = folder / "package.json"
            package = json.loads(manifest.read_text())
            package["files"][4]["source"] = package["files"][0]["source"]
            manifest.write_text(json.dumps(package))
            before = file_bytes(folder)
            with cheap_renderers():
                with self.assertRaisesRegex(ValueError, "separate source files"):
                    repair.prepare(folder, root / "previews")
            self.assertEqual(file_bytes(folder), before)

    def test_builder_packages_native_ui_encoders_and_optional_background(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source_dir, icons = root / "source", root / "icons"
            source_dir.mkdir()
            icons.mkdir()
            mesh = dict(name="BODY_H", positions=[[0, 0, 0], [1, 0, 0], [0, 1, 0]],
                        normals=[[0, 0, 1]] * 3, uvs=[[0, 0], [1, 0], [0, 1]],
                        triangles=[[0, 1, 2, 0]])
            meshes = [mesh]
            for name in builder.mm3_model.WHEEL_NODES.values():
                wheel = copy.deepcopy(mesh)
                wheel["name"] = name
                meshes.append(wheel)
            source = dict(materials=[dict(texture=None, diffuse=[1, 1, 1, 1])], meshes=meshes)
            source_path = source_dir / "model.json"
            source_path.write_text(json.dumps(source))
            # Exercise prefix fallback used by preview.py's --icon-prefix Car.
            for filename in ("Car_Menu.png", "Car_Small.png", "Car_Background.png"):
                (icons / filename).write_bytes(b"fixture-image")
            output = root / "package"
            with patch.object(builder, "Data") as data, \
                    patch.object(builder, "alias_paths", side_effect=lambda blob, donor, alias: blob), \
                    patch.object(builder.mm3_model, "convert_exterior", return_value=(b"model", {})), \
                    patch.object(builder.mm3_model, "write_omb", return_value=b"materials"), \
                    patch.object(builder.mm3_lights, "source_anchors", return_value=({}, {})), \
                    patch.object(builder.mm3_lights, "align_lights", return_value=(b"model", {})), \
                    patch.object(builder.mm3_textures, "image_to_cdds") as body_encoder, \
                    patch.object(builder.mm3_textures, "image_to_menu_cdds", return_value=b"native-icon") as menu_encoder, \
                    patch.object(builder.mm3_textures, "image_to_background_cdds", return_value=b"native-background") as bg_encoder, \
                    patch("sys.stdout", new=io.StringIO()):
                data.return_value.read.return_value = b"donor"
                builder.build(source_path, output, 9004, y_offset=0, icons=icons)
            body_encoder.assert_not_called()
            self.assertEqual([call.args[0].name for call in menu_encoder.call_args_list],
                             ["Car_Menu.png", "Car_Small.png"])
            bg_encoder.assert_called_once_with(icons / "Car_Background.png")
            package = json.loads((output / "package.json").read_text())
            ui_assets = [item for item in package["files"] if item["path"].startswith("Menu/")
                         or item["path"].endswith("/bg.cdds")]
            self.assertEqual({(item["archive"], item["path"]) for item in ui_assets}, {
                ("Data_dvd.zip", "Menu/Gfx/Vehicles/Panoz.cdds"),
                ("Data_dvd.zip", "Menu/Gfx/Vehicles/Panoz_Small.cdds"),
                ("loose", "Data/Shared/Cars/Panoz/bg.cdds"),
            })
            for item in ui_assets:
                expected = b"native-background" if item["archive"] == "loose" else b"native-icon"
                self.assertEqual((output / item["source"]).read_bytes(), expected)


if __name__ == "__main__":
    unittest.main()
