"""Frame interpolation check on the player's car: one-frame pops (wheels vanishing, parts of the car drawn elsewhere).

Drives a hidden Cruise run (driving.json up to the start, then weaving with the throttle held), records every presented
image (RECOMP_RECORD), and looks in the box around the player's car for frames that are darker than both the frame
before and the frame after while those two agree: a tyre or body part that is missing or replaced for one frame.
Darkening only, so sparks and lights (brighter for a frame) do not count. Also counts frames where the rear tyres are
not drawn (the dark band below the bumper's corners is road-coloured): blended toward the wrong wheel, a tyre sat
inside the body. Run it with interp on and off: off is the noise floor (real one-step effects, the car off-centre in
turns and crashes: tyres "missing" ~15%), on must not exceed it. Always look at the saved crops too.

  py tools/game_test/interp_car_check.py --exe build/RelWithDebInfo/mm3_recomp.exe --game game_files \
      --save <copied save template> --out <dir> [--interp 0|1] [--secs 10] [KEY=VALUE env ...]
  py tools/game_test/interp_car_check.py --analyse <dir>/rec.raw

Defaults are the heavy settings interp has to hold up under: 240 fps cap, render scale 4, MSAA 4, a 960x540 window
(at 480x270 a tyre is a few pixels and checks miss it). Runs spawning away from downtown are retried.
"""
import argparse, os, struct, sys, time
from pathlib import Path
import numpy as np

SPAWN = (97.657, -1028.309)      # downtown: the reproducible spawn with the user's save
BOX = (.30, .45, .70, .85)       # x0, y0, x1, y1 of the frame: the player's car in the chase camera
POP_PX = 1000                    # odd pixels (at 960x540) that make a frame a pop


def analyse(raw, out=None, report=8):
    """Returns (frames compared, pops, worst list). Saves A|B|C|mask crops of the worst into out."""
    f = open(raw, 'rb'); t0 = None; win = []; rows = []; tyres_missing = 0
    while True:
        h = f.read(16)
        if len(h) < 16: break
        t, w, hh = struct.unpack('<qII', h); px = f.read(w * hh * 4)
        if len(px) < w * hh * 4: break
        t0 = t if t0 is None else t0
        d = np.frombuffer(px, np.uint8).reshape(hh, w, 4)
        d = d[int(hh * BOX[1]):int(hh * BOX[3]), int(w * BOX[0]):int(w * BOX[2]), 2::-1].astype(np.int16)
        if win and np.array_equal(win[-1][1], d): continue          # a held frame
        full = np.frombuffer(px, np.uint8).reshape(hh, w, 4)[:, :, :3].astype(int).sum(2)
        sy, sx = hh / 540, w / 960                                  # tyre bands measured at 960x540
        band = lambda x0: full[int(404 * sy):int(411 * sy), int(x0 * sx):int((x0 + 30) * sx)].mean()
        tyres_missing += band(388) > 60 and band(538) > 60
        win = (win + [((t - t0) / 1e6, d)])[-3:]
        if len(win) < 3: continue
        A, B, C = (x[1] for x in win)
        sa, sb, sc = A.sum(2), B.sum(2), C.sum(2)
        m = (sa - sb > 90) & (sc - sb > 90) & (np.abs(C - A).max(2) < 25)
        rows.append((int(m.sum()) * 518400 // (w * hh), win[1][0], A, B, C, m))
    n = np.array([r[0] for r in rows]) if rows else np.zeros(1, int)
    rows.sort(key=lambda r: -r[0])
    pops = int((n > POP_PX).sum())
    print(f'{raw}: frames {len(rows)}  pops (>{POP_PX} px) {pops}  over 300 px {(n > 300).sum()}  max {n.max()}')
    for k, s, *_ in rows[:report]:
        print(f'  {s:7.3f} s  {k} px')
    print(f'tyres missing (both rear corners road-coloured): {tyres_missing} of {len(rows)} frames '
          f'({100 * tyres_missing // max(len(rows), 1)}%)')
    if out:
        from PIL import Image
        os.makedirs(out, exist_ok=True)
        for i, (k, s, A, B, C, m) in enumerate(rows[:6]):
            if k <= 300: break
            mk = np.zeros_like(A); mk[m] = 255
            Image.fromarray(np.concatenate([A, B, C, mk], 1).astype(np.uint8)).save(f'{out}/pop{i}_{s:.3f}_{k}.png')
    return len(rows), pops, rows[:report]


def drive(a, env_extra):
    sys.path.insert(0, str(Path(__file__).parent))
    import json, run
    out = Path(a.out); out.mkdir(parents=True, exist_ok=True)
    raw, on = out / 'rec.raw', out / 'rec.raw.on'
    env = dict(RECOMP_FPS_LIMIT='240', RECOMP_RENDER_SCALE='4', RECOMP_MSAA='4', RECOMP_WINDOW_SIZE='960x540',
               MM3_MOD_FRAMERATE='1', MM3_MOD_INTERP=str(a.interp), MM3_MOD_WIDESCREEN='1', MM3_MOD_SKY='1',
               MM3_MOD_DRAW_DISTANCE='1', MM3_MOD_FLARE_DEPTH='1', MM3_MOD_COLLISION_FIX='1', RECOMP_RECORD=str(raw))
    env.update(env_extra)
    steps = json.load(open(Path(__file__).parent / 'driving.json'))['steps']
    for attempt in range(10):
        for p in (raw, on):
            if p.exists(): p.unlink()
        with run.Session(a.exe, a.game, output_root=out / 'runs', timeout=240, env=env,
                         save_template=a.save) as g:
            wrong = False
            for s in steps:
                if s['action'] == 'sample': break
                if s['action'] == 'press' and s.get('hold_ms', 0) > 5000:
                    p = g.query('position', 1)
                    if abs(p['x'] - SPAWN[0]) + abs(p['z'] - SPAWN[1]) > 10:
                        wrong = True; break
                g.perform(s)
            if not wrong:
                on.write_text('1')
                end, k = time.monotonic() + a.secs, 0
                while time.monotonic() < end:
                    g.press('rt+' + ('lleft', 'lright')[k % 2], hold_ms=1200, wait_release=False); g.wait(1.3); k += 1
                on.unlink()
            g.result['status'] = 'passed'
        if not wrong: return raw
        print('spawned away from downtown, retrying', flush=True)
    sys.exit('never spawned downtown')


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--analyse'); ap.add_argument('--exe'); ap.add_argument('--game'); ap.add_argument('--save')
    ap.add_argument('--out'); ap.add_argument('--interp', type=int, default=1); ap.add_argument('--secs', type=float, default=10)
    a, extra = ap.parse_known_args()
    if a.analyse:
        analyse(a.analyse, str(Path(a.analyse).parent / 'pops'))
    else:
        raw = drive(a, dict(x.split('=', 1) for x in extra))
        analyse(str(raw), str(Path(a.out) / 'pops'))
