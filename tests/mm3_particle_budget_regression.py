"""Check upstream particle budgets and retained explicit overrides at native ABI."""
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    if not shutil.which("cl"):
        raise SystemExit("Run scripts/enter-env.ps1 first so cl.exe is available.")
    with tempfile.TemporaryDirectory(prefix="mm3-particle-budget-") as temporary:
        folder = Path(temporary)
        executable = folder / "contract.exe"
        subprocess.run([
            "cl", "/nologo", "/W3", "/WX", "/Od", "/std:c11", "/D_CRT_SECURE_NO_WARNINGS",
            "/I" + str(ROOT / "src/recomp/gen"),
            "/I" + str(ROOT / "tools/xboxrecomp/include"),
            str(ROOT / "tests/mm3_particle_budget_contract.c"),
            "/Fo:" + str(folder / "contract.obj"), "/Fe:" + str(executable),
        ], check=True, cwd=folder)
        for mode in ("default", "baseline", "explicit", "low", "title", "0", "256", "garbage", "1extra"):
            subprocess.run([str(executable), mode], check=True, cwd=folder)


if __name__ == "__main__":
    main()
