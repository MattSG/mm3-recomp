#!/usr/bin/env python3
"""Install editable MM3 variants without moving any original ZIP local headers.

Only the user's extracted game data is modified. The first successful install
backs up both archives; subsequent edits rebuild from those originals. A dry run
is the default. The original meshes are reused with equal-length path aliases.
"""

from __future__ import annotations

import argparse
import copy
import hashlib
import io
import json
import math
import os
from pathlib import Path
import re
import struct
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[2]


def default_config_path(directory: Path | None = None) -> Path:
    """Prefer machine-local settings without shipping model asset paths."""
    folder = Path(directory) if directory is not None else Path(__file__).resolve().parent
    local = folder / "cars.local.json"
    return local if local.is_file() else folder / "cars.json"


ARCHIVES = ("Data/Data_hd.zip", "Data/Data_dvd.zip")
STATE_DIR = ".mm3-car-mods"
RATINGS = {"TOPSPEED", "ACCELERATION", "MASS", "HANDLING", "DURABILITY"}
SPEEDOMETER = {"max_kmh": (100, 1200), "max_mph": (60, 750)}
SPEEDOMETER_FILE = "Data/mm3_speedometers.ini"
ORIGINAL_SPEEDOMETERS = {
    "AudiTT": "Audi TT", "Avalanche": "Chevrolet Avalanche", "Avant": "Audi RS 6 Avant",
    "ChevySSR": "Chevrolet SSR", "Corvette": "Chevrolet Corvette", "Crossfire": "Chrysler Crossfire",
    "Koenigsegg": "Koenigsegg", "Lotus": "Lotus", "MFastBack": "Mustang Fastback",
    "NewBeetle": "Volkswagen New Beetle", "Saab93": "Saab 9-3",
    "SerpentMk2": "Serpent Mk II", "Viper": "Dodge Viper",
}
PHYSICS = {
    "Mass": (100, 10000), "TorqueScale": (1, 5000),
    "TorqueScaleFactor": (0.1, 5), "WheelDrive": (0, 2),
    "BrakeTorque": (100, 30000), "HandBrakeTorque": (100, 30000),
    "LatFrictionFactor": (0.1, 5), "AirKZZ": (0.001, 10),
    "AirKZY": (0.001, 10), "MaxTorqueRPM": (500, 8000),
    "MaxRPM": (500, 20000), "CutRPM": (500, 20000),
    "MinRPM": (100, 10000), "TimeChangeGear": (0.05, 5),
    "TimeChangeGearFR": (0.05, 5), "WheelRatio": (0.5, 10),
    "MinRPMGearChange": (500, 20000), "MaxTurboTorqueRPM": (500, 8000),
    "GearStatusDownGearFactor": (0.1, 0.95),
    "SteeringEndSpeed": (20, 1000), "WheelRadius": (0.1, 2),
}
PAINT_MATERIALS = {
    "mini": ({"body1", "texture_gloss", "minitexturMatt"}, {"Skin_Blank"}),
    "mfastback": ({"Color", "Color_tex_logo", "Color_tex_mply"}, {"Lodtexture"}),
    "viper": ({"Body1", "ViperTextureAlpha", "ViperTextureMulti"}, {"Texture_Gloss"}),
    "koenigsegg": ({"Lack", "Lack_textur"}, {"Lod_texture"}),
    "lotus": ({"Lacken", "Lacktexturh"}, {"TextureBlank"}),
}
IDENTITIES = {
    9001: ("MRly", "mini", "Mini Rally"),
    9002: ("MustangGT", "MFastBack", "Mustang GT"),
    9003: ("VRace", "Viper", "Viper Race"),
    9004: ("Panoz", "Viper", "Panoz GTR-1"),
    9005: ("McLaren_F1", "Koenigsegg", "McLaren F1"),
    9006: ("Form1", "Lotus", "Formula 1"),
    9007: ("BChiron_GT", "Koenigsegg", "Bugatti Chiron"),
    9008: ("BBolide_GT", "Koenigsegg", "Bugatti Bolide"),
    9009: ("Ford_GTcar", "Koenigsegg", "Ford GT"),
    9010: ("Senna_Road", "Koenigsegg", "McLaren Senna"),
    9011: ("Senna_GTR1", "Koenigsegg", "McLaren Senna GTR"),
    9012: ("FXXK_Racer", "Koenigsegg", "Ferrari FXX K"),
    9013: ("Regera_GT1", "Koenigsegg", "Koenigsegg Regera"),
    9014: ("AventadorX", "Koenigsegg", "Lamborghini Aventador"),
}


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def exact_keys(obj: dict, allowed: set[str], description: str) -> None:
    if not isinstance(obj, dict):
        raise ValueError(f"{description} must be an object")
    unknown = set(obj) - allowed
    if unknown:
        raise ValueError(f"Unknown {description} fields: {', '.join(sorted(unknown))}")


