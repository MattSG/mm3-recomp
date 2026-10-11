"""Offline tests of writable catalog assets and reversible archive installation."""

import copy
import importlib.util
import io
import json
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("car_mods_prepare", ROOT / "tools/car-mods/prepare.py")
MOD = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MOD)


def zip_bytes(entries):
    result = io.BytesIO()
    with zipfile.ZipFile(result, "w", compression=zipfile.ZIP_STORED) as archive:
        for name, data in entries.items():
            archive.writestr(name, data)
    # Match the retail archives: locals have no extra field, while each central
    # entry carries a 36-byte NTFS field. Standard zipfile.writestr puts it in
    # both locations, so attach it to only the central directory here.
    original = result.getvalue()
    with zipfile.ZipFile(io.BytesIO(original)) as archive:
        start = archive.start_dir
        count = len(archive.infolist())
    ntfs = bytes.fromhex("0a0020000000000001001800") + bytes(24)
    central, pos = bytearray(), start
    for _ in range(count):
        name, extra, comment = struct.unpack_from("<HHH", original, pos + 28)
        end = pos + 46 + name + extra + comment
        record = bytearray(original[pos:pos + 46 + name + extra])
        struct.pack_into("<H", record, 30, extra + len(ntfs))
        central.extend(record + ntfs + original[pos + 46 + name + extra:end])
        pos = end
    eocd = struct.pack("<4s4H2IH", b"PK\x05\x06", 0, 0, count, count, len(central), start, 0)
    return original[:start] + bytes(central) + eocd


def material(names, donor, lod=False):
    data = b"\x00" + struct.pack("<I", len(names))
    for name in sorted(names):
        data += name.encode() + b"\x00+Cars\\" + donor.encode()
        data += (b"_lod1" if lod else b"") + b"\\texture.dds\x00"
        data += b"\xcc\x22\x33\x44\xff" + struct.pack("<I", 3)
    return data


class CarModsTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.game = Path(self.temp.name)
        self.config = json.loads((ROOT / "tools/car-mods/cars.json").read_text())
        hd, dvd = {"Cars/Tweak/SortOrder": b"12\r\n8\r\n11\r\n", "unrelated.bin": bytes(range(256))}, {"unrelated.texture": b"preserve me"}
        donor_ids = {"mini": 12, "MFastBack": 8, "Viper": 11, "Koenigsegg": 15, "Lotus": 18}
        for donor, stock_id in donor_ids.items():
            text = "Begin(CarInfo)\r\n\tID\t" + str(stock_id) + "\r\n"
            text += "\tCOLOR\t0 0x443322 0x443322\r\n\tSKIN\t1 0xff0000 0xff0000\r\n"
            text += "\tLOCKED\r\n\tHIDDEN\r\n\tADMITTANCE\t0 0 2\r\n"
            text += "".join(f"\t{k}\t1\r\n" for k in MOD.RATINGS)
            text += "\tVEHICLECLASS\t1\r\nEnd(CarInfo)\r\nBegin(CarBody)\r\n"
            text += "\tMass\t1500\r\n\tTorqueScale\t300\r\n\tNWheels\t4\r\nEnd(CarBody)\r\n"
            text += "Begin(CarWheel0)\r\n\tMass\t9\r\nEnd(CarWheel0)\r\n"
            text += "Begin(CarCamera)\r\n\tHoodPos\t0 1 2\r\n\tSpeedometerMaxKmh\t620\r\n\tSpeedometerMaxMph\t500\r\nEnd(CarCamera)\r\n"
            text += "Begin(CarCameraSS)\r\n\tOutFarStartZ\t6\r\nEnd(CarCameraSS)\r\n"
            hd[f"Cars/Tweak/{donor}.txt"] = text.encode()
            for suffix in ("", "_lod1"):
                base = f"Cars/{donor}{suffix}"
                names = MOD.PAINT_MATERIALS[donor.lower()][int(bool(suffix))]
                hd[f"{base}/materialSet00.omb"] = material(names, donor, bool(suffix))
                hd[f"{base}/data.ccol"] = b"collision unchanged"
                dvd[f"{base}/texture.cdds"] = b"texture unchanged"
                loose = self.game / f"Data/Shared/{base}"
                loose.mkdir(parents=True)
                (loose / "body.cmp").write_bytes(b"binary\x00+Cars\\" + (donor + suffix).encode() + b"\\materialSet00.oms\x00")
            hd[f"Cars/{donor}/Camera.txt"] = b"fov=40"
            for scope in ("Cars", "RaceAi"):
                hd[f"SoundScripts/{scope}/{donor}/Vehicle.mcs"] = b"native engine script"
            for suffix in ("", "_Small"):
                dvd[f"Menu/Gfx/Vehicles/{donor}{suffix}.cdds"] = b"menu image"
            dvd[f"LoadingScreens/Default_{donor}.tga"] = b"donor loading image " + donor.encode()
        self.originals = dict(zip(MOD.ARCHIVES, (zip_bytes(hd), zip_bytes(dvd))))
        for name, data in self.originals.items():
            target = self.game / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
        save = self.game / "UDATA/profile"
        save.parent.mkdir()
        save.write_bytes(b"do not change player save")

    def tearDown(self):
        self.temp.cleanup()

    def plan(self, config=None):
        return MOD.build_plan(self.game, self.originals, config or self.config)[0]

    def test_archive_prefix_offsets_payload_and_catalog(self):
        output = self.plan()
        for name, original in self.originals.items():
            self.assertTrue(output[name].startswith(original))
            with zipfile.ZipFile(io.BytesIO(original)) as before, zipfile.ZipFile(io.BytesIO(output[name])) as after:
                self.assertIsNone(after.testzip())
                self.assertEqual(len(after.namelist()), len(set(after.namelist())))
                for old in before.infolist():
                    if old.filename == "Cars/Tweak/SortOrder":
                        continue
                    new = after.getinfo(old.filename)
                    self.assertEqual(new.header_offset, old.header_offset)
                    self.assertEqual(after.read(new), before.read(old))
                for info in after.infolist():
                    self.assertEqual(info.compress_type, zipfile.ZIP_STORED)
                    name_length, extra_length = struct.unpack_from("<HH", output[name], info.header_offset + 26)
                    self.assertEqual(extra_length, 0)
                    payload = info.header_offset + 30 + name_length
                    # This is how the game's reader at 0x001BCD7E
                    # obtains resources; test real-reader bytes, not just ZIP CRC.
                    self.assertEqual(output[name][payload:payload + info.file_size], after.read(info))
                self.assertTrue(all(len(i.extra) == 36 for i in before.infolist()))
        with zipfile.ZipFile(io.BytesIO(output[MOD.ARCHIVES[0]])) as hd:
            added_ids = b"".join(str(car["id"]).encode() + b"\r\n" for car in self.config["cars"])
            self.assertEqual(hd.read("Cars/Tweak/SortOrder"), b"12\r\n8\r\n11\r\n" + added_ids)
            text = hd.read("Cars/Tweak/MRly.txt").decode()
            self.assertIn("ID\t\t9001", text)
            self.assertIn("Mass\t\t980", text)
            self.assertIn("HANDLING\t\t9", text)
            self.assertIn("\tMass\t9\r\nEnd(CarWheel0)", text)
            self.assertNotIn("LOCKED", text)
            self.assertNotIn("SKIN", text)
            paint = hd.read("Cars/MRly/materialSet00.omb")
            self.assertIn(b"Cars\\MRly\\texture.dds", paint)
            self.assertIn(bytes.fromhex("288cf2"), paint)
            self.assertIn(b"Cars\\MRly_lod1\\texture.dds", hd.read("Cars/MRly_lod1/materialSet00.omb"))
        mesh = output["Data/Shared/Cars/MRly/body.cmp"]
        self.assertIn(b"Cars\\MRly\\materialSet00.oms", mesh)
        self.assertEqual(len(mesh), len((self.game / "Data/Shared/Cars/mini/body.cmp").read_bytes()))
        with zipfile.ZipFile(io.BytesIO(output[MOD.ARCHIVES[1]])) as dvd:
            for car in self.config["cars"]:
                self.assertEqual(dvd.read(f"LoadingScreens/Default_{car['alias']}.tga"), dvd.read(f"LoadingScreens/Default_{car['donor']}.tga"))

    def test_requested_cars_are_independent_variants_of_shared_donors(self):
        expected = ["Panoz GTR-1", "McLaren F1", "Formula 1", "Bugatti Chiron", "Bugatti Bolide", "Ford GT"]
        self.assertEqual([car["display_name"] for car in self.config["cars"][3:]], expected)
        self.assertEqual(set(car["id"] for car in self.config["cars"]), set(range(9001, 9010)))
        output = self.plan()
        with zipfile.ZipFile(io.BytesIO(output[MOD.ARCHIVES[0]])) as hd:
            materials = set()
            for car in self.config["cars"]:
                alias = car["alias"]
                tweak = hd.read(f"Cars/Tweak/{alias}.txt").decode()
                self.assertIn(f"ID\t\t{car['id']}\r\n", tweak)
                self.assertIn(f"TorqueScale\t\t{car['physics']['TorqueScale']}\r\n", tweak)
                self.assertNotIn("LOCKED", tweak)
                for suffix in ("", "_lod1"):
                    material_name = f"Cars/{alias}{suffix}/materialSet00.omb"
                    self.assertNotIn(material_name.lower(), materials)
                    materials.add(material_name.lower())
                    self.assertIn(bytes.fromhex(car["colors"][0][1:])[::-1], hd.read(material_name))
                    mesh = output[f"Data/Shared/Cars/{alias}{suffix}/body.cmp"]
                    self.assertIn(f"Cars\\{alias}{suffix}\\materialSet00.oms".encode(), mesh)
                    self.assertEqual(len(mesh), len((self.game / f"Data/Shared/Cars/{car['donor']}{suffix}/body.cmp").read_bytes()))
            self.assertNotEqual(hd.read("Cars/McLaren_F1/materialSet00.omb"), hd.read("Cars/BChiron_GT/materialSet00.omb"))

    def test_speedometer_validation_and_camera_serialization(self):
        path = self.game / "speedometer.json"
        output = self.plan()
        with zipfile.ZipFile(io.BytesIO(output[MOD.ARCHIVES[0]])) as hd:
            tweak = hd.read("Cars/Tweak/BBolide_GT.txt").decode()
            self.assertEqual(tweak.count("SpeedometerMaxKmh\t\t800\r\n"), 2)
            self.assertEqual(tweak.count("SpeedometerMaxMph\t\t500\r\n"), 2)
            self.assertIn("SPEEDOMETERTYPE\t\t1\r\n", tweak)
            self.assertIn("HoodPos\t0 1 2", tweak)
            self.assertIn("OutFarStartZ\t6", tweak)
        legacy = copy.deepcopy(self.config)
        del legacy["cars"][0]["speedometer"]
        path.write_text(json.dumps(legacy))
        MOD.load_config(path)
        legacy_output = self.plan(legacy)
        with zipfile.ZipFile(io.BytesIO(legacy_output[MOD.ARCHIVES[0]])) as hd:
            self.assertIn(b"SpeedometerMaxKmh\t620\r\n", hd.read("Cars/Tweak/MRly.txt"))
        for value in [{"max_kmh": 800}, {"max_kmh": 800, "max_mph": 500, "bogus": 1},
                      {"max_kmh": 1201, "max_mph": 500}, {"max_kmh": 800, "max_mph": 0},
                      {"max_kmh": float("nan"), "max_mph": 500}, {"max_kmh": 800, "max_mph": True}, None]:
            config = copy.deepcopy(self.config)
            config["cars"][0]["speedometer"] = value
            path.write_text(json.dumps(config))
            with self.subTest(value=value), self.assertRaises(ValueError):
                MOD.load_config(path)

    def test_reapply_stats_is_idempotent_and_restore_is_exact(self):
        output = self.plan()
        self.assertGreater(MOD.apply_plan(self.game, None, self.originals, output, self.config), 0)
        state, stock = MOD.read_install(self.game)
        self.assertEqual(stock, self.originals)
        self.assertEqual(MOD.apply_plan(self.game, state, stock, output, self.config), 0)
        changed_config = copy.deepcopy(self.config)
        changed_config["cars"][0]["physics"]["Mass"] = 1050
        changed_config["cars"][0]["ratings"]["HANDLING"] = 8
        changed, _ = MOD.build_plan(self.game, stock, changed_config)
        self.assertGreater(MOD.apply_plan(self.game, state, stock, changed, changed_config), 0)
        with zipfile.ZipFile(self.game / MOD.ARCHIVES[0]) as archive:
            text = archive.read("Cars/Tweak/MRly.txt")
            self.assertIn(b"Mass\t\t1050", text)
            self.assertIn(b"HANDLING\t\t8", text)
        state, stock = MOD.read_install(self.game)
        MOD.restore(self.game, state, stock)
        for name, original in self.originals.items():
            self.assertEqual((self.game / name).read_bytes(), original)
        self.assertFalse((self.game / "Data/Shared/Cars/MRly/body.cmp").exists())
        self.assertEqual((self.game / "UDATA/profile").read_bytes(), b"do not change player save")

    def test_conflicting_edits_and_alias_collisions_fail_before_writes(self):
        output = self.plan()
        target = self.game / "Data/Shared/Cars/MRly/body.cmp"
        target.parent.mkdir()
        target.write_bytes(b"unrelated existing mod")
        with self.assertRaisesRegex(ValueError, "already exists"):
            MOD.apply_plan(self.game, None, self.originals, output, self.config)
        self.assertEqual((self.game / MOD.ARCHIVES[0]).read_bytes(), self.originals[MOD.ARCHIVES[0]])
        target.unlink()
        MOD.apply_plan(self.game, None, self.originals, output, self.config)
        target.write_bytes(b"later manual edit")
        with self.assertRaisesRegex(ValueError, "changed outside"):
            MOD.read_install(self.game)

    def test_invalid_stats_unknown_fields_and_identities(self):
        path = self.game / "cars.json"
        for key, value, parent in [("HANDLING", 11, "ratings"), ("ImaginaryTorque", 20, "physics"), ("Mass", float("nan"), "physics"), ("id", 9008, None), ("display_name", "Different car", None)]:
            config = copy.deepcopy(self.config)
            target = config["cars"][0][parent] if parent else config["cars"][0]
            target[key] = value
            path.write_text(json.dumps(config))
            with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                MOD.load_config(path)
        MOD.load_config(ROOT / "tools/car-mods/cars.json")

    def test_gearing_serialization_and_validation(self):
        config = copy.deepcopy(self.config)
        viper = config["cars"][2]
        ratios = [-3.3, 2.5, 1.72, 1.28, 1, 0.82, 0.7]
        viper["physics"].update({"GearRatioVector": ratios, "WheelRatio": 3.8, "TimeChangeGear": 0.3})
        path = self.game / "gearing.json"
        path.write_text(json.dumps(config))
        MOD.load_config(path)
        output = self.plan(config)
        with zipfile.ZipFile(io.BytesIO(output[MOD.ARCHIVES[0]])) as archive:
            tweak = archive.read("Cars/Tweak/VRace.txt")
            self.assertIn(b"NGears\t\t6\r\n", tweak)
            self.assertLess(tweak.index(b"NGears"), tweak.index(b"GearRatioVector"))
            self.assertIn(b"GearRatioVector\t\t-3.3 2.5 1.72 1.28 1 0.82 0.7\r\n", tweak)
            self.assertIn(b"WheelRatio\t\t3.8\r\n", tweak)
        fewer_gears = copy.deepcopy(config)
        fewer_gears["cars"][2]["physics"]["GearRatioVector"] = [-3.3, 2.5, 1.5, 0.8]
        fewer_output = self.plan(fewer_gears)
        with zipfile.ZipFile(io.BytesIO(fewer_output[MOD.ARCHIVES[0]])) as archive:
            tweak = archive.read("Cars/Tweak/VRace.txt")
            self.assertIn(b"NGears\t\t3\r\n", tweak)
            self.assertIn(b"GearRatioVector\t\t-3.3 2.5 1.5 0.8\r\n", tweak)
            self.assertLess(tweak.index(b"NGears"), tweak.index(b"GearRatioVector"))
        for key, invalid in [("GearRatioVector", [3.3, 2, 1]), ("GearRatioVector", [-3.3, 2, 2]),
                             ("GearRatioVector", [-3.3, 2]), ("GearRatioVector", [-3.3, 2, float("inf")]),
                             ("GearRatioVector", [-3.3, 4, 3, 2, 1.7, 1.4, 1.1, 0.8]),
                             ("GearRatioVector", [-3.3, 2, True]), ("NGears", 6),
                             ("WheelRatio", 0), ("MinRPMGearChange", 9000),
                             ("CutRPM", 11000), ("GearStatusDownGearFactor", 1)]:
            rejected = copy.deepcopy(config)
            rejected["cars"][2]["physics"][key] = invalid
            path.write_text(json.dumps(rejected))
            with self.subTest(key=key, invalid=invalid), self.assertRaises(ValueError):
                MOD.load_config(path)

    def test_failed_write_rolls_back_existing_archives_and_new_assets(self):
        output = self.plan()
        write = MOD.atomic_write
        calls = 0

        def interrupted(path, data):
            nonlocal calls
            if MOD.STATE_DIR not in str(path):
                calls += 1
                if calls == 3:
                    raise OSError("simulated unavailable disk")
            return write(path, data)

        with patch.object(MOD, "atomic_write", side_effect=interrupted), self.assertRaises(OSError):
            MOD.apply_plan(self.game, None, self.originals, output, self.config)
        for name, original in self.originals.items():
            self.assertEqual((self.game / name).read_bytes(), original)
        self.assertFalse((self.game / MOD.STATE_DIR / "state.json").exists())
        self.assertFalse((self.game / "Data/Shared/Cars/MRly/body.cmp").exists())

    def test_automatic_shifts_cannot_require_rpm_beyond_cutoff_or_native_torque_table(self):
        path = self.game / "rpm.json"
        for key, value in (("CutRPM", 6000), ("MaxTorqueRPM", 8000),
                           ("MaxTurboTorqueRPM", 8000), ("MaxTorqueRPM", 8500),
                           ("MaxTurboTorqueRPM", 8500), ("MaxTurboTorqueRPM", 10500),
                           ("MaxTorqueRPM", 1000)):
            rejected = copy.deepcopy(self.config)
            rejected["cars"][2]["physics"][key] = value
            path.write_text(json.dumps(rejected))
            with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                MOD.load_config(path)
        # A lower cutoff remains available when both automatic shift peaks are
        # lowered with it, rather than silently inheriting unusable donor peaks.
        valid = copy.deepcopy(self.config)
        valid["cars"][2]["physics"].update({"MaxTorqueRPM": 5500, "MaxTurboTorqueRPM": 5700, "CutRPM": 6000})
        path.write_text(json.dumps(valid))
        MOD.load_config(path)

    def test_failed_restore_retains_valid_installed_state(self):
        output = self.plan()
        MOD.apply_plan(self.game, None, self.originals, output, self.config)
        state, stock = MOD.read_install(self.game)
        write = MOD.atomic_write
        calls = 0

        def interrupted(path, data):
            nonlocal calls
            calls += 1
            if calls == 2:
                raise OSError("simulated locked second archive")
            return write(path, data)

        with patch.object(MOD, "atomic_write", side_effect=interrupted), self.assertRaises(OSError):
            MOD.restore(self.game, state, stock)
        next_state, next_stock = MOD.read_install(self.game)
        self.assertEqual(next_state, state)
        self.assertEqual(next_stock, stock)
        self.assertEqual((self.game / MOD.ARCHIVES[0]).read_bytes(), output[MOD.ARCHIVES[0]])

    def test_cancelled_initial_install_restores_originals_and_removes_state(self):
        output = self.plan()
        state_path = self.game / MOD.STATE_DIR / "state.json"
        write = MOD.atomic_write
        for phase in ("before second archive", "after state commit"):
            interrupted = False

            def cancel(path, data):
                nonlocal interrupted
                if not interrupted and phase == "before second archive" and path == self.game / MOD.ARCHIVES[1]:
                    interrupted = True
                    raise KeyboardInterrupt("cancelled before second archive")
                result = write(path, data)
                if not interrupted and phase == "after state commit" and path == state_path:
                    interrupted = True
                    raise KeyboardInterrupt("cancelled after state commit")
                return result

            with self.subTest(phase=phase), patch.object(MOD, "atomic_write", side_effect=cancel):
                with self.assertRaises(KeyboardInterrupt):
                    MOD.apply_plan(self.game, None, self.originals, output, self.config)
            self.assertTrue(interrupted)
            self.assertFalse(state_path.exists())
            for relative, original in self.originals.items():
                self.assertEqual((self.game / relative).read_bytes(), original)
                self.assertEqual((self.game / MOD.STATE_DIR / Path(relative).name).read_bytes(), original)
            for relative in output:
                if relative not in MOD.ARCHIVES:
                    self.assertFalse((self.game / relative).exists())
            state, stock = MOD.read_install(self.game)
            self.assertIsNone(state)
            self.assertEqual(stock, self.originals)
            MOD.build_plan(self.game, stock, self.config)  # A retry can prepare cleanly.
            self.assertEqual((self.game / "UDATA/profile").read_bytes(), b"do not change player save")

    def test_cancelled_reapply_preserves_exact_installation_and_state(self):
        output = self.plan()
        MOD.apply_plan(self.game, None, self.originals, output, self.config)
        state, stock = MOD.read_install(self.game)
        state_path = self.game / MOD.STATE_DIR / "state.json"
        previous_state = state_path.read_bytes()
        config = copy.deepcopy(self.config)
        config["cars"][0]["physics"]["TorqueScale"] += 1
        changed = self.plan(config)
        write = MOD.atomic_write
        for phase in ("before state commit", "after state commit"):
            interrupted = False

            def cancel(path, data):
                nonlocal interrupted
                if not interrupted and path == state_path:
                    interrupted = True
                    if phase == "after state commit":
                        write(path, data)
                    raise KeyboardInterrupt("cancelled at state commit")
                return write(path, data)

            with self.subTest(phase=phase), patch.object(MOD, "atomic_write", side_effect=cancel):
                with self.assertRaises(KeyboardInterrupt):
                    MOD.apply_plan(self.game, state, stock, changed, config)
            self.assertTrue(interrupted)
            self.assertEqual(state_path.read_bytes(), previous_state)
            for relative, previous in output.items():
                self.assertEqual((self.game / relative).read_bytes(), previous)
            self.assertEqual(MOD.read_install(self.game), (state, stock))
            self.assertEqual((self.game / "UDATA/profile").read_bytes(), b"do not change player save")

    def test_cancelled_restore_preserves_exact_installation_and_state(self):
        output = self.plan()
        MOD.apply_plan(self.game, None, self.originals, output, self.config)
        state, stock = MOD.read_install(self.game)
        state_path = self.game / MOD.STATE_DIR / "state.json"
        previous_state = state_path.read_bytes()
        write, unlink = MOD.atomic_write, Path.unlink
        for phase in ("before second archive", "after state removal"):
            interrupted = False

            def cancel_write(path, data):
                nonlocal interrupted
                if not interrupted and phase == "before second archive" and path == self.game / MOD.ARCHIVES[1]:
                    interrupted = True
                    raise KeyboardInterrupt("cancelled before second archive")
                return write(path, data)

            def cancel_unlink(path, *args, **kwargs):
                nonlocal interrupted
                result = unlink(path, *args, **kwargs)
                if not interrupted and phase == "after state removal" and path == state_path:
                    interrupted = True
                    raise KeyboardInterrupt("cancelled after state removal")
                return result

            with self.subTest(phase=phase), patch.object(MOD, "atomic_write", side_effect=cancel_write), \
                    patch.object(Path, "unlink", autospec=True, side_effect=cancel_unlink):
                with self.assertRaises(KeyboardInterrupt):
                    MOD.restore(self.game, state, stock)
            self.assertTrue(interrupted)
            self.assertEqual(state_path.read_bytes(), previous_state)
            for relative, previous in output.items():
                self.assertEqual((self.game / relative).read_bytes(), previous)
            self.assertEqual(MOD.read_install(self.game), (state, stock))
            self.assertEqual((self.game / "UDATA/profile").read_bytes(), b"do not change player save")


if __name__ == "__main__":
    unittest.main()
