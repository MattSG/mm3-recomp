#!/usr/bin/env python3
"""Check that the AI drives off the start line forwards in the first race.

Usage:
  python tools/scripts/test_ai_start.py --exe <mm3_recomp.exe> --save <save template>
      [--tag ai-start] [--fps-limit 60] [--secs 20] [--course | --crash | --crash-at X,Z,HEADING]

Takes the bench's menu path into a race, holds throttle on the player's car, and
samples every car through the MM3_DEBUG_TELEPORT pipe ("cars": X/Z plus the
body's forward vector) every 250 ms. Per car it sums distance travelled along
its own forward vector and against it. The player's car is the sign check: under
throttle it must come out forwards.

Verdict FAIL when any AI car covers more than 10 units backwards or an AI car
ends up behind where it started. Exit code 0 = PASS, 1 = FAIL, 2 = no race.
--course runs a braking and steering sequence; --crash throttles into buildings;
--crash-at teleports to X,Z facing HEADING (radians) and throttles into that spot.
Set MM3_CRASH_CAPTURE to a path to also request present captures during a crash run.

Evidence goes to conformance_tmp/ai_start/<tag>; per-car tracks are written as
car-<id>.csv. Works on Windows (named pipe) and POSIX (Unix socket, see send_teleport.py).

The bench's menu path is repeated here.
"""
import argparse
import csv
import importlib.util
import math
import os
import shutil
import subprocess
import time


def die(msg):
    raise SystemExit(msg)


def game_running():
    if os.name != "nt":
        return subprocess.run(["pgrep", "-x", "mm3_recomp"], capture_output=True).returncode == 0
    out = subprocess.run(["tasklist", "/FI", "IMAGENAME eq mm3_recomp.exe", "/NH"],
                         capture_output=True, text=True).stdout
    return "mm3_recomp.exe" in out.lower()


def kill_game():
    if os.name != "nt":
        subprocess.run(["pkill", "-KILL", "-x", "mm3_recomp"], capture_output=True)
    else:
        subprocess.run(["taskkill", "/F", "/IM", "mm3_recomp.exe"], capture_output=True)