def load_config(path: Path) -> dict:
    config = json.loads(path.read_text(encoding="utf-8"))
    exact_keys(config, {"version", "cars", "original_speedometers"}, "manifest")
    if config.get("version") != 1 or not isinstance(config.get("cars"), list):
        raise ValueError("Manifest requires version 1 and a cars array")
    originals = config.get("original_speedometers", {})
    exact_keys(originals, set(ORIGINAL_SPEEDOMETERS), "original speedometer")
    for settings in originals.values():
        validate_speedometer(settings)
    seen_ids, seen_aliases = set(), set()
    for car in config["cars"]:
        required = {"id", "alias", "donor", "display_name", "ratings", "physics", "colors"}
        exact_keys(car, required | {"speedometer", "model_package"}, "car")
        if not required <= set(car):
            raise ValueError("Each car requires id, alias, donor, display_name, ratings, physics, colors")
        ident = car["id"]
        if type(ident) is not int or not 1 <= ident <= 32767 or ident in seen_ids:
            raise ValueError("Car IDs must be unique integers from 1 to 32767")
        seen_ids.add(ident)
        if IDENTITIES.get(ident) != (car["alias"], car["donor"], car["display_name"]):
            raise ValueError("Car id, alias, donor and display_name are fixed identities; edit ratings, physics, colors, speedometer and model_package")
        for key in ("alias", "donor"):
            if not isinstance(car[key], str) or not re.fullmatch(r"[A-Za-z][A-Za-z0-9_]{1,31}", car[key]):
                raise ValueError(f"Invalid car {key}")
        if len(car["alias"]) != len(car["donor"]):
            raise ValueError("Alias and donor must have equal lengths to preserve compiled mesh layouts")
        if car["alias"].lower() == car["donor"].lower() or car["alias"].lower() in seen_aliases:
            raise ValueError("Car aliases must be unique and different from their donors")
        seen_aliases.add(car["alias"].lower())
        if car["donor"].lower() not in PAINT_MATERIALS:
            raise ValueError("Supported donors: " + ", ".join(sorted(PAINT_MATERIALS)))
        if not isinstance(car["display_name"], str) or not re.fullmatch(r"[ -~]{1,50}", car["display_name"]):
            raise ValueError("Display names must contain 1 to 50 printable ASCII characters")
        exact_keys(car["ratings"], RATINGS, "rating")
        if set(car["ratings"]) != RATINGS or any(type(v) is not int or not 0 <= v <= 9 for v in car["ratings"].values()):
            raise ValueError("All five ratings must be integers from 0 to 9")
        exact_keys(car["physics"], set(PHYSICS) | {"GearRatioVector"}, "physics")
        for key, value in car["physics"].items():
            if key == "GearRatioVector":
                if not isinstance(value, list) or not 3 <= len(value) <= 7:
                    raise ValueError("GearRatioVector requires a reverse ratio followed by 2 to 6 forward gears")
                if any(type(ratio) not in (int, float) or not math.isfinite(ratio) for ratio in value):
                    raise ValueError("Gear ratios must be finite numbers")
                if not -10 <= value[0] <= -0.1 or any(not 0.1 <= ratio <= 10 for ratio in value[1:]):
                    raise ValueError("Reverse ratio must be negative and forward ratios positive, with magnitudes 0.1 to 10")
                if any(a <= b for a, b in zip(value[1:], value[2:])):
                    raise ValueError("Forward gear ratios must decrease as gears increase")
                continue
            low, high = PHYSICS[key]
            if key in {"MaxTorqueRPM", "MaxTurboTorqueRPM"} and type(value) in (int, float) and math.isfinite(value) and value > high:
                raise ValueError("Supported stock torque curve ends near 8500 RPM; automatic peak must be <= 8000 RPM")
            if type(value) not in (int, float) or not math.isfinite(value) or not low <= value <= high:
                raise ValueError(f"{key} must be a finite number from {low} to {high}")
            if key == "WheelDrive" and type(value) is not int:
                raise ValueError("WheelDrive must be 0 (front), 1 (rear), or 2 (all wheels)")
        # The supported donors all use this idle/shift/cutoff/redline ordering.
        physics = car["physics"]
        idle, shift, cutoff, redline = (physics.get(key, default) for key, default in
                                      (("MinRPM", 1000), ("MinRPMGearChange", 4000),
                                       ("CutRPM", 8000), ("MaxRPM", 10000)))
        if not idle < shift < cutoff <= redline:
            raise ValueError("RPM settings require MinRPM < MinRPMGearChange < CutRPM <= MaxRPM")
        # sub_001CF7B4 builds road-speed upshift thresholds from peak RPM - 50;
        # sub_001CB7E8 samples the supported stock curve, which ends near 8500
        # RPM (and has a hard 9500-RPM limit); keep peaks <= 8000 for headroom.
        # All supported donors inherit these two peaks. Lowering only CutRPM
        # must not leave an automatic shift threshold beyond the usable engine.
        for key, default in (("MaxTorqueRPM", 6500), ("MaxTurboTorqueRPM", 6750)):
            if not idle < physics.get(key, default) < cutoff:
                raise ValueError(f"{key} must be above MinRPM and below CutRPM for automatic gear changes")
        if not isinstance(car["colors"], list) or not 1 <= len(car["colors"]) <= 3 or any(not isinstance(c, str) or not re.fullmatch(r"#[0-9a-fA-F]{6}", c) for c in car["colors"]):
            raise ValueError("Each car requires 1 to 3 colors in #rrggbb form")
        if "speedometer" in car:
            validate_speedometer(car["speedometer"])
        if "model_package" in car:
            value = car["model_package"]
            if not isinstance(value, str) or "\x00" in value or len(value) > 4096:
                raise ValueError("model_package must be a prepared model folder path, or blank to reuse the donor")
    if not config["cars"]:
        raise ValueError("At least one car is required; use --restore to remove installed variants")
    return config


