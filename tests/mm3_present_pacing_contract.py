"""Compile real flip/present/tick bodies with deterministic graphics mocks.

Run after scripts/enter-env.ps1 with tools/venv/Scripts/python.exe. No GPU,
game process, game assets, native waits or saved profile are needed.
"""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def production_bodies(source):
    pacing = re.search(r"typedef struct FlipPacing \{.*?^\} FlipPacing;", source, re.M | re.S)
    if pacing is None:
        raise ValueError("Missing production FlipPacing type")
    bodies = [pacing.group()]
    for name in ("frame_csv_present", "frame_csv", "pace_after_present", "pace_flip",
                 "present_surface", "nv2a_d3d_flip", "nv2a_d3d_tick"):
        match = re.search(r"^(?:static )?(?:int|void) " + name + r"\([^\n]*\)\n\{.*?^\}", source, re.M | re.S)
        if match is None:
            raise ValueError(f"Missing production {name} body")
        body = match.group()
        if name in ("frame_csv_present", "frame_csv"):
            # Event wrappers call these actual implementations, including the
            # CSV writer's clearing of per-flip pending state.
            body = body.replace("void " + name + "(", "void contract_" + name + "(", 1)
        bodies.append(body)
    return "\n\n".join(bodies)


def main():
    if not shutil.which("cl"):
        raise SystemExit("Run scripts/enter-env.ps1 first so cl.exe is available.")
    source = (ROOT / "tools/xboxrecomp/src/video/nv2a_d3d11.c").read_text()
    with tempfile.TemporaryDirectory(prefix="mm3-present-pacing-") as folder:
        folder = Path(folder)
        (folder / "production_pacing.inc").write_text(production_bodies(source))
        executable = folder / "contract.exe"
        subprocess.run(["cl", "/nologo", "/W4", "/WX", "/Od", "/std:c11",
                        "/D_CRT_SECURE_NO_WARNINGS", "/I" + str(folder),
                        str(ROOT / "tests/mm3_present_pacing_contract.c"),
                        "/Fo:" + str(folder / "contract.obj"), "/Fe:" + str(executable)],
                       check=True, cwd=folder)
        for policy in ("before", "after"):
            subprocess.run([str(executable), policy], check=True)


if __name__ == "__main__":
    main()
