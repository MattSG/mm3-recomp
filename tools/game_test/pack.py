"""Full test pack: menus, every car, every paint, drives, weather/season/time conditions, missions, gfx.

  python tools/game_test/pack.py --exe EXE --game-dir game_files --save-template TPL \
      [--suites menus,cars,paints,drive,conditions,missions,gfx|all] [--quick] [--out DIR] [--baseline DIR]

Needs a Python with Pillow + numpy (.venv-tests) and a copied save template that holds a profile.
Never needs more than one game process at a time: suites run one after another, sequential by design.
Every captured frame is checked for black/uniform/magenta/blown-out frames and black holes; menu
screens must also respond to input (a press that changes nothing is a warning); cars/paints must
change the picture. Game stderr is scanned for ICALL/ABI/crash/device-lost markers.
Output: DIR/report.json, DIR/report.html (thumbnails of every frame), DIR/<suite>/*.png; exit 1 on a fail.
Warnings (status "warn") do not fail the pack: look at them in the report.

Suites (--quick trims each to a few minutes; the full run is long, so start it when you can leave it):
  menus       every top-level menu, the Single Race / Cruise / city / settings screens, option rows
  cars        the car-select screen for every car (--car-count, 49 with all cars unlocked), consecutive cars must differ
  paints      every paint colour of every car on the car-select screen (a paint job must change the car)
  drive       cruise drive per car (--drive-cars all|sample|1,5,9): motion, frame check, pause menu and resume
  conditions  cruise in each city under each season / time of day / weather (diag = k-th of each; full = all)
  missions    win Checkpoint / Blitz / Undercover through the native route (tools/game_test/mission_win.py)
  gfx         the vantage-point sweep of gfx_sweep.py in each mode
"""
import argparse, base64, io, json, math, os, re, sys, time, traceback
from contextlib import contextmanager
from pathlib import Path

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))
from tools.game_test.run import Session
from tools.game_test import mission_win, gfx_sweep
from tools.game_test.gfx_sweep import analyse, drift, LOG_BAD, vantages, OFFSETS
from PIL import Image
import numpy as np

BOOT = [{"action": "wait_log", "text": "[MOD] skip_intro: intro", "timeout": 60}, {"action": "wait", "seconds": 30}]
PAL_FPS = "60"
PAUSE_FLAG = 0x00429DB2   # the game's own pause singleton (see src/mods/pause_hold.c)
SUITES = "menus cars paints drive conditions missions gfx lamps".split()
# Menu tour scripts, from the main menu with the cursor on the first item. Tokens: a button (optionally
# "button:seconds" to wait), or "!name" to capture and check a screen. "b" goes back.
TOUR = {
    "main": "!main",
    "undercover": "a:3 !list down !list-2 b",
    "single-race": "down a:3 !modes down !modes-blitz down !modes-checkpoint up up a:3 !city right !city-washington left "
                   "a:3 !settings down !s-peds down !s-cops down !s-season down !s-time down !s-weather up up up up up "
                   "b b b b",
    "multiplayer": "down down a:3 !multiplayer b",
    "options": "down down down a:3 !options a:3 !game-setup b down a:3 !controller b down a:3 !sound b down a:3 !records b "
               "down a:4 !credits b:3 down a:4 !bonus-movie b:3 down a:3 !demos b",
    "profiles": "down down down down a:3 !profiles b",
}


def to_png(bmp_path, out):
    img = Image.open(bmp_path).convert("RGB")
    img.save(out)
    return img


def small(img):
    return np.asarray(img.convert("L").resize((96, 54)), np.float32)