def validate_speedometer(settings: dict) -> None:
    exact_keys(settings, set(SPEEDOMETER), "speedometer")
    if set(settings) != set(SPEEDOMETER):
        raise ValueError("Speedometer requires both max_kmh and max_mph")
    for key, (low, high) in SPEEDOMETER.items():
        value = settings[key]
        if type(value) not in (int, float) or not math.isfinite(value) or not low <= value <= high:
            raise ValueError(f"Speedometer {key} must be a finite number from {low} to {high}")


def speedometer_settings(config: dict) -> bytes:
    """Write only explicit overrides; absent aliases use the retail HUD."""
    lines = ["# MM3 speedometer maxima: alias max_kmh max_mph"]
    entries = list(config.get("original_speedometers", {}).items())
    entries += [(car["alias"], car["speedometer"]) for car in config["cars"] if "speedometer" in car]
    for alias, limits in entries:
        lines.append(f"{alias} {limits['max_kmh']:g} {limits['max_mph']:g}")
    return ("\n".join(lines) + "\n").encode("ascii")


def package_source(root: Path, relative: str) -> Path:
    """Accept only ordinary files below the selected package, without links."""
    if not isinstance(relative, str) or not relative or "\\" in relative or ":" in relative:
        raise ValueError("Model source must be a relative path using forward slashes")
    parts = relative.split("/")
    if any(part in ("", ".", "..") for part in parts):
        raise ValueError("Model source must remain inside its package folder")
    current = root
    for part in (None, *parts):
        if part is not None:
            current /= part
        info = current.lstat()
        # Junctions and other Windows reparse points are links too.
        if current.is_symlink() or getattr(info, "st_file_attributes", 0) & 0x400:
            raise ValueError("Model packages cannot contain symbolic links or junctions")
    if not current.is_file() or not current.resolve().is_relative_to(root.resolve()):
        raise ValueError("Model source must be a file inside its package folder")
    return current


