"""Where does the game thread spend its time? Samples it for a while in a live Checkpoint race.

Usage: MM3_SAVE_TEMPLATE=<copied save> python tools/game_test/profile_thread.py [seconds] [fps_limit]
Uses the debug pipe's `profile` command (src/mods/teleport.c): the thread is sampled every
millisecond and the share inside the executable's translated code, in ntdll (blocked in a wait)
and elsewhere is printed. An fps_limit of 0 (the default here) leaves the frame cap off.
The exe rvas can be symbolised with dbghelp against the build's pdb.
"""
import collections, os, sys, tempfile, time
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))
from tools.game_test.mission_win import ALL_MODS, CHECKPOINT
from tools.game_test.run import Session


def summarise(path):
    lines = [l.split() for l in open(path) if l.strip()]
    shares = collections.Counter(l[0] for l in lines)
    total = len(lines)
    print("samples:", total)
    for name, n in shares.most_common(6):
        print("  %-20s %5.1f%%" % (name, 100.0 * n / total))
    hot = collections.Counter(l[1] for l in lines if l[0] == "exe")
    print("hottest exe rvas:", ", ".join("%s %.1f%%" % (r, 100.0 * n / total) for r, n in hot.most_common(5)))


def main(seconds=20, fps_limit="0"):
    out = os.path.join(tempfile.gettempdir(), "mm3-profile.txt").replace("\\", "/")
    env = dict(ALL_MODS, RECOMP_FPS_LIMIT=str(fps_limit))
    with Session(os.path.join(os.path.dirname(__file__), "..", "..", "build/RelWithDebInfo/mm3_recomp.exe"),
                 os.path.join(os.path.dirname(__file__), "..", "..", "game_files"),
                 save_template=os.environ["MM3_SAVE_TEMPLATE"], timeout=seconds + 240, env=env) as g:
        g.run({"steps": CHECKPOINT})
        g.press("rt", 60000, wait_release=False)
        time.sleep(12)                               # past the countdown
        print(g.request("profile %d %s" % (seconds, out)), flush=True)
        time.sleep(seconds + 4)
    summarise(out)


if __name__ == "__main__":
    main(int(sys.argv[1]) if len(sys.argv) > 1 else 20, sys.argv[2] if len(sys.argv) > 2 else "0")
