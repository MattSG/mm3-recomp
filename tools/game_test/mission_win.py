"""Win a Checkpoint race or a Blitz run by teleporting through the native route, all mods on.

Usage: MM3_MISSION=checkpoint|blitz python tools/game_test/mission_win.py [exe] [assets] [save_template]
Work Undercover (a 'collect 5 boxes' mission) is not supported: its targets are not in the
`route` snapshot (empty in mode 0), so a win needs the box positions found first.
Reads the race's remaining checkpoints with the `route` pipe command (the list
shrinks as they are passed), teleports to the next one until the game refuses
the teleport (race over), then captures the results screen for inspection.
Exit 0 when every checkpoint was passed and the player update stopped.
"""
import json, os, sys, time
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


def main(exe, assets, save, mission="checkpoint"):
    with Session(exe, assets, save_template=save, timeout=420, env=ALL_MODS) as g:
        g.run({"steps": MENUS[mission]})
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