def load_model_package(car: dict, config_dir: Path) -> list[tuple[str, str, bytes]]:
    """Read prepared MM3 visual assets; raw MM2 archives are not model packages."""
    folder = car.get("model_package", "")
    if not isinstance(folder, str):
        raise ValueError("model_package must be a prepared model folder path")
    folder = folder.strip()
    if not folder:
        return []
    root = Path(folder)
    if not root.is_absolute():
        root = config_dir / root
    for ancestor in (root, *root.parents):
        info = ancestor.lstat()
        if ancestor.is_symlink() or getattr(info, "st_file_attributes", 0) & 0x400:
            raise ValueError("Model package folder cannot be reached through symbolic links or junctions")
    manifest = package_source(root, "package.json")
    package = json.loads(manifest.read_text(encoding="utf-8"))
    exact_keys(package, {"format", "alias", "car_id", "files"}, "model package")
    if package.get("format") != "mm3-car-model-v1":
        raise ValueError("Expected a prepared mm3-car-model-v1 package")
    if "alias" not in package and "car_id" not in package:
        raise ValueError("Model package requires its target alias or car_id")
    if "alias" in package and package["alias"] != car["alias"]:
        raise ValueError("Model package alias does not match the selected car")
    if "car_id" in package and (type(package["car_id"]) is not int or package["car_id"] != car["id"]):
        raise ValueError("Model package car_id does not match the selected car")
    files = package.get("files")
    if not isinstance(files, list) or not files:
        raise ValueError("Model package requires a nonempty files array")
    assets, seen = [], set()
    alias = re.escape(car["alias"])
    for item in files:
        exact_keys(item, {"archive", "path", "source"}, "model package file")
        if set(item) != {"archive", "path", "source"}:
            raise ValueError("Each model package file requires archive, path and source")
        archive, target = item["archive"], item["path"]
        if archive not in ("loose", "Data_hd.zip", "Data_dvd.zip") or not isinstance(target, str):
            raise ValueError("Model package archive must be loose, Data_hd.zip or Data_dvd.zip")
        prefix = "Data/Shared/" if archive == "loose" else ""
        pattern = prefix + r"Cars/" + alias + r"(?:_lod[1-9][0-9]*)?/(?:[A-Za-z0-9_-]+/)*[A-Za-z0-9_-]+\.(cmp|omb|cdds)"
        match = re.fullmatch(pattern, target)
        thumbnail = archive != "loose" and re.fullmatch(r"Menu/Gfx/Vehicles/" + alias + r"(?:_Small)?\.cdds", target)
        loading = archive != "loose" and re.fullmatch(r"LoadingScreens/Default_" + alias + r"\.tga", target)
        if not (match or thumbnail or loading):
            raise ValueError("Model destination must be a visual asset in this car's own folder, thumbnail or loading screen")
        extension = match.group(1) if match else ("cdds" if thumbnail else "tga")
        if (archive == "loose") != (extension == "cmp"):
            raise ValueError("Model CMP files must be loose; OMB materials and CDDS textures must be archived")
        destination = (archive, target.casefold())
        if destination in seen:
            raise ValueError("Duplicate model package destination")
        seen.add(destination)
        source = package_source(root, item["source"])
        assets.append((archive, target, source.read_bytes()))
    return assets


def update_section(text: str, section: str, values: dict, remove: set[str] = frozenset()) -> str:
    pattern = re.compile(r"(Begin\(" + re.escape(section) + r"\)\r?\n)(.*?)(End\(" + re.escape(section) + r"\))", re.S)
    match = pattern.search(text)
    if not match:
        raise ValueError(f"Missing {section} in donor tweak")
    body = match.group(2)
    for key in remove | set(values):
        body = re.sub(r"^[ \t]*" + re.escape(key) + r"(?:[ \t]+[^\r\n]*)?\r?\n", "", body, flags=re.M)
    body += "".join(f"\t{key}\t\t{value}\r\n" for key, value in values.items())
    return text[:match.start(2)] + body + text[match.end(2):]


