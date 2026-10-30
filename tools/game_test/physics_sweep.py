#!/usr/bin/env python3
"""Standardised accel/brake bench: one boot per setting, several identical trials.

  python tools/game_test/physics_sweep.py --exe X --game-dir G --save-template S
      [--runs 4] [--config SIM_HZ:FPS ...]      (FPS 0 = uncapped)

Cruise/Paris with traffic and pedestrians at their lowest ("Very Light": the menu
has no Off), then each trial teleports to the same straight (the Cruise spawn,
~180 units clear, no steering) and holds throttle for 3 s, then brake for 2.5 s. Speed
comes from timestamped positions; trials that see a second car, or travel past
SAFE_UNITS (the straight's end), are discarded rather than averaged in. Reports
peak speed, time to 10/20 units/s, brake time and brake distance per setting."""
import argparse, json, math, statistics, sys, tempfile, time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.game_test.run import Session  # noqa: E402

POSE = (1861.87, 630.421, math.atan2(-0.389089, 0.921185))
SAFE_UNITS = 165
DEBUG_CAPTURE = bool(__import__('os').environ.get('PHYS_CAPTURE'))
MENU = [{"action": "wait_log", "text": "[MOD] skip_intro: intro", "timeout": 60}, {"action": "wait", "seconds": 32}] + [
    {"action": "press", "buttons": b, "after": t} for b, t in
    (("start", 3), ("a", 7), ("down", 3), ("a", 3), ("a", 3), ("a", 3),
     ("left", 1), ("left", 1), ("left", 1), ("down", 1), ("left", 1), ("left", 1), ("left", 1),
     ("a", 3), ("a", 3), ("a", 3))] + [
    {"action": "wait_player", "timeout": 40, "advance": "start"},
    {"action": "peek", "address": 4226076, "size": 4, "expected": "01000000"}]  # native mode 1 = Cruise


def trial(g):
    g.teleport(*POSE)
    g.wait(2.5)
    pts, others = [], False
    t0 = time.monotonic()
    g.press("rt", 3000, wait_release=False)
    braked = captured = False
    while True:
        t = time.monotonic() - t0
        cars = g.query("cars", 1)["cars"]
        p = next(c for c in cars if c["player"])
        near = [c for c in cars if not c['player'] and math.hypot(c['x'] - p['x'], c['z'] - p['z']) < 150]
        others |= bool(near)
        pts.append((t, p["x"], p["z"]))
        if DEBUG_CAPTURE and not captured and t >= 2.4:
            g.capture(); captured = True
        if not braked and t >= 3.4:
            g.press("lt", 2500, wait_release=False)
            braked = True
        if t >= 6.4:
            return pts, others
        g.wait(0.03)


def turn_trial(g):
    """Throttle for 2 s, then throttle with the stick held left for 3 s: how far the heading turns
    and how far the car travels in that time (the two depend on handling, not on the frame rate)."""
    g.teleport(*POSE)
    g.wait(2.5)
    g.press("rt", 2000)
    t0 = time.monotonic()
    g.press("rt+lleft", 3000, wait_release=False)
    first = last = None
    while time.monotonic() - t0 < 3.0:
        c = next(c for c in g.query("cars", 1)["cars"] if c["player"])
        a = math.degrees(math.atan2(c["fx"], c["fz"]))
        first = first or (a, c["x"], c["z"])
        last = (a, c["x"], c["z"])
        g.wait(0.05)
    turn = (last[0] - first[0] + 540) % 360 - 180
    return {"turn": turn, "path": math.hypot(last[1] - first[1], last[2] - first[2])}


def wall_trial(g):
    """Full throttle down the straight into its end (~180 units): how far the car gets."""
    g.teleport(*POSE)
    g.wait(2.5)
    pts = []
    t0 = time.monotonic()
    g.press("rt", 9000, wait_release=False)
    while time.monotonic() - t0 < 9.5:
        c = next(c for c in g.query("cars", 1)["cars"] if c["player"])
        pts.append((time.monotonic() - t0, c["x"], c["z"]))
        g.wait(0.05)
    x0, z0 = pts[0][1:]
    d = [math.hypot(x - x0, z - z0) for _, x, z in pts]
    return {"max": max(d), "end": d[-1]}


