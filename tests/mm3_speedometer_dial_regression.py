"""Run the production configurable HUD dial offline, with native drawing mocked.

Run inside `python scripts/devenv.py --shell`. Compilation uses the project's real guest
register/memory header. A temporary fixture contains the settings sidecar;
no game, GPU, archives or saved profile is opened or changed.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile

import cc

ROOT = Path(__file__).resolve().parents[1]


def main():
    cc.require_compiler()
    with tempfile.TemporaryDirectory(prefix="mm3-speedometer-dial-") as temporary:
        folder = Path(temporary)
        shutil.copyfile(ROOT / "src/mm3_recomp_types.h", folder / "recomp_types.h")
        shutil.copyfile(ROOT / "tools/xboxrecomp/templates/runtime/recomp_types.h", folder / "recomp_base.h")
        executable = folder / f"contract{cc.exe_suffix()}"
        subprocess.run(cc.compile_command(
            [ROOT / "tests/mm3_speedometer_dial_contract.c"], executable,
            includes=[folder, ROOT / "tools/xboxrecomp/include"], std="c11", optimize="none",
            warnings="normal", werror=True, obj=folder / "contract.obj"), check=True, cwd=folder)
        subprocess.run([str(executable), "missing"], check=True, cwd=folder)
        data = folder / "game_files/Data"
        data.mkdir(parents=True)
        settings = data / "mm3_speedometers.ini"
        settings.write_text(
            "# alias max_kmh max_mph\n"
            "KoEnIgSeGg 800 500\n"
            "Viper 650 404\n"
            "Lotus 500 311\n"
            "AudiTT 1200 750\n"
            "# Old installer entries must never replace added-car retail HUDs.\n"
            "Panoz 650 404\n"
            "McLaren_F1 800 500\n"
            "Form1 800 500\n"
            "Ford_GTcar 800 500\n"
            "Senna_Road 800 500\n"
            "Senna_GTR1 800 500\n"
            "FXXK_Racer 800 500\n"
            "Regera_GT1 800 500\n"
            "AventadorX 800 500\n"
            "MRly 500 311\n"
            "ThisAliasIsLongerThan15 1200 750\n"
            "Avant nan 500\n"
            "Avalanche 800 inf\n"
            "Corvette 1201 500\n"
            "ChevySSR 99 500\n"
            "Crossfire 800 751\n"
            "NewBeetle 800\n", encoding="ascii")
        subprocess.run([str(executable), "configured"], check=True, cwd=folder)
        settings.write_text("Koenigsegg nan 500\nViper 800 751\n", encoding="ascii")
        subprocess.run([str(executable), "invalid"], check=True, cwd=folder)
    print("PASS: added-car retail HUD fallback with legacy settings, unchanged original overrides, parser, native quad ABI and guest state")


if __name__ == "__main__":
    main()