def load_teleport():
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "send_teleport.py")
    spec = importlib.util.spec_from_file_location("send_teleport", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def dist(a, b):
    return math.hypot(a["x"] - b["x"], a["z"] - b["z"])


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--exe", required=True)
    parser.add_argument("--save", required=True)
    parser.add_argument("--tag", default="ai-start")
    parser.add_argument("--fps-limit", dest="fps_limit", type=int, default=60)
    parser.add_argument("--secs", type=int, default=20)
    parser.add_argument("--course", action="store_true")
    parser.add_argument("--crash", action="store_true")
    parser.add_argument("--crash-at", dest="crash_at", metavar="X,Z,HEADING")
    args = parser.parse_args()

    repo = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    teleport = load_teleport()
    crash_at = None
    if args.crash_at:
        crash_at = [float(v) for v in args.crash_at.split(",")]
        if len(crash_at) != 3:
            die("--crash-at needs X,Z,HEADING")
    crash_capture = os.environ.get("MM3_CRASH_CAPTURE")

    run_dir = os.path.join(repo, "conformance_tmp", "ai_start", args.tag)
    if game_running():
        kill_game()
        deadline = time.monotonic() + 15
        while game_running() and time.monotonic() < deadline:
            time.sleep(0.25)
    if os.path.exists(run_dir):
        shutil.rmtree(run_dir)
    os.makedirs(run_dir)
    save = os.path.join(run_dir, "save")
    shutil.copytree(args.save, save)
    pad = os.path.join(run_dir, "pad.txt")
    with open(pad, "w"):
        pass
    log = os.path.join(run_dir, "stderr.log")
    envs = {
        "RECOMP_PB_EXEC": "1", "RECOMP_FB_WINDOW": "1", "RECOMP_USB": "1", "RECOMP_PAD_LIVE": pad,
        "RECOMP_APU_DSP_ACK": "0x80458810", "MM3_SAVE_DIR": save,
        "RECOMP_FPS_LIMIT": str(args.fps_limit), "MM3_DEBUG_TELEPORT": "1",
        "RECOMP_FRAME_CSV": os.path.join(run_dir, "frames.csv"),
    }
    if crash_capture:
        envs["RECOMP_PRESENT_CAPTURE"] = crash_capture
    child_env = dict(os.environ)
    child_env.update(envs)

    with open(os.path.join(run_dir, "stdout.log"), "wb") as stdout, open(log, "wb") as stderr:
        proc = subprocess.Popen([args.exe], cwd=repo, env=child_env, stdout=stdout, stderr=stderr)

    def press(button, after, ms=150):
        with open(pad, "a") as f:
            f.write(f"{button}:{ms}\n")
        time.sleep(0.1)
        time.sleep(after)

    def cars():
        try:
            return teleport.send_command(proc.pid, "cars").get("cars", [])
        except Exception:
            return []

    def pad_write(line):
        with open(pad, "a") as f:
            f.write(line + "\n")

    samples = {}
    try:
        # Same path as the bench: skip intro movies, title -> profile -> menu -> race.
        start = time.monotonic()
        while time.monotonic() - start < 120:
            time.sleep(0.7)
            text = ""
            if os.path.exists(log):
                with open(log, encoding="utf-8", errors="replace") as f:
                    text = f.read()
            if "MOVIE_END] name=intro" in text:
                break
            if "MOVIE_FRAME" in text:
                press("start", 0)
        time.sleep(5)
        press("start", 4)
        press("a", 6)
        for button, after in [("down", 1), ("a", 4), ("a", 4), ("a", 5), ("a", 8), ("a", 10), ("a", 9), ("start", 6)]:
            press(button, after)
        live = []
        for _ in range(6):
            if live:
                break
            live = [c for c in cars() if c.get("player")]
            if not live:
                press("a", 9)
        if not live:
            print("NO RACE: player car never became live", flush=True)
            raise SystemExit(2)

        # Hold throttle (right trigger) through the window; sample everyone.
        clock = time.monotonic()
        steps = []
        if args.course:
            pad_write("rt:6000")
            steps = [(6000, "lt:3000"), (9000, "rt:4000"), (13000, "rt+lleft:1000"),
                     (14000, f"rt:{args.secs * 1000}")]
        elif crash_at:
            # The same wall every run: teleported to x,z facing heading (radians), then throttle into it.
            x, z, h = crash_at
            teleport.send_command(proc.pid, f"teleport {x!r} {z!r} {h!r}")
            time.sleep(2)
            clock = time.monotonic()
            pad_write(f"rt:{args.secs * 1000}")
        elif args.crash:
            pad_write("rt:4000")
            steps = [(4000, "rt+lright:600"), (4600, f"rt:{args.secs * 1000}")]
        else:
            pad_write(f"rt:{args.secs * 1000 + 1000}")

        step_i = 0
        capped = False
        while time.monotonic() - clock < args.secs:
            elapsed = time.monotonic() - clock
            if crash_at and crash_capture and not capped and elapsed >= 1.2:
                # Frames of the approach, to see what touches the car.
                with open(crash_capture + ".flag", "w") as f:
                    f.write("200")
                capped = True
            if step_i < len(steps) and elapsed * 1000 >= steps[step_i][0] - 150:
                pad_write(steps[step_i][1])
                step_i += 1
            for c in cars():
                samples.setdefault(c["id"], []).append(dict(c, t=elapsed))
            time.sleep(0.25)
    finally:
        if proc.poll() is None:
            proc.kill()
            proc.wait(timeout=15)
        shutil.rmtree(save, ignore_errors=True)  # 5 GB per run

    for car_id, track in samples.items():  # every car's track, for a closer look
        with open(os.path.join(run_dir, f"car-{car_id}.csv"), "w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=["t", "x", "z", "fx", "fz", "player"],
                                    extrasaction="ignore", quoting=csv.QUOTE_ALL)
            writer.writeheader()
            writer.writerows(track)

    fail = False
    rows = []
    for car_id, s in samples.items():
        fwd = back = 0.0
        for i in range(1, len(s)):
            along = (s[i]["x"] - s[i - 1]["x"]) * s[i - 1]["fx"] + (s[i]["z"] - s[i - 1]["z"]) * s[i - 1]["fz"]
            if along >= 0:
                fwd += along
            else:
                back -= along
        net = (s[-1]["x"] - s[0]["x"]) * s[0]["fx"] + (s[-1]["z"] - s[0]["z"]) * s[0]["fz"]
        player = bool(s[0].get("player"))
        bad = not player and (back > 10 or net < 0)
        if bad:
            fail = True
        rows.append({
            "car": "player" if player else f"ai {car_id}",
            "vt": f"{int(s[0]['vt']) & 0xFFFFFFFF:08X}", "samples": len(s),
            "forward": round(fwd, 1), "backward": round(back, 1), "net": round(net, 1),
            "verdict": "REVERSING" if bad else "ok",
        })
    rows.sort(key=lambda r: r["car"])
    headers = ["car", "vt", "samples", "forward", "backward", "net", "verdict"]
    widths = [max(len(h), *(len(str(r[h])) for r in rows)) for h in headers] if rows else [len(h) for h in headers]
    print("  ".join(h.ljust(w) for h, w in zip(headers, widths)))
    for r in rows:
        print("  ".join(str(r[h]).ljust(w) for h, w in zip(headers, widths)))
    player_row = next((r for r in rows if r["car"] == "player"), None)

    # Throttle/gear check: top speed over any 1 s of the player's samples (units/s).
    ps = [s for s in samples.values() if s[0].get("player")]
    ps = [sample for s in ps for sample in s]
    top = 0.0
    for i in range(len(ps)):
        j = i + 1
        while j < len(ps) and ps[j]["t"] - ps[i]["t"] < 1:
            j += 1
        if j < len(ps):
            top = max(top, dist(ps[j], ps[i]) / (ps[j]["t"] - ps[i]["t"]))
    print(f"player top speed: {top:,.1f} units/s")
    # Acceleration: path length covered by 2/4/6/8/12 s of throttle.
    if len(ps) > 1:
        d = 0.0
        marks = [2, 4, 6, 8, 12]
        mi = 0
        out = []
        i = 1
        while i < len(ps) and mi < len(marks):
            d += dist(ps[i], ps[i - 1])
            while mi < len(marks) and ps[i]["t"] - ps[0]["t"] >= marks[mi]:
                out.append(f"{marks[mi]}s={d:,.0f}")
                mi += 1
            i += 1
        print("player distance: " + " ".join(out))
    if args.course and len(ps) > 1:
        # Braking: from 6 s, distance rolled until the first sample under 1 unit/s.
        # Steering: heading turned and path covered from 13 s to 14 s.
        def at(s):
            return next((p for p in ps if p["t"] - ps[0]["t"] >= s), None)

        def path(a, b):
            total = 0.0
            for i in range(1, len(ps)):
                t = ps[i]["t"] - ps[0]["t"]
                if a < t <= b:
                    total += dist(ps[i], ps[i - 1])
            return total

        v6 = path(5.5, 6) / 0.5
        roll = 0.0
        stop = None
        for i in range(1, len(ps)):
            t = ps[i]["t"] - ps[0]["t"]
            if t <= 6:
                continue
            step = dist(ps[i], ps[i - 1])
            roll += step
            if step / (ps[i]["t"] - ps[i - 1]["t"]) < 1:
                stop = t
                break
        stopped = f"after {stop - 6:,.2f} s" if stop else "never"
        print(f"braking: from {v6:,.1f} units/s, rolled {roll:,.1f} units, stopped {stopped}")
        h0, h1 = at(13), at(14.25)
        if h0 and h1:
            turn = (math.degrees(math.atan2(h1["fx"], h1["fz"])) - math.degrees(math.atan2(h0["fx"], h0["fz"])))
            while turn > 180:
                turn -= 360
            while turn < -180:
                turn += 360
            print(f"steering: from {path(12.5, 13) / 0.5:,.1f} units/s turned {turn:,.0f} deg "
                  f"over {path(13, 14.25):,.1f} units")
        ai = []
        for s in samples.values():
            if not s[0].get("player"):
                ai.append(sum(dist(s[i], s[i - 1]) for i in range(1, len(s))))
        print(f"ai path over {args.secs} s: " + " ".join(f"{d:,.0f}" for d in sorted(ai)))
    if (args.crash or crash_at) and len(ps) > 2:
        # The first impact after 4 s (0.5 s for --crash-at): a sample interval in
        # which speed falls by more than half from over 10 units/s.
        v = [0.0] + [dist(ps[i], ps[i - 1]) / (ps[i]["t"] - ps[i - 1]["t"]) for i in range(1, len(ps))]
        hit = None
        for i in range(2, len(ps)):
            if (ps[i]["t"] - ps[0]["t"] > (0.5 if crash_at else 4) and v[i - 1] > 10 and v[i] < v[i - 1] / 2):
                hit = i
                break
        if hit is not None:
            j = hit
            while j < len(ps) - 1 and ps[j]["t"] - ps[hit]["t"] < 2:
                j += 1
            k = hit
            while k > 0 and ps[hit]["t"] - ps[k]["t"] < 1.5:
                k -= 1
            print(f"crash: at {ps[hit]['t'] - ps[0]['t']:,.1f} s, {v[hit - 1]:,.1f} -> {v[hit]:,.1f} units/s, "
                  f"{v[j]:,.1f} units/s 2 s later")
            print(f"crash: 1.5 s before at x={ps[k]['x']!r} z={ps[k]['z']!r} "
                  f"heading={math.atan2(ps[k]['fx'], ps[k]['fz'])!r}")
        else:
            print("crash: no impact")
        if crash_at:
            # Into a wall: peak speed, when it stops, where it rests.
            pk = 0
            for i in range(1, len(v)):
                if v[i] > v[pk]:
                    pk = i
            st = pk
            while st < len(v) - 1 and v[st] >= 1:
                st += 1
            far = min(p["z"] for p in ps)
            print(f"wall: peak {v[pk]:,.1f} units/s at {ps[pk]['t'] - ps[0]['t']:,.2f} s, "
                  f"stopped at {ps[st]['t'] - ps[0]['t']:,.2f} s, rests z={ps[-1]['z']:,.2f} (furthest {far:,.2f})")
    if not player_row or player_row["forward"] <= player_row["backward"]:
        print("WARNING: player car did not come out forwards under throttle; forward vector sign is suspect")
    # The frame rate the race actually ran at: median of the last $Secs seconds of flips.
    frame_times = []
    frames = os.path.join(run_dir, "frames.csv")
    if os.path.exists(frames):
        with open(frames, encoding="utf-8", errors="replace") as f:
            for line in f.read().splitlines()[1:]:
                try:
                    frame_times.append(float(line.split(",")[1]))
                except (ValueError, IndexError):
                    pass
    n = min(len(frame_times), args.secs * 60)
    if n > 0:
        med = sorted(frame_times[len(frame_times) - n:])[round(n / 2)]
        if med > 0:
            print(f"race frame rate: {1000.0 / med:,.0f} fps (median)")
    print(f"fps limit {args.fps_limit}: {'FAIL' if fail else 'PASS'}")
    raise SystemExit(1 if fail else 0)


if __name__ == "__main__":
    main()
