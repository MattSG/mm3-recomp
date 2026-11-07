import sys, unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from PIL import Image
import numpy as np
from tools.game_test.gfx_sweep import analyse, drift


def scene(seed=1):
    r = np.random.default_rng(seed)
    return Image.fromarray(r.integers(40, 200, (270, 480, 3), dtype=np.uint8))


class GfxSweepChecks(unittest.TestCase):
    def test_good_frame_clean(self):
        self.assertEqual(analyse(scene()), [])

    def test_black_and_uniform(self):
        f = analyse(Image.new("RGB", (480, 270)))
        self.assertTrue(any("uniform" in x for x in f) and any("black" in x for x in f))

    def test_magenta(self):
        a = np.asarray(scene()).copy(); a[:20, :] = (255, 0, 255)
        self.assertTrue(any("magenta" in x for x in analyse(Image.fromarray(a))))

    def test_black_holes(self):
        a = np.asarray(scene()).copy(); a[64:96, 64:96] = 0; a[160:192, 256:288] = 0
        self.assertTrue(any("black tiles" in x for x in analyse(Image.fromarray(a))))

    def test_drift(self):
        self.assertEqual(drift(scene(), scene()), 0)
        self.assertGreater(drift(scene(1), scene(2)), 12)
        self.assertIsNone(drift(scene(), Image.new("RGB", (10, 10))))


if __name__ == "__main__":
    unittest.main()
