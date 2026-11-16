"""Exercise production gameplay mod hooks; optionally validate the local mission."""
from pathlib import Path
import subprocess
import sys
import tempfile
import zipfile
import cc

ROOT = Path(__file__).resolve().parents[1]


def check_lua(original, patched):
    """Optional --lua: execute both scripts with Lupa's Lua 5.1 runtime."""
    from lupa.lua51 import LuaRuntime
    prelude = """
    end_slide = {}; current_target = {1, 0}; timers = {}; score = 0
    function noop(...) end
    gameLoadScript = noop; playerDisableZone = noop; playerForceBrake = noop
    playerEnableZone = noop; objectCreate = noop; objectSetPos = noop
    objectSetParam = noop; objectEnable = noop; playerEnableWayPoint = noop
    playerBindToObject = noop; soundPlayVoice = noop
    function playerId(...) return 2 end
    function isPlayerSlowerThan(...) return stopped end
    function timerRemove(id) timers[id] = nil end
    function timerCreate(id, delay, callback) timers[id] = callback end
    function playerAddScore(...) score = score + 1 end
    function objectGetPos(...) return 0 end
    function objectGetParam(...) return 0 end
    function randomId(...) return 0 end
    strsub = string.sub
    function fire(id)
        local callback = timers[id]; assert(callback); timers[id] = nil; callback()
    end
    """
    for source, corrected in ((original, False), (patched, True)):
        for mode in ("stopped", "poll", "fallback"):
            lua = LuaRuntime()
            lua.execute(prelude)
            lua.execute(source.decode("latin1"))
            g = lua.globals()
            g.stopped = int(mode == "stopped")
            g.zone("ZN-0", "Comp--0")
            if mode == "poll":
                g.stopped = 1
                g.fire("wait_for_competitor_to_stop")
            if g.timers["competitor_stop_fallback"] is not None:
                g.fire("competitor_stop_fallback")
            expected = 2 if mode == "stopped" and not corrected else 1
            assert g.score == expected and g.current_target[2] == expected
    print("Lua execution: reproduced retail double advance; production patch awards once in all three cases")


def main():
    cc.require_compiler()
    with tempfile.TemporaryDirectory(prefix="mm3-gameplay-fixes-") as temporary:
        folder = Path(temporary)
        (folder / "recomp_types.h").write_text(
            "#define MEM32(a) (*(uint32_t*)(memory+(a)))\n"
            "#define MEM8(a) memory[(a)]\n"
            "#define MEMF(a) (*(float*)(memory+(a)))\n"
            "#define XBOX_PTR(a) (memory+(a))\n")
        (folder / "xbox_memory_layout.h").write_text(
            "int xbox_GuestMapped(uint32_t, uint32_t);\n")
        executable = folder / f"contract{cc.exe_suffix()}"
        subprocess.run(cc.compile_command(
            [ROOT / "tests/mm3_gameplay_fixes_contract.c"], executable,
            includes=[folder], defines=["_CRT_SECURE_NO_WARNINGS"],
            std="c11", warnings="high", werror=True), check=True, cwd=folder)
        subprocess.run([str(executable)], check=True)
        archive = ROOT / "game_files/Data/Data_hd.zip"
        if archive.exists():
            with zipfile.ZipFile(archive) as game:
                original = game.read("Scripts/Washington/Career/01/00Logic.lua")
            source, patched = folder / "mission.lua", folder / "patched.lua"
            source.write_bytes(original)
            subprocess.run([str(executable), str(source), str(patched)], check=True)
            result = patched.read_bytes()
            check = b"waitForCompetitorToStop()"
            timer = b'timerCreate("competitor_stop_fallback", comp_stop_fallback_time, competitorStopFallback)'
            start = original.index(check)
            end = original.index(timer, start) + len(timer)
            gap = original[start + len(check):end - len(timer)]
            assert result == original[:start] + timer + gap + check + original[end:]
            print("Retail mission: only the two adjacent statements changed; length preserved")
            if "--lua" in sys.argv:
                check_lua(original, result)


if __name__ == "__main__":
    main()
