"""Run title overlay placement and route upload contracts without a GPU."""
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    if not shutil.which("cl"):
        raise SystemExit("Run scripts/enter-env.ps1 first so cl.exe is available.")
    with tempfile.TemporaryDirectory(prefix="mm3-overlay-") as temporary:
        folder = Path(temporary)
        executable = folder / "contract.exe"
        subprocess.run([
            "cl", "/nologo", "/W3", "/WX", "/O2", "/std:c11",
            "/I" + str(ROOT / "src/mods"),
            "/I" + str(ROOT / "tools/xboxrecomp/src/video"),
            str(ROOT / "tests/mm3_overlay_contract.c"),
            "/Fo:" + str(folder / "contract.obj"), "/Fe:" + str(executable),
        ], check=True, cwd=folder)
        subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == "__main__":
    main()
