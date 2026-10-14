"""Compile and run the shared production profile helper with real environment variables."""
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    if not shutil.which("cl"):
        raise SystemExit("Run scripts/enter-env.ps1 first so cl.exe is available.")
    with tempfile.TemporaryDirectory(prefix="mm3-performance-profile-") as temporary:
        folder = Path(temporary)
        executable = folder / "contract.exe"
        subprocess.run([
            "cl", "/nologo", "/W4", "/WX", "/Od", "/std:c11", "/D_CRT_SECURE_NO_WARNINGS",
            "/I" + str(ROOT / "src"),
            str(ROOT / "tests/mm3_performance_profile_contract.c"),
            "/Fo:" + str(folder / "contract.obj"), "/Fe:" + str(executable),
        ], check=True, cwd=folder)
        subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == "__main__":
    main()
