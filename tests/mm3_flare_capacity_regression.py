"""Check native flare capacity without running the game."""
from pathlib import Path
import os
import subprocess
import tempfile
import cc

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="mm3-flare-") as temporary:
    folder = Path(temporary)
    executable = folder / ("contract" + cc.exe_suffix())
    subprocess.run(cc.compile_command(
        [ROOT / "tests/mm3_draw_distance_contract.c"], executable,
        includes=[ROOT / "tools/xboxrecomp/templates/runtime", ROOT / "tools/xboxrecomp/src", ROOT / "tools/xboxrecomp/src/kernel", ROOT / "tools/xboxrecomp/src/video", ROOT / "tools/xboxrecomp/include"],
        warnings="none", obj=folder / "contract.obj", gcc_extra=["-lm"]), check=True, cwd=folder)
    for draw, view, expected in [(None, None, 1024), ("1", "1", 256), ("1", "2", 512),
                                 ("2", "4", 1024), ("4", "2", 1024),
                                 ("4", "4", 1024), ("1", "1.5", 384),
                                 ("1.5", "1", 384), ("nan", "1", 1024),
                                 ("1", "0", 512)]:
        env = dict(os.environ)
        for name, value in [("RECOMP_DRAW_DISTANCE", draw), ("RECOMP_VIEW_DISTANCE", view)]:
            env.pop(name, None)
            if value is not None:
                env[name] = value
        subprocess.run([str(executable), str(expected)], env=env, check=True, cwd=folder)
print("PASS: flare capacity, mod gate, caller isolation and guest ABI")