class Pack:
    def __init__(self, a):
        self.a, self.out = a, Path(a.out)
        self.cases = []
        self.out.mkdir(parents=True, exist_ok=True)

    # --- recording -------------------------------------------------------------------------------
    def record(self, suite, name, status, faults=(), image=None, note=None):
        self.cases.append(dict(suite=suite, name=name, status=status, faults=list(faults), image=image, note=note))
        mark = {"pass": "ok  ", "warn": "WARN", "fail": "FAIL"}[status]
        print(f"[{mark}] {suite}/{name}" + (": " + "; ".join(faults) if faults else ""), flush=True)

    def frame(self, g, suite, name, previous=None, must_change=False, crop=None, drift_limit=None):
        """Capture, analyse, store. previous: the last frame, to check the picture responded."""
        sub = self.out / suite
        sub.mkdir(exist_ok=True)
        cap = g.capture()
        png = sub / f"{name}.png"
        img = to_png(cap["path"], png)
        faults, warns = analyse(img), []
        if suite in ("menus", "cars", "paints"):    # dark UI panels look like holes; that check is for 3D scenes
            faults = [f for f in faults if "isolated black tiles" not in f]
        if crop:
            w, h = img.size
            box = (int(crop[0] * w), int(crop[1] * h), int(crop[2] * w), int(crop[3] * h))
            region = np.asarray(img.crop(box).convert("L"), np.float32)
            if region.std() < 8:
                faults.append(f"subject area empty (std {region.std():.1f})")
        if previous is not None and must_change:
            d = float(np.abs(small(img) - small(previous)).mean())
            if d < 0.35:
                warns.append(f"picture did not change after the input (diff {d:.2f})")
        if self.a.baseline:
            gold = Path(self.a.baseline) / suite / f"{name}.png"
            if self.a.update_baseline:
                gold.parent.mkdir(parents=True, exist_ok=True)
                img.save(gold)
            elif gold.exists():
                d = drift(img, Image.open(gold))
                limit = drift_limit or self.a.drift
                if d is None:
                    faults.append("baseline size differs")
                elif d > limit:
                    faults.append(f"drift {d:.1f} from baseline (> {limit})")
        self.record(suite, name, "fail" if faults else "warn" if warns else "pass", faults + warns,
                    image=f"{suite}/{name}.png")
        return img

    # --- sessions --------------------------------------------------------------------------------
    @contextmanager
    def game(self, suite, label, env=None):
        """One owned game process; errors become a failed case so the other suites still run."""
        e = dict(mission_win.ALL_MODS, MM3_UNLOCK_ALL_CARS="1", RECOMP_FPS_LIMIT=PAL_FPS)
        e.update(dict(x.split("=", 1) for x in self.a.env))
        e.update(env or {})
        g = None
        try:
            with Session(self.a.exe, self.a.game_dir, save_template=self.a.save_template,
                         output_root=str(self.out / "runs"), timeout=self.a.timeout, env=e) as g:
                yield g
        except Exception as ex:
            self.record(suite, f"{label}: session", "fail", [f"{type(ex).__name__}: {ex}"])
            if self.a.verbose:
                traceback.print_exc()
        finally:
            if g is not None:
                bad = sorted({l.strip()[:200] for l in g.log_text.splitlines() if LOG_BAD.search(l)})[:10]
                self.record(suite, f"{label}: game log", "fail" if bad else "pass", bad)

    def press(self, g, button, after=1.5):
        g.press(button, after=after)

    def to_main(self, g):
        g.run({"steps": BOOT})
        self.press(g, "start", 3)
        self.press(g, "a", 7)

    def to_car_select(self, g, city_right=0, settings=()):
        """Single Race -> Cruise -> city -> settings -> car select. settings: (row, rights) per option row."""
        for b, t in (("down", 3), ("a", 3), ("a", 2)):
            self.press(g, b, t)
        for _ in range(city_right):
            self.press(g, "right", 1)
        self.press(g, "a", 2)                       # city -> settings
        row = 0
        for r, rights in settings:
            for _ in range(r - row):
                self.press(g, "down", .6)
            row = r
            for _ in range(rights):
                self.press(g, "right", .6)
        self.press(g, "a", 3)                       # settings -> car select
        # The list opens on the last-used car, in the middle, and does not wrap: go to its first car so that
        # "car N" means the same car in every session.
        for _ in range(self.a.car_count - 1):
            self.press(g, "left", .5)

    def script(self, g, suite, tag, script, prev=None):
        for tok in script.split():
            if tok.startswith("!"):
                prev = self.frame(g, suite, f"{tag}-{tok[1:]}", previous=prev, must_change=True)
            else:
                button, _, secs = tok.partition(":")
                self.press(g, button, float(secs) if secs else 1.5)
        return prev

    # --- suites ----------------------------------------------------------------------------------
    def menus(self):
        for tag, script in TOUR.items():
            if self.a.quick and tag not in ("main", "single-race", "options"):
                continue
            with self.game("menus", tag) as g:
                self.to_main(g)
                self.script(g, "menus", tag, script)

    def browse(self, suite, paints):
        """Every car (and with paints, every paint) of the car-select screen in one session."""
        n = self.a.car_count if not self.a.quick else min(self.a.car_count, 6)
        np_ = (self.a.paint_count if not self.a.quick else 3) if paints else 0
        with self.game(suite, "browse") as g:
            self.to_main(g)
            self.to_car_select(g)
            prev = None
            for car in range(n):
                prev = self.frame(g, suite, f"car{car:02d}", previous=prev, must_change=True, crop=(.2, .25, .8, .85),
                                  drift_limit=30)
                for p in range(1, np_ + 1):
                    self.press(g, "down", .9)
                    prev = self.frame(g, suite, f"car{car:02d}-paint{p}", previous=prev, must_change=True,
                                      crop=(.2, .25, .8, .85), drift_limit=30)
                self.press(g, "right", 1.2)

    def cars(self):
        self.browse("cars", False)

    def paints(self):
        self.browse("paints", True)

    def drive_one(self, suite, label, car=0, city_right=0, settings=(), paint=0, sweep=0):
        with self.game(suite, label) as g:
            self.to_main(g)
            self.to_car_select(g, city_right, settings)
            for _ in range(car):
                self.press(g, "right", 1.2)
            for _ in range(paint):
                self.press(g, "down", .9)
            self.press(g, "a", 2)                   # -> transmission
            self.press(g, "a", 0)                   # -> load
            g.wait_player(timeout=60, advance="start")
            hold = 9000                              # pad commands queue behind a held throttle: keep it short
            t_hold = time.time()
            g.press("rt", hold, wait_release=False)
            try:
                g.wait_motion(timeout=15)
                g.sample(seconds=4, interval=.2, min_distance=1)
                self.record(suite, f"{label}: moves", "pass")
            except (AssertionError, TimeoutError) as ex:
                self.record(suite, f"{label}: moves", "fail", [str(ex)])
            img = self.frame(g, suite, f"{label}-drive")
            if suite == "drive":
                time.sleep(max(0, hold / 1000 + 1 - (time.time() - t_hold)))
                flag = lambda: g.perform({"action": "peek", "address": PAUSE_FLAG, "size": 1})["hex"]
                self.press(g, "start", 2.5)
                paused = flag() != "00"
                self.frame(g, suite, f"{label}-pause", previous=img, must_change=True)
                self.press(g, "start", 2.5)
                resumed = flag() == "00"
                ok = paused and resumed
                self.record(suite, f"{label}: pause/resume", "pass" if ok else "fail",
                            [] if ok else [f"pause flag paused={paused} resumed={resumed}"])
            if sweep:
                for i, (x, z) in enumerate(vantages(g)[:sweep]):
                    for k in range(self.a.headings):
                        for dx, dz in OFFSETS:
                            try:
                                g.teleport(x + dx, z + dz, 2 * math.pi * k / self.a.headings)
                                break
                            except AssertionError:
                                continue
                        else:
                            continue
                        time.sleep(self.a.settle)
                        self.frame(g, suite, f"{label}-v{i}h{k}")

    def drive(self):
        spec = self.a.drive_cars
        count = self.a.car_count
        cars = (list(range(count)) if spec == "all" else
                [i for i in (0, 8, 16, 24, 32, 40, 48) if i < count][:3 if self.a.quick else 7] if spec == "sample"
                else [int(x) for x in spec.split(",")])
        for car in cars:
            self.drive_one("drive", f"car{car:02d}", car=car)

    def conditions(self):
        full = self.a.conditions == "full"
        combos = ([(s, t, w) for s in range(4) for t in range(4) for w in range(4)] if full
                  else [(k, k, k) for k in range(4)])
        for city, city_right in (("paris", 0), ("washington", 1)):
            for s, t, w in combos[:2 if self.a.quick else None]:
                self.drive_one("conditions", f"{city}-s{s}t{t}w{w}", city_right=city_right,
                               settings=((3, s), (4, t), (5, w)) if (s or t or w) else (), sweep=2)

    def missions(self):
        for mission in mission_win.MENUS:
            if self.a.quick and mission not in ("checkpoint", "blitz", "undercover"):
                continue
            ok, why = False, ""
            try:
                with self.game("missions", mission) as g:
                    g.run({"steps": mission_win.MENUS[mission]})
                    ok, why = self.win(g, mission)
            finally:
                self.record("missions", f"{mission}: won", "pass" if ok else "fail", [] if ok else [why])

    def win(self, g, mission):
        route = lambda: json.loads(g.request("route"))
        if mission == "undercover":
            time.sleep(8)
            zones = mission_win.undercover_zones(self.a.game_dir)
            done = mission_win.run_zones(g, zones)
            for _ in range(3):
                time.sleep(10)
            self.frame(g, "missions", f"{mission}-end")
            return done == len(zones), f"zones {done}/{len(zones)}"
        t_end = time.time() + 90
        while (route()["count"] == 0 or route()["current"] >= route()["count"]) and time.time() < t_end:
            time.sleep(1)
        g.press("rt", 60000, wait_release=False)
        time.sleep(4)
        total, passed = route()["count"], 0
        while passed <= total + 2:
            r = route()
            x, z = r["points"][r["current"]]
            try:
                g.teleport(x, z)
            except AssertionError:
                break
            passed += 1
            t0 = time.time()
            while time.time() - t0 < 8 and route()["points"][0] == r["points"][0]:
                time.sleep(.4)
        time.sleep(3)
        self.frame(g, "missions", f"{mission}-results")
        return passed == total, f"checkpoints {passed}/{total}"

    def gfx(self):
        modes = ["cruise", "checkpoint"] if self.a.quick else list(gfx_sweep.MENUS)
        for mode in modes:
            self.drive_gfx(mode)

    def drive_gfx(self, mode):
        with self.game("gfx", mode) as g:
            g.run({"steps": gfx_sweep.MENUS[mode]})
            time.sleep(8)
            pts = vantages(g)
            for i, (x, z) in enumerate(pts[:3] if self.a.quick else pts):
                for k in range(self.a.headings):
                    for dx, dz in OFFSETS:
                        try:
                            g.teleport(x + dx, z + dz, 2 * math.pi * k / self.a.headings)
                            break
                        except AssertionError:
                            continue
                    else:
                        self.record("gfx", f"{mode}-v{i:02d}h{k}", "warn", ["no usable ground near the vantage"])
                        continue
                    time.sleep(self.a.settle)
                    self.frame(g, "gfx", f"{mode}-v{i:02d}h{k}")

    def lamps(self):
        """Per car in Cruise: the driven car's own lamp nodes (MM3_LAMP_TRACE) and how red its rear view is."""
        spec = self.a.drive_cars
        count = self.a.car_count
        cars = (list(range(count)) if spec == "all" else [0, 8, 16, 24, 32, 40, 48][:3 if self.a.quick else 7]
                if spec == "sample" else [int(x) for x in spec.split(",")])
        for car in cars:
            label = f"car{car:02d}"
            with self.game("lamps", label, env={"MM3_LAMP_TRACE": "1", "RECOMP_RENDER_SCALE": "2"}) as g:
                self.to_main(g)
                self.to_car_select(g)
                for _ in range(car):
                    self.press(g, "right", 1.2)
                self.press(g, "a", 2)
                self.press(g, "a", 0)
                g.wait_player(timeout=60, advance="start")
                time.sleep(6)
                g.press("rt", 2500, wait_release=False)
                time.sleep(4)
                text = (g.run_dir / "stderr.log").read_text(errors="replace")
                own = {}
                for m in re.finditer(r"\[LAMP\] node \w+ owner (\w+) kind (\d+) brightness (\S+) color \w+ mask (\w+)", text):
                    if int(m.group(4), 16) & 0x100:        # the driven car's lamps carry the road-projector bit
                        own.setdefault(m.group(1), {})[int(m.group(2))] = float(m.group(3))
                kinds = list(own.values())[-1] if own else {}
                faults = [f"no {n} lamp node for the driven car" for k, n in ((1, "tail"), (0, "front")) if kinds.get(k, 0) <= 0]
                img = self.frame(g, "lamps", f"{label}-rear")
                a = np.asarray(img.convert("RGB"), np.int16)
                h, w, _ = a.shape
                reg = a[int(.55 * h):int(.78 * h), int(.35 * w):int(.65 * w)]
                red = int((reg[..., 0] - np.maximum(reg[..., 1], reg[..., 2])).max())
                if red < 100:
                    faults.append(f"rear lens area is not lit (peak red excess {red})")
                self.record("lamps", f"{label}: lamps", "fail" if faults else "pass", faults,
                            note=f"nodes {kinds} red {red}")

    # --- report ----------------------------------------------------------------------------------
    def finish(self):
        (self.out / "report.json").write_text(json.dumps(self.cases, indent=1))
        rows = []
        for c in self.cases:
            img = ""
            if c["image"]:
                p = self.out / c["image"]
                im = Image.open(p).convert("RGB")
                im.thumbnail((240, 135))
                buf = io.BytesIO()
                im.save(buf, "JPEG", quality=70)
                img = f'<a href="{c["image"]}"><img src="data:image/jpeg;base64,{base64.b64encode(buf.getvalue()).decode()}"></a>'
            rows.append(f'<tr class="{c["status"]}"><td>{c["suite"]}</td><td>{c["name"]}</td><td>{c["status"]}</td>'
                        f'<td>{img}</td><td>{"; ".join(c["faults"])}</td></tr>')
        (self.out / "report.html").write_text(
            "<style>body{font:13px sans-serif}td{padding:2px 8px;vertical-align:top}.fail{background:#fcc}.warn{background:#ffd}"
            "</style><table>" + "".join(rows) + "</table>")
        counts = {s: sum(c["status"] == s for c in self.cases) for s in ("pass", "warn", "fail")}
        print(f"{len(self.cases)} cases: {counts['pass']} pass, {counts['warn']} warn, {counts['fail']} fail -> {self.out}/report.html")
        return 1 if counts["fail"] else 0


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--exe", default="build/RelWithDebInfo/mm3_recomp.exe")
    p.add_argument("--game-dir", default="game_files")
    p.add_argument("--save-template", required=True)
    p.add_argument("--suites", default="all")
    p.add_argument("--quick", action="store_true")
    p.add_argument("--out", default="pack_out")
    p.add_argument("--baseline")
    p.add_argument("--update-baseline", action="store_true")
    p.add_argument("--drift", type=float, default=12.0)
    p.add_argument("--car-count", type=int, default=49)
    p.add_argument("--paint-count", type=int, default=5)
    p.add_argument("--drive-cars", default="sample")
    p.add_argument("--conditions", choices=("diag", "full"), default="diag")
    p.add_argument("--headings", type=int, default=4)
    p.add_argument("--settle", type=float, default=2.5)
    p.add_argument("--timeout", type=int, default=900)
    p.add_argument("--env", action="append", default=[], help="extra KEY=VALUE for the game (repeatable)")
    p.add_argument("--verbose", action="store_true")
    a = p.parse_args()
    pack = Pack(a)
    for suite in SUITES if a.suites == "all" else a.suites.split(","):
        print(f"== {suite}", flush=True)
        getattr(pack, suite)()
    sys.exit(pack.finish())


if __name__ == "__main__":
    main()
