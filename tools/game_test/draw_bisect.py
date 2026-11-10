"""Find which draw calls paint a screen region: hold a race still, re-render it with the first K draws.

  python tools/game_test/draw_bisect.py --save-template TPL [--roi x0,y0,x1,y1] [--car N] [--out DIR] [--env K=V]

The race is frozen with the hold-the-race mod (F2, posted to the game window) so every frame is the same.
RECOMP_DRAW_LIMIT (src/host_sdl3/video/nv2a_sdlgpu.c) names a file holding K; draws numbered above K are
skipped. A region of interest (fractions of the frame, default the player car's rear lamps in a chase view)
is compared between K and K-1; wherever it changes, that draw is one that paints it, and its state is printed
(program, blend, depth, colour mask, textures). Needs Pillow + numpy; sequential, one game, light on the CPU.
"""
import argparse, ctypes, json, os, sys, time
from ctypes import wintypes
from pathlib import Path

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))
import numpy as np
from PIL import Image
from tools.game_test import pack as packmod

WM_KEYDOWN, WM_KEYUP, VK_F2 = 0x100, 0x101, 0x71
user32 = ctypes.windll.user32


def game_window(pid):
    found = []
    cb = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)(
        lambda h, _: (found.append(h) if user32.IsWindowVisible(h) and _pid(h) == pid else None) or True)
    user32.EnumWindows(cb, 0)
    return found


def _pid(h):
    p = wintypes.DWORD()
    user32.GetWindowThreadProcessId(h, ctypes.byref(p))
    return p.value


def press_f2(hwnds):
    for h in hwnds:
        user32.PostMessageW(h, WM_KEYDOWN, VK_F2, 0)
    time.sleep(.12)
    for h in hwnds:
        user32.PostMessageW(h, WM_KEYUP, VK_F2, 0)


class Bisect:
    def __init__(self, g, flag, roi, out):
        self.g, self.flag, self.roi, self.out = g, flag, roi, Path(out)
        self.cache = {}
        self.out.mkdir(parents=True, exist_ok=True)

    def render(self, k):
        """The region at limit k (and the whole frame, kept on disk)."""
        if k not in self.cache:
            Path(self.flag).write_text(str(k))
            time.sleep(.35)                       # a few frames at the new limit
            cap = self.g.capture()
            img = Image.open(cap["path"]).convert("RGB")
            w, h = img.size
            x0, y0, x1, y1 = self.roi
            crop = img.crop((int(x0 * w), int(y0 * h), int(x1 * w), int(y1 * h)))
            img.save(self.out / f"k{k:05d}.png")
            self.cache[k] = np.asarray(crop, np.float32)
        return self.cache[k]

    def differs(self, a, b):
        return float(np.abs(self.render(a) - self.render(b)).mean()) > self.eps

    def find(self, lo, hi, hits):
        """Draws in (lo, hi] that change the region, assuming a change shows between the ends."""
        if hi - lo < 1 or not self.differs(lo, hi):
            return
        if hi - lo == 1:
            hits.append(hi)
            return
        mid = (lo + hi) // 2
        self.find(lo, mid, hits)
        self.find(mid, hi, hits)


def main():
    a = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    a.add_argument("--exe", default="build/RelWithDebInfo/mm3_recomp.exe")
    a.add_argument("--game-dir", default="game_files")
    a.add_argument("--save-template", required=True)
    a.add_argument("--roi", default="0.40,0.60,0.60,0.72", help="x0,y0,x1,y1 as fractions of the frame")
    a.add_argument("--car", type=int, default=0)
    a.add_argument("--max-draws", type=int, default=6000)
    a.add_argument("--eps", type=float, default=0.15, help="mean pixel change that counts")
    a.add_argument("--out", default="draw_bisect_out")
    a.add_argument("--env", action="append", default=[])
    a.add_argument("--timeout", type=int, default=600)
    o = a.parse_args()
    out = Path(o.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    flag = str(out / "limit.txt")
    Path(flag).write_text("-1")
    pk = packmod.Pack(argparse.Namespace(exe=o.exe, game_dir=o.game_dir, save_template=o.save_template, out=str(out),
                                         env=o.env, baseline=None, timeout=o.timeout, verbose=True, quick=False,
                                         headings=4, settle=2.5, drift=12))
    env = {"RECOMP_DRAW_LIMIT": flag, "RECOMP_DRAW_INFO": "1", "MM3_MOD_PAUSE_HOLD": "1", "MM3_SMOOTH": "0"}
    with pk.game("bisect", "run", env=env) as g:
        pk.to_main(g)
        pk.to_car_select(g)
        for _ in range(o.car):
            pk.press(g, "right", 1.2)
        pk.press(g, "a", 2)
        pk.press(g, "a", 0)
        g.wait_player(timeout=60, advance="start")
        time.sleep(8)
        g.press("rt", 2500, wait_release=False)
        time.sleep(3)
        press_f2(game_window(g.process.pid))
        g.wait_for(lambda: "[MM3_PAUSE] race held" in g.log_text, 5, "race held")
        time.sleep(1)
        b = Bisect(g, flag, [float(x) for x in o.roi.split(",")], out)
        b.eps = o.eps
        full, empty = o.max_draws, -0
        b.render(full)
        b.render(-1)
        base = float(np.abs(b.render(full) - b.render(0)).mean())
        print(f"region change between 0 draws and all: {base:.2f}")
        hits = []
        b.find(0, full, hits)
        print(f"{len(hits)} draws change the region: {hits}")
        for k in hits:
            Path(flag).write_text(str(k))
            time.sleep(.4)
            line = [l for l in (g.run_dir / "stderr.log").read_text(errors="replace").splitlines()
                    if l.startswith(f"[DRAW {k}]")]
            print(line[-1] if line else f"[DRAW {k}] (no state line)")
        Path(flag).write_text("-1")
        (out / "hits.json").write_text(json.dumps(hits))
    print(f"frames at each step: {out}")


if __name__ == "__main__":
    main()
