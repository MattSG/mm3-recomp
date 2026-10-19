"""Exercise the game-owned fog packet decoder, including interrupted streams."""
from pathlib import Path
import subprocess
import tempfile
import cc
ROOT = Path(__file__).resolve().parents[1]
def main():
    cc.require_compiler()
    with tempfile.TemporaryDirectory(prefix="mm3-render-command-") as folder:
        folder = Path(folder)
        exe = folder / ("contract" + cc.exe_suffix())
        subprocess.run(cc.compile_command([ROOT / "tests/mm3_render_commands_contract.c"], exe,
            includes=[ROOT / "tools/xboxrecomp/src/video"], std="c11", optimize="none",
            warnings="high", obj=folder / "contract.obj"), check=True, cwd=folder)
        subprocess.run([str(exe)], check=True)
if __name__ == "__main__": main()
