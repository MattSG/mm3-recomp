"""Graphics sweep: load each game mode, visit many vantage points, capture, analyse.

Usage (use a Python with Pillow + numpy, e.g. .venv-tests):
  python tools/game_test/gfx_sweep.py --exe EXE --game-dir game_files --save-template TPL \
      [--modes cruise,checkpoint,blitz,undercover,checkpoint-washington,blitz-paris] \
      [--baseline DIR] [--update-baseline] [--out DIR] [--headings 4]

Per mode it boots once, then teleports to each native route checkpoint (spread over the map)
and the start point, at N headings, capturing every view. Each frame is checked for:
  blank/black/uniform frames, large black holes (the old "dark wedge" class), magenta/NaN-looking
  pixels, clipped white, and (with --baseline) drift from a stored golden frame.
stderr is scanned for ICALL/ABI/exception markers. Output: report.json, contact sheet per mode,
exit 1 if any check failed. Findings are triage signals: look at the contact sheet before filing.
"""
import argparse, json, math, os, re, sys, time
from pathlib import Path

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))
from tools.game_test.run import Session
from tools.game_test.mission_win import ALL_MODS, MENUS
from PIL import Image
import numpy as np

# cruise: same menu path as driving.json
CRUISE = [{"action": "wait_log", "text": "[MOD] skip_intro: intro", "timeout": 60}, {"action": "wait", "seconds": 30}] + [
    {"action": "press", "buttons": b, "after": t} for b, t in
    (("start", 3), ("a", 7), ("down", 3), ("a", 3), ("a", 2), ("a", 2), ("a", 2), ("a", 2), ("a", 0))] + [
    {"action": "wait_player", "timeout": 30, "advance": "start"}]
MENUS = dict(MENUS, cruise=CRUISE)

LOG_BAD = re.compile(r"\[ICALL\] Failed|\[CRASH\]|\[ABI_|unhandled exception|shader.*(fail|error)|device lost", re.I)


