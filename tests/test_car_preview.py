"""Menu preview filtering, shared-edge alpha, and mixed-atlas occlusion."""
from pathlib import Path
import sys
import unittest

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/car-models"))
import preview


def quad(material=0, slope=0):
    positions = [[-1, -1, -slope], [1, -1, slope], [1, 1, slope], [-1, 1, -slope]]
    return dict(name="BODY_H", positions=positions, normals=[[0, 0, -1]] * 4,
                uvs=[[0, 0]] * 4, triangles=[[0, 1, 2, material], [0, 2, 3, material]])


class PreviewTests(unittest.TestCase):
    def test_texture_sampling_keeps_top_left_origin_and_repeat(self):
        texture = np.asarray([[[1, 0, 0, 1], [0, 1, 0, 1]],
                              [[0, 0, 1, 1], [1, 1, 1, 1]]], dtype=float)
        levels = preview.texture_mipmaps(texture)
        np.testing.assert_allclose(preview.sample_texture(levels, np.array([[0, 0], [0, 1]])),
                                   [[1, 0, 0, 1], [0, 0, 1, 1]])
        np.testing.assert_allclose(preview.sample_texture(levels, np.array([[.25, .25], [1.25, -.75]]), repeat=True),
                                   [[1, 0, 0, 1], [1, 0, 0, 1]])

    def test_minified_checker_averages_instead_of_aliasing(self):
        texture = np.ones((8, 8, 4))
        texture[..., :3] = (np.indices((8, 8)).sum(axis=0) % 2)[..., None]
        levels = preview.texture_mipmaps(texture)
        sampled = preview.sample_texture(levels, np.array([[.02, .38], [4.2, -7.1]]), repeat=True, level=3)
        np.testing.assert_allclose(sampled, [[.5, .5, .5, 1], [.5, .5, .5, 1]])
        mesh = quad()
        mesh["uvs"] = [[0, 0], [200, 0], [200, 200], [0, 200]]
        image = np.asarray(preview.render([mesh], [texture], (0, 0, -1), (96, 96), "", "",
                                          transparent_background=True))
        covered = image[..., 3] > 0
        self.assertEqual(np.ptp(image[..., 0][covered]), 0)
        self.assertTrue(80 < int(image[48, 48, 0]) < 130)

    def test_transparent_texel_rgb_does_not_stain_filtered_edges(self):
        texture = np.array([[[1, 0, 0, 1], [0, 0, 1, 0]]], dtype=float)
        levels = preview.texture_mipmaps(texture)
        sampled = preview.sample_texture(levels, np.array([[.5, .5]]))
        np.testing.assert_allclose(sampled, [[1, 0, 0, .5]])
        np.testing.assert_allclose(preview.sample_texture(levels, np.array([[.5, .5]]), level=1),
                                   [[1, 0, 0, .5]])

    def test_shared_edge_does_not_blend_glass_twice(self):
        texture = np.array([[[1, 1, 1, .5]]])
        image = np.asarray(preview.render([quad()], [texture], (0, 0, -1), (96, 96), "", "",
                                          transparent_background=True))
        self.assertEqual(int(image[48, 48, 3]), 128)
        self.assertEqual(int(image[30, 40, 3]), 128)
        self.assertEqual(int(image[40, 30, 3]), 128)

    def test_opaque_pixels_in_mixed_alpha_atlases_write_depth(self):
        red = np.array([[[1, 0, 0, 1], [1, 0, 0, .5]]], dtype=float)
        green = np.array([[[0, 1, 0, 1], [0, 1, 0, .5]]], dtype=float)
        image = np.asarray(preview.render([quad(0, .5), quad(1, -.5)], [red, green],
                                          (0, 0, -1), (96, 96), "", "", transparent_background=True))
        self.assertGreater(int(image[48, 25, 1]), 100)
        self.assertEqual(int(image[48, 25, 0]), 0)
        self.assertGreater(int(image[48, 70, 0]), 100)
        self.assertEqual(int(image[48, 70, 1]), 0)

    def test_menu_artwork_matches_native_image_contract(self):
        texture = np.array([[[1, .2, .1, 1]]])
        icons = preview.render_menu_icons([quad()], [texture], "Test", supersample=1)
        self.assertEqual(icons["Menu"].size, (128, 64))
        self.assertEqual(icons["Small"].size, (64, 64))
        for icon in icons.values():
            self.assertEqual(icon.mode, "RGBA")
            self.assertEqual(icon.getpixel((0, 0))[3], 0)
            self.assertGreater(np.asarray(icon)[..., 3].max(), 0)
        background = preview.render_selection_background([quad()], [texture], "Test", supersample=1)
        self.assertEqual(background.size, (640, 480))
        self.assertEqual(background.mode, "RGB")


if __name__ == "__main__":
    unittest.main()