def tweak_variant(original: bytes, car: dict) -> bytes:
    text = original.decode("ascii")
    info = {"ID": car["id"], **car["ratings"], "PAINTJOBNAME": car["alias"]}
    if "speedometer" in car:
        # The sport HUD supports the native extended digital speed readout.
        info["SPEEDOMETERTYPE"] = 1
    text = update_section(text, "CarInfo", info, {"LOCKED", "HIDDEN", "ADMITTANCE", "SKIN", "COLOR"})
    colors = "".join(f"\tCOLOR\t\t{i} 0x{c[1:]} 0x{c[1:]}\r\n" for i, c in enumerate(car["colors"]))
    text = text.replace("End(CarInfo)", colors + "End(CarInfo)", 1)
    physics = dict(car["physics"])
    if "GearRatioVector" in physics:
        ratios = physics["GearRatioVector"]
        physics = {"NGears": len(ratios) - 1, **physics}
        physics["GearRatioVector"] = " ".join(format(ratio, ".9g") for ratio in ratios)
    text = update_section(text, "CarBody", physics)
    if "speedometer" in car:
        gauges = {"SpeedometerMaxKmh": car["speedometer"]["max_kmh"],
                  "SpeedometerMaxMph": car["speedometer"]["max_mph"]}
        text = update_section(text, "CarCamera", gauges)
        if "Begin(CarCameraSS)" in text:
            text = update_section(text, "CarCameraSS", gauges)
    return text.encode("ascii")


def alias_paths(data: bytes, donor: str, alias: str) -> bytes:
    """Change only a Cars/<donor> component, never similarly named textures."""
    pattern = re.compile(rb"(Cars[\\/])" + re.escape(donor.encode()) + rb"(?=([\\/]|_lod1[\\/]))", re.I)
    result = pattern.sub(lambda m: m.group(1) + alias.encode(), data)
    if len(result) != len(data):
        raise ValueError("Compiled asset length changed")
    return result


def recolor_material(data: bytes, names: set[str], color: str) -> bytes:
    if len(data) < 5 or data[0] != 0:
        raise ValueError("Unsupported OMB header")
    out, pos, changed = bytearray(data), 5, set()
    for _ in range(struct.unpack_from("<I", data, 1)[0]):
        end = data.index(0, pos)
        name = data[pos:end].decode("ascii")
        pos = data.index(0, end + 1) + 1
        if pos + 9 > len(data) or data[pos] != 0xCC:
            raise ValueError("Unsupported OMB material record")
        if name in names:
            rgb = bytes.fromhex(color[1:])
            out[pos + 1:pos + 4] = rgb[::-1]
            changed.add(name)
        pos += 9
    if pos != len(data) or changed != names:
        raise ValueError(f"Expected paint materials missing: {sorted(names - changed)}")
    return bytes(out)