def analyse(img):
    """Return a list of fault strings for one RGB frame (HxWx3 uint8)."""
    a = np.asarray(img.convert("RGB"), dtype=np.int16)
    h, w, _ = a.shape
    faults, lum = [], a.mean(axis=2)
    if lum.std() < 3:
        faults.append(f"uniform frame (std {lum.std():.1f}, mean {lum.mean():.0f})")
    if (lum < 4).mean() > 0.6:
        faults.append(f"mostly black ({(lum < 4).mean():.0%})")
    mag = (a[..., 0] > 200) & (a[..., 2] > 200) & (a[..., 1] < 60)
    if mag.mean() > 0.002:
        faults.append(f"magenta pixels {mag.mean():.2%} (missing texture/NaN)")
    if (lum > 253).mean() > 0.4:
        faults.append(f"blown-out white ({(lum > 253).mean():.0%})")
    # black holes: 32px tiles that are pure black while all four neighbours are lit
    t = 32
    tiles = lum[:h // t * t, :w // t * t].reshape(h // t, t, w // t, t).mean(axis=(1, 3))
    dark = tiles < 3
    holes = 0
    for y in range(1, dark.shape[0] - 1):
        for x in range(1, dark.shape[1] - 1):
            if dark[y, x] and tiles[y - 1:y + 2, x - 1:x + 2].sum() / 8 > 25 and dark[y - 1:y + 2, x - 1:x + 2].sum() <= 3:
                holes += 1
    if holes >= 2:
        faults.append(f"{holes} isolated black tiles (holes/missing geometry)")
    return faults


def drift(img, golden):
    """Mean absolute luminance difference 0..255 against a golden frame, or None if sizes differ."""
    if img.size != golden.size:
        return None
    return float(np.abs(np.asarray(img.convert("L"), np.int16) - np.asarray(golden.convert("L"), np.int16)).mean())


def sheet(images, path, cols=4, tw=320):
    th = tw * 9 // 16
    rows = math.ceil(len(images) / cols)
    s = Image.new("RGB", (cols * tw, rows * th))
    for i, (_, im) in enumerate(images):
        s.paste(im.convert("RGB").resize((tw, th)), (i % cols * tw, i // cols * th))
    s.save(path)


def vantages(g):
    route = json.loads(g.request("route"))
    pts = [tuple(p) for p in route.get("points", [])]
    cars = [c for c in json.loads(g.request("cars"))["cars"] if c["player"]]
    if cars:
        pts.insert(0, (cars[0]["x"], cars[0]["z"]))
    return pts


def sweep_mode(mode, a, report):
    out = Path(a.out) / mode
    out.mkdir(parents=True, exist_ok=True)
    frames, entry = [], report.setdefault(mode, {"frames": [], "log": []})
    env = dict(ALL_MODS)
    with Session(a.exe, a.game_dir, save_template=a.save_template, output_root=str(out), timeout=a.timeout, env=env) as g:
        g.run({"steps": MENUS[mode]})
        time.sleep(8)  # countdown / camera settle
        for i, (x, z) in enumerate(vantages(g)):
            for k in range(a.headings):
                heading = 2 * math.pi * k / a.headings
                name = f"v{i:02d}_h{k}"
                try:
                    g.teleport(x, z, heading)
                    time.sleep(a.settle)
                    cap = g.capture()
                except AssertionError as e:
                    entry["frames"].append({"name": name, "faults": [f"harness: {e}"]})
                    continue
                img = Image.open(cap["path"]).convert("RGB")
                png = out / f"{name}.png"
                img.save(png)
                faults = analyse(img)
                d = None
                if a.baseline:
                    gold = Path(a.baseline) / mode / f"{name}.png"
                    if a.update_baseline:
                        gold.parent.mkdir(parents=True, exist_ok=True)
                        img.save(gold)
                    elif gold.exists():
                        d = drift(img, Image.open(gold))
                        if d is None:
                            faults.append("baseline size differs")
                        elif d > a.drift:
                            faults.append(f"drift {d:.1f} from baseline (> {a.drift})")
                entry["frames"].append({"name": name, "faults": faults, "drift": d})
                frames.append((name, img))
        for line in g.log_text.splitlines():
            if LOG_BAD.search(line):
                entry["log"].append(line.strip()[:200])
        entry["log"] = sorted(set(entry["log"]))[:20]
    if frames:
        sheet(frames, out / "contact_sheet.png")


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--exe", required=True)
    p.add_argument("--game-dir", required=True)
    p.add_argument("--save-template", required=True)
    p.add_argument("--modes", default="cruise,checkpoint")
    p.add_argument("--out", default="gfx_sweep_out")
    p.add_argument("--baseline")
    p.add_argument("--update-baseline", action="store_true")
    p.add_argument("--headings", type=int, default=4)
    p.add_argument("--settle", type=float, default=2.5)
    p.add_argument("--drift", type=float, default=12.0)
    p.add_argument("--timeout", type=int, default=600)
    a = p.parse_args()
    report = {}
    for mode in a.modes.split(","):
        try:
            sweep_mode(mode, a, report)
        except Exception as e:  # keep sweeping the other modes
            report.setdefault(mode, {"frames": [], "log": []})["error"] = repr(e)
    Path(a.out).mkdir(parents=True, exist_ok=True)
    (Path(a.out) / "report.json").write_text(json.dumps(report, indent=1))
    bad = 0
    for mode, e in report.items():
        for f in e["frames"]:
            if f["faults"]:
                bad += 1
                print(f"{mode}/{f['name']}: " + "; ".join(f["faults"]))
        for l in e["log"]:
            bad += 1
            print(f"{mode} LOG: {l}")
        if "error" in e:
            bad += 1
            print(f"{mode} ERROR: {e['error']}")
    print(f"{sum(len(e['frames']) for e in report.values())} frames, {bad} findings -> {a.out}/report.json")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
