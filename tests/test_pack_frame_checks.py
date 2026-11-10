"""The test pack's frame checks must flag broken frames (no game needed)."""
import sys, unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import numpy as np
from PIL import Image
from tools.game_test.gfx_sweep import analyse


def scene():
    rng = np.random.default_rng(1)
    return rng.integers(60, 200, (270, 480, 3), dtype=np.uint8)


class FrameChecks(unittest.TestCase):
    def faults(self, a):
        return " | ".join(analyse(Image.fromarray(a)))

    def test_good_frame_is_clean(self):
        self.assertEqual(self.faults(scene()), "")

    def test_black_frame(self):
        self.assertIn("black", self.faults(np.zeros((270, 480, 3), np.uint8)))

    def test_uniform_frame(self):
        self.assertIn("uniform", self.faults(np.full((270, 480, 3), 120, np.uint8)))

    def test_magenta_texture(self):
        a = scene(); a[:60, :, :] = (255, 0, 255)
        self.assertIn("magenta", self.faults(a))

    def test_white_out(self):
        a = scene(); a[:200] = 255
        self.assertIn("blown-out", self.faults(a))

    def test_black_holes(self):
        a = scene(); a[64:96, 64:96] = 0; a[160:192, 320:352] = 0
        self.assertIn("black tiles", self.faults(a))


if __name__ == "__main__":
    unittest.main()