def append_zip(original: bytes, updates: dict[str, tuple[bytes, zipfile.ZipInfo]]) -> bytes:
    """Keep the entire original archive, append locals and one authoritative CD.

    Original local header offsets and payload bytes never change. Replaced names
    occur once in the final directory, even though old bytes remain in the file.
    """
    if not updates:
        return original
    with zipfile.ZipFile(io.BytesIO(original)) as stock:
        original_infos = stock.infolist()
        cd_start = stock.start_dir
    records, pos = [], cd_start
    for info in original_infos:
        if original[pos:pos + 4] != b"PK\x01\x02":
            raise ValueError("Unsupported ZIP central directory")
        size = 46 + sum(struct.unpack_from("<HHH", original, pos + 28))
        if info.filename not in updates:
            records.append(original[pos:pos + size])
        pos += size
    scratch = io.BytesIO()
    with zipfile.ZipFile(scratch, "w", allowZip64=False) as additions:
        for name, (data, template) in updates.items():
            info = copy.copy(template)
            info.filename = name
            info.orig_filename = name
            info.compress_type = zipfile.ZIP_STORED
            info.flag_bits = 0
            # Retail has NTFS metadata only in the central directory. Its asset
            # reader at 0x001BCD7E computes payload = local offset + 30 +
            # central filename length, without reading the local extra length;
            # putting that central-only metadata in a local header shifts every
            # binary resource by 36 bytes even though standard ZIP readers pass.
            info.extra = b""
            additions.writestr(info, data)
    added = scratch.getvalue()
    with zipfile.ZipFile(io.BytesIO(added)) as archive:
        new_cd = archive.start_dir
        infos = archive.infolist()
    pos = new_cd
    for _ in infos:
        size = 46 + sum(struct.unpack_from("<HHH", added, pos + 28))
        record = bytearray(added[pos:pos + size])
        offset = struct.unpack_from("<I", record, 42)[0] + len(original)
        struct.pack_into("<I", record, 42, offset)
        records.append(bytes(record))
        pos += size
    central = b"".join(records)
    count = len(records)
    offset = len(original) + new_cd
    if count > 65535 or offset + len(central) >= 0xFFFFFFFF:
        raise ValueError("ZIP64 archives are not supported by this installer")
    eocd = struct.pack("<4s4H2IH", b"PK\x05\x06", 0, 0, count, count, len(central), offset, 0)
    return original + added[:new_cd] + central + eocd


def find_info(archive: zipfile.ZipFile, name: str) -> zipfile.ZipInfo:
    matches = [i for i in archive.infolist() if i.filename.lower() == name.lower()]
    if len(matches) != 1:
        raise ValueError(f"Expected one donor asset: {name}")
    return matches[0]


