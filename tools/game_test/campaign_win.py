"""Play through a city's career with the race scripts' own debug win (hidden, one game).

Each career job is a Lua script in Data/Data_hd.zip (Scripts/<City>/Career/NN/MMLogic.lua). logic_general.lua
defines forceWin() (DEBUG): the job ends won through the game's own path (career_result 1, raceFinish for every
human and AI, the debrief, the career save). The `forcewin` debug-pipe command runs it on the game thread
(src/mods/teleport.c). This script starts the Paris career, and for every job in order: waits for the race,
checks the player spawned at that job's spawn_location (jobs are told apart by it; a few share one, so the
expected order decides), waits until the car moves (wins in the countdown do not count), forces the win,
captures the result and takes NEXT RACE from the post-race menu.

  python tools/game_test/campaign_win.py [--city Paris] [--jobs N] [--save TEMPLATE] [--exe EXE] [--game DIR] [--out DIR]

Exit 0 when every job loaded where expected and was won. Captures (capture-NNN.bmp) in the run directory show each
result screen; look at them.
"""
import argparse, json, math, os, re, sys, time, zipfile
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))
from tools.game_test.run import Session
from tools.game_test.mission_win import ALL_MODS, MENUS

ROOT = os.path.join(os.path.dirname(__file__), "..", "..")


def career_jobs(game, city):
    """[(chapter/job, title, [(x, z) of each spawn_location])] in career order, from the job scripts."""
    jobs = []
    with zipfile.ZipFile(os.path.join(game, "Data", "Data_hd.zip")) as z:
        names = sorted(n for n in z.namelist() if re.fullmatch(rf"Scripts/{city}/Career/\d\d/\d\dLogic\.lua", n))
        for n in names:
            s = z.read(n).decode("latin-1")
            spawns = [(float(x), float(z)) for x, z in
                      re.findall(r"spawn_location\[\d+\]\s*=\s*\{\s*([-\d.]+)\s*,\s*[-\d.]+\s*,\s*([-\d.]+)", s)]
            t = re.search(r'\* "(.*?)"', s)
            jobs.append((n.split("/")[3] + "/" + n.split("/")[4][:2], t.group(1) if t else "?", spawns))
    return jobs


def share(bmp, box, test):
    """The share of pixels in box (fractions of the frame) that pass test(r, g, b)."""
    from PIL import Image
    im = Image.open(bmp).convert("RGB")
    w, h = im.size
    raw = im.crop((int(w * box[0]), int(h * box[1]), int(w * box[2]), int(h * box[3]))).tobytes()
    return sum(map(test, raw[0::3], raw[1::3], raw[2::3])) / max(len(raw) // 3, 1)


def career_menu(bmp):
    """The Work Undercover menus (occupations, then their jobs): orange top right (57%; the car screen and the
    race have none there)."""
    return share(bmp, (.80, .20, .88, .40), lambda r, g, b: r > 180 and 100 < g < 200 and b < 60) > .4


def notice(bmp):
    """An unlock notice ("Press A to continue") over the post-race menu: its box fills 81% of this patch with
    saturated blue; "Are you sure?" 47%, the career menus under 60%."""
    return share(bmp, (.25, .35, .33, .60), lambda r, g, b: b > 90 and b > r + 40) > .7 and not career_menu(bmp)


def next_chapter(g, chapter):
    """From the post-race menu after a chapter's last job (NEXT RACE greyed): EXIT TO MENU, confirm, past the
    cut-scene to Work Undercover; the occupation `chapter` rows down (one per chapter), its first open job, the
    car as offered. True once the Work Undercover menu was reached."""
    g.press("down", after=1); g.press("a", after=1); g.press("left", after=1); g.press("a", after=4)
    t_end, k = time.time() + 90, 0
    while time.time() < t_end:
        if career_menu(g.capture()["path"]): break
        k += 1                                            # Start skips most cut-scenes, A the briefing call
        g.press("start" if k % 2 else "a", after=4)
    else:
        return False
    for _ in range(chapter): g.press("down", after=1)
    g.press("a", after=3)                                 # the occupation: its job list
    g.press("a", after=4)                                 # the first open job: the car
    g.press("a", after=2)                                 # the car as offered: loading
    return True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--city", default="Paris")
    ap.add_argument("--jobs", type=int, default=0, help="stop after this many (0: all)")
    ap.add_argument("--exe", default=os.path.join(ROOT, "build/RelWithDebInfo/mm3_recomp.exe"))
    ap.add_argument("--game", default=os.path.join(ROOT, "game_files"))
    ap.add_argument("--save", default=os.environ.get("MM3_SAVE_TEMPLATE"))
    ap.add_argument("--out", default=None)
    a = ap.parse_args()
    start = MENUS["undercover"][:-1]                         # Work Undercover, Paris (Left on the city choice)
    if a.city == "Washington":
        start = [s for s in start if s.get("buttons") != "left"]
    jobs = career_jobs(a.game, a.city)
    if a.jobs: jobs = jobs[:a.jobs]
    won = 0
    with Session(a.exe, a.game, save_template=a.save, output_root=a.out, timeout=300 + 120 * len(jobs),
                 env=ALL_MODS) as g:
        g.run({"steps": start})                              # to the first job's loading; waited for below
        for i, (jid, title, spawn) in enumerate(jobs):
            try:
                g.wait_player(timeout=120, advance="start")  # Start skips intro slides and cut-scenes
            except TimeoutError:
                g.capture()
                print(f"job {jid} {title!r}: never started (capture taken)", flush=True)
                break
            p = json.loads(g.request("position"))
            where = (p["x"], p["z"])
            # The player takes one of the job's spawn locations (usually the first; "The younger brother" the
            # second, where the AI starts in "The older brother").
            off = min((math.dist(where, p) for p in spawn), default=-1)
            print(f"job {jid} {title!r}: spawned at {where[0]:.0f},{where[1]:.0f} ({off:.0f} m from its spawn)", flush=True)
            if spawn and off > 40:
                print("  not the expected job; stopping", flush=True)
                break
            g.press("rt", 8000, wait_release=False)
            t_end, p0 = time.time() + 40, where
            while time.time() < t_end:                       # live once the car moves (after the countdown)
                time.sleep(0.5)
                q = json.loads(g.request("position"))
                if math.dist((q["x"], q["z"]), p0) > 3: break
            r = json.loads(g.request("forcewin", timeout=5))
            print(f"  forcewin: {r['result']}", flush=True)
            if r["result"] != "ran": break
            time.sleep(8)
            for _ in range(4):                               # unlock notices ("Press A to continue") first
                if not notice(g.capture()["path"]): break
                print("  notice dismissed", flush=True)
                g.press("a", after=3)
            won += 1
            if i + 1 < len(jobs) and jobs[i + 1][0][:2] != jid[:2]:
                if not next_chapter(g, int(jobs[i + 1][0][:2])):
                    print("  the career menu never came; stopping", flush=True)
                    break
            elif i + 1 < len(jobs):
                g.press("down", after=1)                     # post-race menu: RESTART RACE, NEXT RACE, ...
                g.press("a", after=2)
        print(f"{won}/{len(jobs)} jobs won; captures in {g.run_dir}", flush=True)
        g.result["status"] = "passed" if won == len(jobs) else "failed"
    return 0 if won == len(jobs) else 1


if __name__ == "__main__":
    sys.exit(main())
