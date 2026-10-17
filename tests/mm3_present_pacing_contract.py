"""Compile real flip/present/tick bodies with deterministic graphics mocks.

Run inside `python scripts/devenv.py --shell` with tools/venv/Scripts/python.exe. No GPU,
game process, game assets, native waits or saved profile are needed.
"""
from pathlib import Path
import re
import subprocess
import tempfile

import cc

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
    cc.require_compiler()
    source = (ROOT / "tools/xboxrecomp/src/video/nv2a_d3d11.c").read_text()
    with tempfile.TemporaryDirectory(prefix="mm3-present-pacing-") as folder:
        folder = Path(folder)
        (folder / "production_pacing.inc").write_text(production_bodies(source))
        executable = folder / f"contract{cc.exe_suffix()}"
        subprocess.run(cc.compile_command(
            [ROOT / "tests/mm3_present_pacing_contract.c"], executable,
            includes=[folder], defines=["_CRT_SECURE_NO_WARNINGS"], std="c11", optimize="none",
            warnings="high", werror=True, obj=folder / "contract.obj"), check=True, cwd=folder)
        for policy in ("before", "after"):
            subprocess.run([str(executable), policy], check=True)


if __name__ == "__main__":
    main()
