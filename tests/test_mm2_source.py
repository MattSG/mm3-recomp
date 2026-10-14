"""Parser integrity and source-coordinate tests, without proprietary fixtures."""
import importlib.util
import io
import math
from pathlib import Path
import struct
import tempfile
import unittest
import zipfile

from PIL import Image

SPEC = importlib.util.spec_from_file_location('mm2_source', Path(__file__).parents[1] / 'tools/car-models/mm2_source.py')
source = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(source)


def angel_string(text):
    value = text.encode() + b'\0'
    return bytes([len(value)]) + value


def geometry(invalid_normal=False):
    data = struct.pack('<5I', 1, 3, 3, 1, 0x112)
    data += struct.pack('<2H3I', 1, 0, 0, 3, 3)
    vertices = [((0, 0, 0), (-2, 0)), ((1, 0, 0), (2, 0)), ((0, 0, 1), (0, 2))]
    for index, (position, uv) in enumerate(vertices):
        normal = (float('nan'), 0, 0) if invalid_normal and index == 0 else (0, -1, 0)
        data += struct.pack('<8f', *position, *normal, *uv)
    return data + struct.pack('<I3H', 3, 0, 1, 2)


def pkg(version=3, invalid_normal=False):
    shader = struct.pack('<2I', 129, 1) + angel_string('car_paint')
    shader += bytes([255, 255, 255, 255, 80, 80, 80, 255, 0, 0, 0, 255]) + struct.pack('<f', 3)
    chunks = [('BODY_H', geometry(invalid_normal)), ('shaders', shader)]
    result = f'PKG{version}'.encode()
    for name, payload in chunks:
        result += b'FILE' + angel_string(name)
        if version == 3:
            result += struct.pack('<I', len(payload))
        result += payload
    return result