def build_plan(game_dir: Path, originals: dict[str, bytes], config: dict,
               config_dir: Path | None = None) -> tuple[dict[str, bytes], dict[str, int]]:
    output, counts = {}, {"cars": len(config["cars"]), "archive_entries": 0, "loose_files": 0}
    packages = [asset for car in config["cars"]
                for asset in load_model_package(car, config_dir or Path(__file__).parent)]
    hd = zipfile.ZipFile(io.BytesIO(originals[ARCHIVES[0]]))
    installed_ids = set()
    for info in hd.infolist():
        if re.fullmatch(r"Cars/Tweak/[^/]+\.txt", info.filename, re.I):
            m = re.search(rb"\bID\s+(\d+)", hd.read(info))
            if m:
                installed_ids.add(int(m.group(1)))
    for car in config["cars"]:
        if car["id"] in installed_ids:
            raise ValueError(f"Car ID already exists: {car['id']}")
    for relative, baseline in originals.items():
        archive = zipfile.ZipFile(io.BytesIO(baseline))
        updates = {}
        for car in config["cars"]:
            donor, alias = car["donor"], car["alias"]
            for info in archive.infolist():
                parts = info.filename.split("/")
                base = parts[1] if len(parts) > 2 and parts[0].lower() == "cars" else ""
                if base.lower() in (donor.lower(), donor.lower() + "_lod1"):
                    target_base = alias + ("_lod1" if base.lower().endswith("_lod1") else "")
                    target = "/".join([parts[0], target_base, *parts[2:]])
                    if info.filename.lower().endswith(".omb"):
                        # Build each chosen paint from the donor's first material set.
                        if parts[-1].lower() != "materialset00.omb":
                            continue
                        names = PAINT_MATERIALS[donor.lower()][int(base.lower().endswith("_lod1"))]
                        for index, color in enumerate(car["colors"]):
                            name = "/".join([parts[0], target_base, f"materialSet{index:02d}.omb"])
                            updates[name] = (recolor_material(alias_paths(archive.read(info), donor, alias), names, color), info)
                    else:
                        updates[target] = (alias_paths(archive.read(info), donor, alias), info)
                elif re.fullmatch(r"SoundScripts/(Cars|RaceAi)/" + re.escape(donor) + r"/Vehicle\.mcs", info.filename, re.I):
                    target = "/".join([*parts[:2], alias, *parts[3:]])
                    updates[target] = (archive.read(info), info)
                elif re.fullmatch(r"Menu/Gfx/Vehicles/" + re.escape(donor) + r"(_Small)?\.cdds", info.filename, re.I):
                    suffix = "_Small" if "_small." in info.filename.lower() else ""
                    target = f"Menu/Gfx/Vehicles/{alias}{suffix}.cdds"
                    updates[target] = (archive.read(info), info)
                elif re.fullmatch(r"LoadingScreens/Default_" + re.escape(donor) + r"\.tga", info.filename, re.I):
                    target = f"LoadingScreens/Default_{alias}.tga"
                    updates[target] = (archive.read(info), info)
            if relative == ARCHIVES[0]:
                info = find_info(archive, f"Cars/Tweak/{donor}.txt")
                updates[f"Cars/Tweak/{alias}.txt"] = (tweak_variant(archive.read(info), car), info)
        if relative == ARCHIVES[0]:
            info = find_info(archive, "Cars/Tweak/SortOrder")
            current = archive.read(info).rstrip(b"\r\n")
            current += b"\r\n" + b"\r\n".join(str(c["id"]).encode() for c in config["cars"]) + b"\r\n"
            updates[info.filename] = (current, info)
        # Prepared visuals win after donor cloning on every apply. Physics,
        # collision, sounds and the catalog continue to come from the donor.
        for destination, name, data in packages:
            if destination != Path(relative).name:
                continue
            old = next((key for key in updates if key.casefold() == name.casefold()), None)
            template = updates.pop(old)[1] if old else zipfile.ZipInfo(name)
            updates[name] = (data, template)
        existing = {i.filename.lower() for i in archive.infolist()}
        for name in updates:
            if name.lower() in existing and name.lower() != "cars/tweak/sortorder":
                raise ValueError(f"Alias would overwrite an installed asset: {name}")
        result = append_zip(baseline, updates)
        with zipfile.ZipFile(io.BytesIO(result)) as check:
            if len(check.namelist()) != len(set(n.lower() for n in check.namelist())) or check.testzip():
                raise ValueError(f"Generated archive failed integrity check: {relative}")
        output[relative] = result
        counts["archive_entries"] += len(updates)
    hd.close()
    cars_dir = game_dir / "Data/Shared/Cars"
    for car in config["cars"]:
        for suffix in ("", "_lod1"):
            candidates = [p for p in cars_dir.iterdir() if p.is_dir() and p.name.lower() == (car["donor"] + suffix).lower()]
            if len(candidates) != 1:
                raise ValueError(f"Missing loose donor directory: {car['donor']}{suffix}")
            for source in candidates[0].iterdir():
                if source.is_file():
                    relative = f"Data/Shared/Cars/{car['alias']}{suffix}/{source.name}"
                    output[relative] = alias_paths(source.read_bytes(), car["donor"], car["alias"])
                    counts["loose_files"] += 1
    for destination, relative, data in packages:
        if destination == "loose":
            old = next((key for key in output if key.casefold() == relative.casefold()), None)
            if old is None:
                counts["loose_files"] += 1
            elif old != relative:
                output.pop(old)
            output[relative] = data
    output[SPEEDOMETER_FILE] = speedometer_settings(config)
    counts["loose_files"] += 1
    return output, counts


