"""Compile and run the real framebuffer close-message handling on Windows."""
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    if not shutil.which("cl"):
        raise SystemExit("Run scripts/enter-env.ps1 first so cl.exe is available.")
    with tempfile.TemporaryDirectory(prefix="mm3-window-close-") as temporary:
        folder = Path(temporary)
        subprocess.run(["cl", "/nologo", "/W3", "/WX", "/O2", "/std:c11",
                        "/D_CRT_SECURE_NO_WARNINGS",
                        "/I" + str(ROOT / "tools/xboxrecomp/src"),
                        str(ROOT / "tests/mm3_window_close_contract.c"),
                        "/Fo:" + str(folder / "contract.obj"),
                        "/Fe:" + str(folder / "contract.exe"), "user32.lib"],
                       cwd=folder, check=True)
        subprocess.run([str(folder / "contract.exe")], cwd=folder, check=True)


if __name__ == "__main__":
    main()
