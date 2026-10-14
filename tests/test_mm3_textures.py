"""CDDS writer checks, optionally against the independent retail-format decoder.

Set MM3_DECDDS_ORACLE to a compiled Daniel Stien decdds DLL for byte-for-byte
validation of the header, alpha/DXT data and every mip level.
"""
import ctypes
import importlib.util
import io
import os
from pathlib import Path
import random
import struct
import tempfile
import unittest

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('mm3_textures', ROOT / 'tools/car-models/mm3_textures.py')
textures = importlib.util.module_from_spec(spec)
spec.loader.exec_module(textures)


class TexturesTest(unittest.TestCase):
    def test_truncated_header(self):
        with self.assertRaises(ValueError):
            textures.dds_to_cdds(b'DDS ')

    def test_image_limits(self):
        with self.assertRaises(ValueError):
            textures.image_to_dds(Path('unused'), max_size=333)

    def test_mips_and_alpha_format(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'source.png'
            for alpha, fourcc in [(255, b'DXT1'), (128, b'DXT5')]:
                Image.new('RGBA', (24, 11), (120, 45, 71, alpha)).save(path)
                dds = textures.image_to_dds(path)
                self.assertEqual(dds[84:88], fourcc)
                self.assertEqual(struct.unpack_from('<2I', dds, 12), (16, 32))
                self.assertEqual(struct.unpack_from('<I', dds, 28)[0], 6)
                self.assertEqual(textures.dds_to_cdds(dds)[:4], bytes.fromhex('c8440f99'))
                with self.assertRaises(ValueError):
                    textures.dds_to_cdds(dds[:-1])

    def test_native_background_header_and_rgb565_pixels(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'background.png'
            image = Image.new('RGB', (640, 480), (0, 0, 0))
            image.putpixel((0, 0), (255, 0, 0))
            image.putpixel((1, 0), (0, 255, 0))
            image.putpixel((2, 0), (0, 0, 255))
            image.putpixel((639, 479), (255, 255, 255))
            image.save(path)
            dds = textures.image_to_background_dds(path)
            # Native bg.cdds: 640x480, RGB565, no alpha and no mip chain.
            expected_header = [0] * 32
            expected_header[:8] = [0x20534444, 124, 0x100F, 480, 640,
                                   640 * 480 * 2, 0, 0]
            expected_header[19:28] = [32, 0x40, 0, 16, 0xF800, 0x7E0,
                                     0x1F, 0, 0x401008]
            self.assertEqual(dds[:128], struct.pack('<32I', *expected_header))
            self.assertEqual(len(dds), 128 + 640 * 480 * 2)
            self.assertEqual(dds[128:134], bytes.fromhex('00f8e0071f00'))
            self.assertEqual(dds[-2:], bytes.fromhex('ffff'))
            decoded = Image.open(io.BytesIO(dds)).convert('RGB')
            self.assertEqual(decoded.size, (640, 480))
            self.assertEqual(decoded.getpixel((0, 0)), (255, 0, 0))
            self.assertEqual(decoded.getpixel((1, 0)), (0, 255, 0))
            self.assertEqual(decoded.getpixel((2, 0)), (0, 0, 255))
            self.assertEqual(decoded.getpixel((639, 479)), (255, 255, 255))

    def test_background_requires_native_dimensions(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'background.png'
            Image.new('RGB', (128, 64)).save(path)
            with self.assertRaisesRegex(ValueError, '640x480'):
                textures.image_to_background_dds(path)

    def test_native_menu_header_and_alpha(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'icon.png'
            for dimensions in ((128, 64), (64, 64), (256, 64)):
                image = Image.new('RGBA', dimensions, (0, 0, 0, 0))
                image.paste((255, 0, 0, 255), (8, 8, 24, 24))
                image.save(path)
                dds = textures.image_to_menu_dds(path)
                payload_size = dimensions[0] * dimensions[1] * 4
                self.assertEqual(dds[84:88], bytes(4))
                self.assertEqual(struct.unpack_from('<I', dds, 8)[0], 0x100F)
                self.assertEqual(struct.unpack_from('<2I', dds, 12),
                                 tuple(reversed(dimensions)))
                self.assertEqual(struct.unpack_from('<I', dds, 20)[0], dimensions[0] * 4)
                self.assertEqual(struct.unpack_from('<I', dds, 28)[0], 0)
                self.assertEqual(struct.unpack_from('<7I', dds, 80),
                                 (0x41, 0, 32, 0xFF0000, 0xFF00, 0xFF, 0xFF000000))
                self.assertEqual(struct.unpack_from('<I', dds, 108)[0], 0x401008)
                self.assertEqual(len(dds), 128 + payload_size)
                decoded = Image.open(io.BytesIO(dds)).convert('RGBA')
                self.assertEqual(decoded.tobytes(), image.tobytes())
            Image.new('RGBA', (120, 60)).save(path)
            with self.assertRaisesRegex(ValueError, 'Vehicle menu icons'):
                textures.image_to_menu_dds(path)

    def test_menu_paint_tire_and_glass_boundaries_are_lossless(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'icon.png'
            # A single 4x4 region can contain paint, tires, windows and clear
            # pixels. Preserve every color/alpha instead of a DXT palette.
            colors = [(238, 19, 7, 255), (5, 8, 12, 255), (24, 34, 48, 255),
                      (100, 160, 210, 127), (190, 70, 40, 0)]
            rgba = bytes(channel for y in range(64) for x in range(64)
                         for channel in colors[(x + 2 * y) % len(colors)])
            image = Image.frombytes('RGBA', (64, 64), rgba)
            image.save(path)
            decoded = Image.open(io.BytesIO(textures.image_to_menu_dds(path))).convert('RGBA')
            self.assertEqual(decoded.tobytes(), rgba)
            self.assertEqual(decoded.getpixel((2, 1)), (190, 70, 40, 0))
            self.assertEqual(decoded.getpixel((1, 1)), (100, 160, 210, 127))

    @unittest.skipUnless(os.environ.get('MM3_DECDDS_ORACLE'), 'Independent decoder not configured')
    def test_independent_decoder(self):
        oracle = ctypes.CDLL(os.environ['MM3_DECDDS_ORACLE'])
        oracle.decdds_extract.argtypes = [ctypes.c_char_p, ctypes.c_uint32,
            ctypes.POINTER(ctypes.POINTER(ctypes.c_uint8)), ctypes.POINTER(ctypes.c_uint32), ctypes.c_int]
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'source.png'
            rng = random.Random(3487)
            for alpha in (False, True):
                image = Image.new('RGBA', (64, 32))
                image.putdata([tuple(rng.randrange(256) for _ in range(3)) +
                               ((rng.randrange(256) if alpha else 255),)
                               for _ in range(64 * 32)])
                image.save(path)
                dds = textures.image_to_dds(path)
                cdds = textures.dds_to_cdds(dds)
                pointer, size = ctypes.POINTER(ctypes.c_uint8)(), ctypes.c_uint32()
                result = oracle.decdds_extract(cdds, len(cdds), ctypes.byref(pointer), ctypes.byref(size), 0)
                self.assertEqual(result, 0)
                self.assertEqual(ctypes.string_at(pointer, size.value), dds)

    @unittest.skipUnless(os.environ.get('MM3_DECDDS_ORACLE'), 'Independent decoder not configured')
    def test_native_ui_independent_decoder(self):
        oracle = ctypes.CDLL(os.environ['MM3_DECDDS_ORACLE'])
        oracle.decdds_extract.argtypes = [ctypes.c_char_p, ctypes.c_uint32,
            ctypes.POINTER(ctypes.POINTER(ctypes.c_uint8)), ctypes.POINTER(ctypes.c_uint32), ctypes.c_int]
        with tempfile.TemporaryDirectory() as folder:
            for filename, dimensions, dds_writer, cdds_writer in [
                ('background.png', (640, 480), textures.image_to_background_dds,
                 textures.image_to_background_cdds),
                ('menu.png', (128, 64), textures.image_to_menu_dds,
                 textures.image_to_menu_cdds),
                ('small.png', (64, 64), textures.image_to_menu_dds,
                 textures.image_to_menu_cdds),
            ]:
                path = Path(folder) / filename
                image = Image.new('RGBA', dimensions, (16, 32, 64, 255))
                image.paste((200, 160, 120, 128), (4, 4, 20, 20))
                image.save(path)
                dds, cdds = dds_writer(path), cdds_writer(path)
                pointer, size = ctypes.POINTER(ctypes.c_uint8)(), ctypes.c_uint32()
                result = oracle.decdds_extract(cdds, len(cdds), ctypes.byref(pointer), ctypes.byref(size), 0)
                self.assertEqual(result, 0, filename)
                self.assertEqual(ctypes.string_at(pointer, size.value), dds, filename)


if __name__ == '__main__':
    unittest.main()