def atomic_write(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix=".mm3-cars-", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(data)
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def read_install(game_dir: Path) -> tuple[dict | None, dict[str, bytes]]:
    state_path = game_dir / STATE_DIR / "state.json"
    state = json.loads(state_path.read_text()) if state_path.exists() else None
    if state:
        for relative, expected in state["installed"].items():
            path = game_dir / relative
            if not path.is_file() or digest(path.read_bytes()) != expected:
                raise ValueError(f"Installed file changed outside this tool: {relative}; preserve that edit before reapplying or restoring")
        originals = {}
        for relative in ARCHIVES:
            backup = game_dir / STATE_DIR / Path(relative).name
            data = backup.read_bytes()
            if digest(data) != state["originals"][relative]:
                raise ValueError(f"Original archive backup failed integrity check: {backup}")
            originals[relative] = data
    else:
        originals = {relative: (game_dir / relative).read_bytes() for relative in ARCHIVES}
    return state, originals


def apply_plan(game_dir: Path, state: dict | None, originals: dict[str, bytes], output: dict[str, bytes], config: dict) -> int:
    owned = set(state["installed"]) if state else set()
    for relative in output:
        if relative not in ARCHIVES and relative not in owned and (game_dir / relative).exists():
            raise ValueError(f"New asset path already exists: {relative}")
    if not state:
        for relative, data in originals.items():
            backup = game_dir / STATE_DIR / Path(relative).name
            if backup.exists() and digest(backup.read_bytes()) != digest(data):
                raise ValueError(f"An unrelated backup already exists: {backup}")
            atomic_write(backup, data)
    changed, rollback = 0, {}
    try:
        for relative, data in output.items():
            target = game_dir / relative
            previous = target.read_bytes() if target.exists() else None
            if previous is None or digest(previous) != digest(data):
                rollback[relative] = previous
                atomic_write(target, data)
                changed += 1
        for relative in owned - set(output):
            if relative not in ARCHIVES:
                rollback[relative] = (game_dir / relative).read_bytes()
                (game_dir / relative).unlink()
        next_state = {"version": 1, "originals": {k: digest(v) for k, v in originals.items()}, "installed": {k: digest(v) for k, v in output.items()}, "config": config}
        state_relative = f"{STATE_DIR}/state.json"
        state_path = game_dir / state_relative
        rollback[state_relative] = state_path.read_bytes() if state_path.exists() else None
        atomic_write(state_path, (json.dumps(next_state, indent=2) + "\n").encode())
        return changed
    except (OSError, KeyboardInterrupt):
        for relative, previous in rollback.items():
            if previous is None:
                (game_dir / relative).unlink(missing_ok=True)
            else:
                atomic_write(game_dir / relative, previous)
        raise


def restore(game_dir: Path, state: dict | None, originals: dict[str, bytes]) -> None:
    if not state:
        raise ValueError("No car-mod installation exists")
    rollback = {}
    try:
        for relative, data in originals.items():
            rollback[relative] = (game_dir / relative).read_bytes()
            atomic_write(game_dir / relative, data)
        for relative in state["installed"]:
            if relative not in ARCHIVES:
                rollback[relative] = (game_dir / relative).read_bytes()
                (game_dir / relative).unlink()
        state_relative = f"{STATE_DIR}/state.json"
        state_path = game_dir / state_relative
        rollback[state_relative] = state_path.read_bytes()
        state_path.unlink()
    except (OSError, KeyboardInterrupt):
        for relative, previous in rollback.items():
            atomic_write(game_dir / relative, previous)
        raise
    # Backups remain available after restoration; user profiles are never read.


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game-dir", type=Path, default=ROOT / "game_files")
    parser.add_argument("--config", type=Path, default=default_config_path(),
                        help="Manifest path (default: cars.local.json when present, otherwise cars.json)")
    actions = parser.add_mutually_exclusive_group()
    actions.add_argument("--apply", action="store_true")
    actions.add_argument("--restore", action="store_true")
    return parser.parse_args(argv)


def main() -> int:
    args = parse_args()
    try:
        game_dir = args.game_dir.resolve()
        state, originals = read_install(game_dir)
        if args.restore:
            restore(game_dir, state, originals)
            print("Restored both original archives and removed installed variant assets. Backups retained.")
            return 0
        config = load_config(args.config)
        output, counts = build_plan(game_dir, originals, config, args.config.resolve().parent)
        for car in config["cars"]:
            body = "prepared custom model" if car.get("model_package", "").strip() else car["donor"] + " model"
            print(f"{car['id']}: {car['display_name']} ({body}), ratings {car['ratings']}")
        print(f"Prepared {counts['archive_entries']} archive entries and {counts['loose_files']} loose assets.")
        if args.apply:
            changed = apply_plan(game_dir, state, originals, output, config)
            print(f"Installed; {changed} files changed. Restart the game to load the catalog.")
        else:
            print(f"Preview only. Use --apply to install, then edit {args.config} and --apply again to change stats.")
        return 0
    except (ValueError, OSError, KeyError, zipfile.BadZipFile, UnicodeError) as error:
        print(f"Car mods: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