def metrics(pts):
    x0, z0 = pts[0][1:]
    sp, j = [], 0
    for i, (t, x, z) in enumerate(pts):
        while t - pts[j][0] > 0.2:
            j += 1
        dt = t - pts[j][0]
        if dt > 0.1:
            sp.append((t, math.hypot(x - pts[j][1], z - pts[j][2]) / dt, math.hypot(x - x0, z - z0)))
    start = next((t for t, s, d in sp if s > 0.5), None)
    if start is None:
        return None
    pk = max(range(len(sp)), key=lambda i: sp[i][1])
    r = {"peak": sp[pk][1], "path": max(d for _, _, d in sp)}
    for v in (10, 20):
        t = next((t for t, s, d in sp if s >= v), None)
        r[f"t{v}"] = None if t is None else t - start
    stop = next(((t, d) for t, s, d in sp[pk:] if s < 2), None)
    r["brake_s"] = stop[0] - sp[pk][0] if stop else None
    r["brake_d"] = stop[1] - sp[pk][2] if stop else None
    return r


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", required=True)
    ap.add_argument("--game-dir", required=True)
    ap.add_argument("--save-template", required=True)
    ap.add_argument("--runs", type=int, default=4)
    ap.add_argument("--config", action="append", help="SIM_HZ:FPS")
    ap.add_argument("--busy", action="store_true", help="leave traffic and pedestrians at their default (Heavy)")
    ap.add_argument("--turn", action="store_true", help="steering test: heading change and distance over a 3 s left turn")
    ap.add_argument("--wall", action="store_true", help="drive into the end of the straight instead; report how far the car got")
    a = ap.parse_args()
    out = Path(tempfile.mkdtemp(prefix="physics-sweep-"))
    table = {}
    for cfg in a.config or ["30:30", "30:15", "30:20", "30:60", "30:144", "30:240", "30:0"]:
        hz, fps = cfg.split(":")
        env = {"MM3_SIM_HZ": hz, "RECOMP_FPS_LIMIT": fps}
        menu = MENU if not a.busy else [st for st in MENU if st.get("buttons") != "left"]
        if a.busy:                                   # the "down" that moved to the pedestrian row goes too
            downs = [i for i, st in enumerate(menu) if st.get("buttons") == "down"]
            del menu[downs[-1]]
        for attempt in range(3):   # early menu input can be eaten by loading: the mode check catches it
            try:
                g = Session(a.exe, a.game_dir, save_template=a.save_template, output_root=out / f"{cfg.replace(chr(58), chr(45))}-{attempt}",
                            timeout=150 + 14 * a.runs, env=env)
                g.__enter__()
                g.run({"steps": menu})
                break
            except (AssertionError, TimeoutError) as e:
                g.__exit__(type(e), e, None)
                print(cfg, "menu path failed:", e, flush=True)
        else:
            continue
        try:
            for n in range(a.runs):
                if a.turn:
                    w = turn_trial(g)
                    print(cfg, n, "turn: heading %+.0f deg, %.0f units travelled" % (w["turn"], w["path"]), flush=True)
                    continue
                if a.wall:
                    w = wall_trial(g)
                    print(cfg, n, "wall: furthest %.0f, ended %.0f units from the start" % (w["max"], w["end"]), flush=True)
                    continue
                pts, others = trial(g)
                m = metrics(pts)
                bad = others or not m or m["path"] > SAFE_UNITS
                print(cfg, n, "DISCARD" if bad else "ok", json.dumps(m and {k: round(v, 2) if v else v for k, v in m.items()}),
                      "other-car" if others else "", flush=True)
                if not bad:
                    table.setdefault(cfg, []).append(m)
        finally:
            g.__exit__(None, None, None)
    print("\nconfig   peak   t10   t20  brake_s brake_d   (median; spread = max-min)")
    for cfg, runs in table.items():
        f = lambda k: [r[k] for r in runs if r[k] is not None] or [float("nan")]
        med = lambda k: statistics.median(f(k))
        spr = lambda k: max(f(k)) - min(f(k))
        print(f"{cfg:7s} {med('peak'):5.1f} {med('t10'):5.2f} {med('t20'):5.2f} {med('brake_s'):6.2f} {med('brake_d'):6.1f}"
              f"   n={len(runs)}  spread peak {spr('peak'):.1f} t20 {spr('t20'):.2f}")
    print("evidence:", out)


if __name__ == "__main__":
    main()
