"""CDDS writer checks, optionally against the independent retail-format decoder.

Set MM3_DECDDS_ORACLE to a compiled Daniel Stien decdds DLL for byte-for-byte
validation of the header, alpha/DXT data and every mip level.
"""
import ctypes
import importlib.util
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


if __name__ == '__main__':
    unittest.main()