class SourceTests(unittest.TestCase):
    def test_pkg2_and_pkg3_preserve_coordinates_and_repeat_uvs(self):
        for version in (2, 3):
            model = source.read_pkg(pkg(version))
            mesh = model['meshes'][0]
            self.assertEqual(mesh['positions'][2], [0, 0, 1])
            self.assertEqual(mesh['uvs'][0], [-2, 0])
            self.assertEqual(mesh['triangles'], [[0, 1, 2, 0]])
            self.assertEqual(model['variants'][0][0]['diffuse'], [1, 1, 1, 1])

    def test_nan_normal_repaired_from_faces(self):
        mesh = source.read_pkg(pkg(invalid_normal=True))['meshes'][0]
        self.assertEqual(mesh['repaired_normals'], 1)
        self.assertEqual(mesh['normals'][0], [0, -1, 0])
        self.assertTrue(all(math.isfinite(v) for n in mesh['normals'] for v in n))

    def test_pkg_truncation_and_bad_index_rejected(self):
        with self.assertRaises(source.FormatError):
            source.read_pkg(pkg()[:-1])
        broken = bytearray(geometry())
        broken[-2:] = struct.pack('<H', 3)
        with self.assertRaises(source.FormatError):
            source.read_geometry(source.Reader(bytes(broken)), 'BODY_H')

    def test_texture_and_wheel_origin_export(self):
        image = io.BytesIO()
        Image.new('RGBA', (2, 2), (100, 20, 30, 128)).save(image, format='TGA')
        model_data = pkg().replace(b'BODY_H', b'WHL0_H')
        assets = {'geometry/car.pkg': model_data, 'texture/car_paint.tga': image.getvalue(),
                  'geometry/car_whl0.mtx': struct.pack('<12f', *([0] * 9), -1, .3, -1.4)}
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            result = source.neutral_model(assets, 'car', output)
            mesh = result['meshes'][0]
            self.assertAlmostEqual(mesh['transform'][0][3], -1)
            self.assertAlmostEqual(mesh['transform'][2][3], -1.4)
            self.assertEqual(result['materials'][0]['texture'], 'textures/car_paint.png')
            with Image.open(output / result['materials'][0]['texture']) as decoded:
                self.assertEqual(decoded.getpixel((0, 0)), (100, 20, 30, 128))

    def test_ares_tree_and_unsafe_names(self):
        # Root directory + one file, extension shared through the name table.
        names = b'\0geo\0car\x01\0bms\0'
        body = b'mesh bytes'
        offset = 16 + 24 + len(names)
        data = b'ARES' + struct.pack('<3I', 2, 1, len(names))
        data += struct.pack('<3I', 1, 1, (1 << 14) | 1)
        data += struct.pack('<3I', offset, (10 << 23) | len(body), (5 << 14) | (12 << 1))
        data += names + body
        self.assertEqual(source.read_ares(data), {'geo/car12.bms': body})
        with self.assertRaises(source.FormatError):
            source.safe_name('../outside.exe')
        with self.assertRaises(source.FormatError):
            source.safe_name('C:/outside.exe')

    def test_bms_normal_table_and_translation(self):
        names = b'car_paint\0'.ljust(32, b'\0') + bytes(16)
        data = b'3HSM' + struct.pack('<3f4I3f2B6s', -1, .3, -1.4, 3, 3, 1, 4,
                                      1, 1, 1, 1, 11, bytes(6))
        data += names + struct.pack('<9f', 0, 0, 0, 1, 0, 0, 0, 0, 1)
        data += bytes([4, 4, 4]) + struct.pack('<6f', 0, 0, 1, 0, 0, 1)
        data += struct.pack('<3H', 0, 1, 2) + bytes([1]) + struct.pack('<4H', 0, 1, 2, 0)
        mesh, textures = source.read_bms(data, 'whl0_h')
        self.assertEqual(mesh['normals'], [[0, -1, 0]] * 3)
        self.assertEqual(mesh['triangles'], [[0, 1, 2, 1]])
        self.assertAlmostEqual(mesh['transform'][2][3], -1.4)
        self.assertEqual(textures, [None, 'car_paint'])
        lattice = source.mm1_normal_table()
        self.assertEqual(len(lattice), 198)
        self.assertEqual(lattice[94], [-.2361, .4505, .861])
        self.assertEqual(lattice[197], [.2448, -.9316, -.2685])

    def test_mm1_tga_stored_rows_without_changing_mm2_image_decoding(self):
        # First stored row is red; second is blue. MM1 BMS samples V=0
        # from the first stored row even for the bottom-origin descriptor.
        header = bytearray(18)
        header[2] = 2
        header[12:16] = struct.pack('<2H', 1, 2)
        header[16] = 32
        pixels = bytes([0, 0, 255, 255, 255, 0, 0, 255])
        for descriptor in (0x08, 0x28):
            header[17] = descriptor
            data = bytes(header) + pixels
            imported = source.mm1_texture_image(data, '.tga')
            self.assertEqual(imported.getpixel((0, 0)), (255, 0, 0, 255))
            self.assertEqual(imported.getpixel((0, 1)), (0, 0, 255, 255))
            ordinary = source.texture_image(data, '.tga')
            expected = (0, 0, 255, 255) if descriptor == 0x08 else (255, 0, 0, 255)
            self.assertEqual(ordinary.getpixel((0, 0)), expected)

    def test_protected_plain_zip_payload_requires_crc(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'legacy.ar'
            with zipfile.ZipFile(path, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
                archive.writestr('geometry/model.pkg', b'PKG3 plain payload')
            data = bytearray(path.read_bytes())
            data[:4] = bytes(4)  # legacy local-header protection quirk
            path.write_bytes(data)
            self.assertEqual(source.archive_assets(path)['geometry/model.pkg'], b'PKG3 plain payload')
            with zipfile.ZipFile(path) as archive:
                info = archive.infolist()[0]
                info.CRC ^= 1
                with self.assertRaises(source.FormatError):
                    source.verified_zip_payload(bytes(data), info)


if __name__ == '__main__':
    unittest.main()
