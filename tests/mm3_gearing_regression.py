"""Compile retail gearbox consumers against every active donor-based preset.

Run after scripts/enter-env.ps1. No game process, save, GPU or driving input
is required. Controlled velocity tests gearbox decisions, not road top speed.
The factory engine hook has its own integration contract; this suite verifies
the shipped cars retain their retail shift boundaries and timing.
"""
import argparse
import json
from pathlib import Path
import os
import re
import shutil
import subprocess
import struct
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
ACTIVE_IDS = {9004, 9005, 9006, 9009, 9010, 9011, 9012, 9013, 9014}


def generated_function(name):
    for path in (Path(os.environ.get("MM3_TEST_GENERATED_DIR", ROOT / "src/recomp/gen"))).glob("*.c"):
        text = path.read_text()
        match = re.search(r"^void " + name + r"\(void\)\n\{.*?^\}", text, re.M | re.S)
        if match:
            return match.group()
    raise ValueError(f"Missing generated {name}")


def imported_radius(car, config_path):
    """Use the same front-wheel node bounds that the native loader reads."""
    sys.path.insert(0, str(ROOT / "tools/car-tools/car-models"))
    from mm3_model import read_model
    package_path = (config_path.parent / car["model_package"]).resolve()
    package = json.loads((package_path / "package.json").read_text())
    asset = next(asset for asset in package["files"]
                 if asset["archive"] == "loose" and asset["path"].casefold() ==
                 f"data/shared/cars/{car['alias']}/body.cmp".casefold())
    model = read_model((package_path / asset["source"]).read_bytes())
    wheel = next(node for _, node in model.root.walk()
                 if node.kind == 1 and node.name == "tireFrontLeft")
    bounds = struct.unpack_from("<6f", wheel.prefix, wheel.bounds_offset)
    return (bounds[4] - bounds[1]) / 2


def case_definitions(config_path=ROOT / "tools/car-tools/car-mods/cars.json"):
    config = json.loads(config_path.read_text())
    ids = [car["id"] for car in config["cars"]]
    if len(ids) != len(ACTIVE_IDS) or set(ids) != ACTIVE_IDS:
        raise ValueError("Expected each of the nine active custom cars exactly once")
    definitions = []
    gear_counts = set()
    # Read actual retail values rather than inventing fallback gearbox values.
    # The installer retains this pristine backup after its first apply.
    pristine = ROOT / "game_files/.mm3-car-mods/Data_hd.zip"
    archive_path = pristine if pristine.is_file() else ROOT / "game_files/Data/Data_hd.zip"
    with zipfile.ZipFile(archive_path) as archive:
        donor_physics = {}
        for donor in {car["donor"] for car in config["cars"]}:
            name = next(name for name in archive.namelist()
                        if name.casefold() == f"cars/tweak/{donor}.txt".casefold())
            text = archive.read(name).decode("latin1")
            body = re.search(r"Begin\(CarBody\)(.*?)End\(CarBody\)", text, re.S).group(1)
            donor_physics[donor] = {
                match.group(1): [float(value) for value in match.group(2).split()]
                for match in re.finditer(r"^\s*(\w+)[ \t]+([^\r\n]+)", body, re.M)
                if match.group(1) in {
                    "GearRatioVector", "WheelRadius", "WheelRatio", "MaxTorqueRPM",
                    "MaxTurboTorqueRPM", "TurboSize", "CutRPM", "MinRPMGearChange",
                    "GearStatusDownGearFactor", "TimeChangeGear"}
            }
    def float_literal(value):
        return repr(float(value)) + "f"
    for car in config["cars"]:
        if car.get("engine_profile") == "factory":
            raise ValueError(f"{car['alias']} must retain the default donor engine")
        if set(car["physics"]) != {"Mass", "TorqueScale"}:
            raise ValueError(f"{car['alias']} defaults must only change weight and power")
        physics = donor_physics[car["donor"]]
        ratios = physics["GearRatioVector"]
        gear_counts.add(len(ratios) - 1)
        if not 2 <= len(ratios) <= 8:
            raise ValueError(f"{car['alias']} requires reverse plus one to seven gears")
        values = [len(ratios) - 1, "{" + ",".join(float_literal(v) for v in ratios) + "}",
                  *[physics[key][0] for key in (
                      "WheelRadius", "WheelRatio", "MaxTorqueRPM", "MaxTurboTorqueRPM",
                      "TurboSize", "CutRPM", "MinRPMGearChange",
                      "GearStatusDownGearFactor", "TimeChangeGear")]]
        if car.get("model_package", "").strip():
            values[2] = imported_radius(car, config_path)
        definitions.append("    {" + json.dumps(car["display_name"]) + "," +
                           str(values[0]) + "," + values[1] + "," +
                           ",".join(float_literal(v) for v in values[2:]) + "}")
    if gear_counts != {5, 6}:
        raise ValueError("Expected the retail five- and six-speed donor gearboxes")
    return "static const CarCase cars[] = {\n" + ",\n".join(definitions) + "\n};\n"


def threshold_builder_tail(text):
    header = text[:text.index("loc_001CF7B4:")]
    tail = text[text.index("loc_001CF8FC:"):]
    # This tail still pops the native function's EDI/ESI saves. Model the
    # entry saves and retain its real RET 4 and all threshold instructions.
    return header.replace("sub_001CF7B4", "contract_build_thresholds") + \
        "    PUSH32(esp, esi);\n    PUSH32(esp, edi);\n" + tail


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path, default=ROOT / "tools/car-tools/car-mods/cars.json")
    args = parser.parse_args()
    if not shutil.which("cl"):
        raise SystemExit("Run scripts/enter-env.ps1 first so cl.exe is available.")
    shifter = generated_function("sub_001CC5D1")
    builder = generated_function("sub_001CF7B4")
    with tempfile.TemporaryDirectory(prefix="mm3-gearing-") as folder:
        folder = Path(folder)
        (folder / "gearing_cases.inc").write_text(case_definitions(args.config.resolve()))
        (folder / "production_gearing.inc").write_text(
            shifter + "\n" + threshold_builder_tail(builder))
        shutil.copyfile(ROOT / "src/mm3_recomp_types.h", folder / "recomp_types.h")
        executable = folder / "contract.exe"
        subprocess.run(["cl", "/nologo", "/W3", "/WX", "/wd4102", "/Od", "/std:c11",
                        "/I" + str(folder), "/I" + str(ROOT / "tools/xboxrecomp/include"),
                        str(ROOT / "tests/mm3_gearing_contract.c"),
                        "/Fo:" + str(folder / "contract.obj"), "/Fe:" + str(executable)],
                       check=True, cwd=folder)
        subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
