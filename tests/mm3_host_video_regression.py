"""Run game-owned host-video policy with mocked playback and waits."""
from pathlib import Path
import os
import subprocess
import tempfile
import cc
ROOT = Path(__file__).resolve().parents[1]
def main():
    cc.require_compiler()
    with tempfile.TemporaryDirectory(prefix="mm3-host-video-") as tmp:
        folder=Path(tmp); exe=folder/("contract"+cc.exe_suffix())
        sources=[ROOT/"tests/mm3_host_video_contract.c"]
        if os.name != "nt": sources.append(ROOT/"tools/xboxrecomp/src/platform/win32_compat.c")
        subprocess.run(cc.compile_command(sources,exe,
            includes=[ROOT/"tools/xboxrecomp/src",ROOT/"tools/xboxrecomp/src/kernel"],
            defines=["_CRT_SECURE_NO_WARNINGS"],std="c11",optimize="none",warnings="high",
            werror=True,obj=folder/"contract.obj"),cwd=folder,check=True)
        subprocess.run([str(exe)],check=True)
if __name__=="__main__": main()
