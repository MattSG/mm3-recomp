"""The language chosen in Game Setup survives a restart (src/mm3_language.c).

Run inside `python scripts/devenv.py --shell`. Uses the real guest register/memory header
and a stand-in for the kernel's path translation; does not launch the game.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

import cc

ROOT = Path(__file__).resolve().parents[1]


def main():
    cc.require_compiler()
    with tempfile.TemporaryDirectory(prefix="mm3-language-") as temporary:
        folder = Path(temporary)
        saves = folder / "UDATA"
        saves.mkdir()
        shutil.copyfile(ROOT / "src/mm3_recomp_types.h", folder / "recomp_types.h")
        shutil.copyfile(ROOT / "tools/xboxrecomp/templates/runtime/recomp_types.h", folder / "recomp_base.h")
        executable = folder / f"contract{cc.exe_suffix()}"
        subprocess.run(cc.compile_command(
            [ROOT / "tests/mm3_language_contract.c", ROOT / "src/mm3_language.c", ROOT / "src/feature_switch.c"],
            executable, includes=[folder, ROOT / "tools/xboxrecomp/include", ROOT / "tools/xboxrecomp/src",
                                  ROOT / "tools/xboxrecomp/src/kernel"],
            defines=["_CRT_SECURE_NO_WARNINGS"], std="c11", optimize="none", warnings="normal",
            werror=True), check=True, cwd=folder)

        def run(mode, **env):
            subprocess.run([str(executable), mode, str(saves)], check=True, cwd=folder,
                           env={**os.environ, **env})

        env = {"MM3_REMEMBER_LANGUAGE": ""}
        run("first", **env)
        run("restart", **env)
        (saves / "mm3_language.ini").write_text("language=banana\n")
        run("garbage", **env)
        (saves / "mm3_language.ini").write_text("language=2\n")
        run("disabled", MM3_REMEMBER_LANGUAGE="0")


if __name__ == "__main__":
    main()
