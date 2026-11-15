"""Whole-screen interpolation check: every pair replayed at t=0 must look like the pair before's real last frame.

Record a run with RECOMP_INTERP_SELFCHECK=1 RECOMP_INTERP_TRACE=3 RECOMP_RECORD=<raw> (interp_car_check.py passes
extra KEY=VALUE env through). The renderer then presents, per pair, its real last frame ([REC] t 1.0) and the pair
replayed at t=0 ([REC] t 0.0). Any difference between a replay at 0 and the previous pair's last frame is drawing
the interpolation got wrong: a draw paired with the wrong object, a register taken from the wrong step, a vertex
blend of other content. Things new in a frame (spawned, culled differently, particles) also differ; look at the
crops before blaming the pairing.

  py tools/game_test/interp_selfcheck.py <rec.raw> <stderr.log> [outdir]
"""
import os, struct, sys
import numpy as np

def frames(raw, log):
    tag = {}
    for l in open(log, errors='replace'):
        if l.startswith('[REC] '):
            _, us, k, t, m = l.split(); tag[int(us)] = (float(t), float(m))
    f = open(raw, 'rb'); t0 = None
    while True:
        h = f.read(16)
        if len(h) < 16: return
        t, w, hh = struct.unpack('<qII', h); px = f.read(w * hh * 4)
        if len(px) < w * hh * 4: return
        t0 = t if t0 is None else t0
        if t in tag:
            yield (t - t0) / 1e6, tag[t][0], tag[t][1], np.frombuffer(px, np.uint8).reshape(hh, w, 4)[:, :, 2::-1]

def erode(m):
    p = np.pad(m, 1); r = np.ones_like(m)
    for dy in range(3):
        for dx in range(3): r &= p[dy:dy + m.shape[0], dx:dx + m.shape[1]]
    return r

def main(raw, log, out=None):
    prev = last = None; rows = []          # the last frames of the pair before and of this pair
    for s, t, m, img in frames(raw, log):
        if t == 1.0: prev, last = last, (m, img)
        elif t == 0.0 and prev is not None and last[0] == m:
            d = np.abs(img.astype(np.int16) - prev[1].astype(np.int16)).max(2)
            mask = erode(d > 40)
            rows.append((int(mask.sum()), s, prev[1], img, mask))
    if not rows: print('no t=0 replays found: was RECOMP_INTERP_SELFCHECK=1 RECOMP_INTERP_TRACE=3 set?'); return
    n = np.array([r[0] for r in rows]); px = rows[0][3].shape[0] * rows[0][3].shape[1]
    print(f'{raw}: pairs {len(rows)}  differing px median {int(np.median(n))}  p90 {int(np.percentile(n, 90))}  '
          f'max {n.max()}  pairs over 0.5% of the screen {(n > px * .005).sum()}')
    rows.sort(key=lambda r: -r[0])
    for k, s, *_ in rows[:10]: print(f'  {s:7.3f} s  {k} px')
    if out:
        from PIL import Image
        os.makedirs(out, exist_ok=True)
        for i, (k, s, a, b, m) in enumerate(rows[:8]):
            mk = np.zeros_like(a); mk[m] = 255
            Image.fromarray(np.concatenate([a, b, mk], 1)).save(f'{out}/sc{i}_{s:.3f}_{k}.png')

if __name__ == '__main__':
    main(*sys.argv[1:4])
