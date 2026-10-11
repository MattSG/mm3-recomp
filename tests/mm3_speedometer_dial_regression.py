"""Run the production configurable HUD dial offline, with native drawing mocked.

Run after scripts/enter-env.ps1. Compilation uses the project's real guest
register/memory header. A temporary fixture contains the settings sidecar;
no game, GPU, archives or saved profile is opened or changed.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    if not shutil.which("cl"):
        raise SystemExit("Run scripts/enter-env.ps1 first so cl.exe is available.")
    with tempfile.TemporaryDirectory(prefix="mm3-speedometer-dial-") as temporary:
        folder = Path(temporary)
        shutil.copyfile(ROOT / "src/mm3_recomp_types.h", folder / "recomp_types.h")
        executable = folder / "contract.exe"
        subprocess.run([
            "cl", "/nologo", "/W3", "/WX", "/Od", "/std:c11",
            "/I" + str(folder),
            "/I" + str(ROOT / "tools/xboxrecomp/include"),
            str(ROOT / "tests/mm3_speedometer_dial_contract.c"),
            "/Fo:" + str(folder / "contract.obj"), "/Fe:" + str(executable),
        ], check=True, cwd=folder)
        subprocess.run([str(executable), "missing"], check=True, cwd=folder)
        data = folder / "game_files/Data"
        data.mkdir(parents=True)
        settings = data / "mm3_speedometers.ini"
        settings.write_text(
            "# alias max_kmh max_mph\n"
            "KoEnIgSeGg 800 500\n"
            "Panoz 650 404\n"
            "MRly 500 311\n"
            "ThisAliasIsLongerThan15 1200 750\n"
            "BadNan nan 500\n"
            "BadInfinity 800 inf\n"
            "TooLarge 1201 500\n"
            "TooSmall 99 500\n"
            "BadMph 800 751\n"
            "HalfRange 800\n", encoding="ascii")
        subprocess.run([str(executable), "configured"], check=True, cwd=folder)
        settings.write_text("Koenigsegg nan 500\nPanoz 800 751\n", encoding="ascii")
        subprocess.run([str(executable), "invalid"], check=True, cwd=folder)
    print("PASS: production dial parser, alias/units, fallbacks, calibrated labels/needle, native quad ABI and guest state")


if __name__ == "__main__":
    main()
