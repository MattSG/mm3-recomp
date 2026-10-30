"""Win a Checkpoint race or a Blitz run by teleporting through the native route, all mods on.

Usage: MM3_MISSION=checkpoint|blitz|undercover python tools/game_test/mission_win.py [exe] [assets] [save_template]
Work Undercover (MM3_MISSION=undercover, Paris career 00/00 "Delivery by dawn") has no checkpoint
list; its Lua setup script (Data_hd.zip) places zones ZN-0..ZN-9 that must be entered in order.
Reads the race's remaining checkpoints with the `route` pipe command (the list
shrinks as they are passed), teleports to the next one until the game refuses
the teleport (race over), then captures the results screen for inspection.
Exit 0 when every checkpoint was passed and the player update stopped.
"""
import json, os, re, sys, time, zipfile
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))
from tools.game_test.run import Session

ALL_MODS = {k: "1" for k in (
    "MM3_DEBUG_OVERLAY", "MM3_MOD_WIDESCREEN", "MM3_MOD_DRAW_DISTANCE", "MM3_MOD_SKY", "MM3_MOD_MENU_MOUSE",
    "MM3_MOD_DEV_TOOLS", "MM3_MOD_COLLISION_FIX", "MM3_MOD_SKIP_INTRO", "MM3_MOD_FOV", "MM3_MOD_FRAMERATE",
    "MM3_MOD_TRANSLATION", "MM3_MOD_KEYBOARD_DRIVING", "MM3_MOD_FLARE_DEPTH")}
ALL_MODS.update(MM3_SMOOTH="0", RECOMP_FPS_LIMIT="60")

CHECKPOINT = [{"action": "wait_log", "text": "[MOD] skip_intro: intro", "timeout": 60}, {"action": "wait", "seconds": 30}] + [
    {"action": "press", "buttons": b, "after": t} for b, t in
    (("start", 3), ("a", 7), ("down", 3), ("a", 3), ("down", 1), ("down", 1), ("a", 3), ("a", 3), ("a", 3), ("a", 3), ("a", 2))] + [
    {"action": "wait_player", "timeout": 60, "advance": "start"}]

UNDERCOVER = [{"action": "wait_log", "text": "[MOD] skip_intro: intro", "timeout": 60}, {"action": "wait", "seconds": 40}, {"action": "press", "buttons": "start", "after": 2}, {"action": "press", "buttons": "a", "after": 2}, {"action": "press", "buttons": "a", "after": 4}, {"action": "press", "buttons": "left", "after": 1}, {"action": "press", "buttons": "a", "after": 3}, {"action": "press", "buttons": "a", "after": 15}, {"action": "press", "buttons": "a", "after": 20}, {"action": "press", "buttons": "start", "after": 5}, {"action": "wait_player", "timeout": 45, "advance": "start"}]
BLITZ = [{"action": "wait_log", "text": "[MOD] skip_intro: intro", "timeout": 60}, {"action": "wait", "seconds": 40}, {"action": "press", "buttons": "start", "after": 2}, {"action": "press", "buttons": "a", "after": 2}, {"action": "press", "buttons": "down", "after": 2}, {"action": "press", "buttons": "a", "after": 2}, {"action": "press", "buttons": "down", "after": 1}, {"action": "press", "buttons": "a", "after": 2}, {"action": "press", "buttons": "right", "after": 2}, {"action": "press", "buttons": "a", "after": 2}, {"action": "press", "buttons": "a", "after": 2}, {"action": "press", "buttons": "a", "after": 2}, {"action": "press", "buttons": "a"}, {"action": "wait_player", "timeout": 30, "advance": "start"}]
MENUS = {"checkpoint": CHECKPOINT, "undercover": UNDERCOVER, "blitz": BLITZ}


def route(g):
    return json.loads(g.request("route"))


def undercover_zones(assets, script="Scripts/Paris/Career/00/00Setup.lua"):
    """ZN-n positions of a career script, in the order the mission enables them."""
    with zipfile.ZipFile(os.path.join(assets, "Data", "Data_hd.zip")) as z:
        lua = z.read(script).decode("latin1")
    zones = {}
    for n, x, z_ in re.findall(r'objectCreate\(3,"ZN-(\d+)"\).*?objectSetPos\(([-\d.]+),[-\d.]+,([-\d.]+)\)', lua, re.S):
        zones.setdefault(int(n), (float(x), float(z_)))
    return [zones[i] for i in sorted(zones)]


def run_zones(g, zones):
    """Teleport into each zone in turn; a refused spot is retried a little to the side."""
    for i, (x, z) in enumerate(zones):
        for dx, dz in ((0, 0), (4, 0), (-4, 0), (0, 4), (0, -4), (8, 8), (-8, -8)) * 3:
            try:
                g.teleport(x + dx, z + dz)
                break
            except AssertionError as e:
                last = e
                time.sleep(1)
        else:
            print("zone %d (%.0f,%.0f) refused: %s" % (i, x, z, last))
            return i
        time.sleep(3)
        print("zone %d entered at (%.0f,%.0f)" % (i, x, z), flush=True)
    return len(zones)


def main(exe, assets, save, mission="checkpoint"):
    with Session(exe, assets, save_template=save, timeout=420, env=ALL_MODS) as g:
        g.run({"steps": MENUS[mission]})
        if mission == "undercover":
            time.sleep(8)                       # countdown
            zones = undercover_zones(assets)
            done = run_zones(g, zones)
            for _ in range(3):                  # end-of-mission scene, then the results
                time.sleep(10)
                g.capture()
            print("zones %d/%d, results capture in %s" % (done, len(zones), g.run_dir))
            return 0 if done == len(zones) else 1
        while route(g)["count"] == 0 or route(g)["current"] >= route(g)["count"]:
            time.sleep(1)                       # countdown: the list is not targeted yet
        g.press("rt", 60000, wait_release=False)
        t_end = time.time() + 60
        def pos():
            c = [x for x in json.loads(g.request("cars"))["cars"] if x["player"]]
            return (c[0]["x"], c[0]["z"]) if c else (0, 0)
        p0 = pos()
        while time.time() < t_end:              # teleports only count once the race is live
            time.sleep(0.5)
            p1 = pos()
            if abs(p1[0] - p0[0]) + abs(p1[1] - p0[1]) > 3:
                break
        total = route(g)["count"]
        passed = 0
        while True:
            r = route(g)
            x, z = r["points"][r["current"]]
            try:
                g.teleport(x, z)
            except AssertionError as e:
                print("teleport refused, race over:", e)
                break
            passed += 1
            t0 = time.time()
            while time.time() - t0 < 8 and route(g)["points"][0] == r["points"][0]:
                time.sleep(0.4)
            if passed > total + 2:
                break
        g.capture()
        print("checkpoints %d/%d, results capture in %s" % (passed, total, g.run_dir))
        return 0 if passed == total else 1


if __name__ == "__main__":
    mission = os.environ.get("MM3_MISSION", "checkpoint")
    a = sys.argv[1:] + [None] * 3
    root = os.path.join(os.path.dirname(__file__), "..", "..")
    sys.exit(main(a[0] or os.path.join(root, "build/RelWithDebInfo/mm3_recomp.exe"),
                  a[1] or os.path.join(root, "game_files"), a[2] or os.environ["MM3_SAVE_TEMPLATE"], mission))
