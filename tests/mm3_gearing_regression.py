"""Compile retail gearbox consumers against every active factory preset.

Run after scripts/enter-env.ps1. No game process, save, GPU or driving input
is required. Controlled velocity tests gearbox decisions, not road top speed.
The factory engine hook has its own integration contract; this suite verifies
the retail consumers still support the full one-to-seven-gear preset range.
"""
import importlib.util
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
ACTIVE_IDS = {9001, 9004, 9005, 9006, 9009, 9010, 9011, 9012, 9013, 9014}


def generated_function(name):
    for path in (ROOT / "src/recomp/gen").glob("*.c"):
        text = path.read_text()
        match = re.search(r"^void " + name + r"\(void\)\n\{.*?^\}", text, re.M | re.S)
        if match:
            return match.group()
    raise ValueError(f"Missing generated {name}")


def case_definitions():
    config = json.loads((ROOT / "tools/car-mods/cars.json").read_text())
    spec = importlib.util.spec_from_file_location(
        "factory_specs", ROOT / "tools/car-mods/factory_specs.py")
    factory_specs = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(factory_specs)
    config = factory_specs.apply_presets(config)
    ids = [car["id"] for car in config["cars"]]
    if len(ids) != len(ACTIVE_IDS) or set(ids) != ACTIVE_IDS:
        raise ValueError("Expected each of the ten active custom cars exactly once")
    definitions = []
    gear_counts = set()
    # TurboSize selects the native peak-RPM field. Explicit preset overrides
    # win; otherwise these values come from the retail donor tweak files.
    turbo_sizes = {"mini": 0.0, "Viper": 0.0, "Lotus": 0.5, "Koenigsegg": 1.0}
    def float_literal(value):
        return repr(float(value)) + "f"
    for car in config["cars"]:
        physics = car["physics"]
        ratios = physics["GearRatioVector"]
        gear_counts.add(len(ratios) - 1)
        if not 2 <= len(ratios) <= 8:
            raise ValueError(f"{car['alias']} requires reverse plus one to seven gears")
        values = [len(ratios) - 1, "{" + ",".join(float_literal(v) for v in ratios) + "}",
                  physics.get("WheelRadius", 0.34), physics.get("WheelRatio", 4.0),
                  physics.get("MaxTorqueRPM", 6500), physics.get("MaxTurboTorqueRPM", 6750),
                  physics.get("TurboSize", turbo_sizes[car["donor"]]),
                  physics.get("CutRPM", 8000),
                  physics.get("MinRPMGearChange", 4000),
                  physics.get("GearStatusDownGearFactor", 0.75), physics["TimeChangeGear"]]
        definitions.append("    {" + json.dumps(car["display_name"]) + "," +
                           str(values[0]) + "," + values[1] + "," +
                           ",".join(float_literal(v) for v in values[2:]) + "}")
    if not {1, 7} <= gear_counts:
        raise ValueError("Factory cases must exercise both single-speed and seven-speed cars")
    return "static const CarCase cars[] = {\n" + ",\n".join(definitions) + "\n};\n"


def threshold_builder_tail(text):
    header = text[:text.index("loc_001CF7B4:")]
    tail = text[text.index("loc_001CF8FC:"):]
    # This tail still pops the native function's EDI/ESI saves. Model the
    # entry saves and retain its real RET 4 and all threshold instructions.
    return header.replace("sub_001CF7B4", "contract_build_thresholds") + \
        "    PUSH32(esp, esi);\n    PUSH32(esp, edi);\n" + tail


def main():
    if not shutil.which("cl"):
        raise SystemExit("Run scripts/enter-env.ps1 first so cl.exe is available.")
    shifter = generated_function("sub_001CC5D1")
    builder = generated_function("sub_001CF7B4")
    with tempfile.TemporaryDirectory(prefix="mm3-gearing-") as folder:
        folder = Path(folder)
        (folder / "gearing_cases.inc").write_text(case_definitions())
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
