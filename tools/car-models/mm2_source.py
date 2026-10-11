#!/usr/bin/env python3
"""Read MM1 ARES/BMS and MM2 ZIP/AR/PKG cars into neutral textured meshes.

This reader changes no game files and never executes archive contents. Native
Angel coordinates and UVs are retained: Y is up, forward is -Z, dimensions are
normally meters. Transform matrices use column vectors and a last-column
translation. Texture paths are relative to the output model.json directory.

Format references (not runtime dependencies):
https://github.com/Dummiesman/angel-file-formats/blob/master/Midtown%20Madness%202/PKG.md
https://github.com/Dummiesman/angel-file-formats/blob/master/General/TEX.md
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import io
import json
import math
from pathlib import Path, PurePosixPath
import re
import struct
import zipfile
import zlib

from PIL import Image


class FormatError(ValueError):
    pass


class Reader:
    def __init__(self, data: bytes):
        self.data = data
        self.pos = 0

    def read(self, size: int) -> bytes:
        if size < 0 or self.pos + size > len(self.data):
            raise FormatError(f"Truncated data at 0x{self.pos:x}, need {size} bytes")
        result = self.data[self.pos:self.pos + size]
        self.pos += size
        return result

    def unpack(self, fmt: str):
        return struct.unpack('<' + fmt, self.read(struct.calcsize('<' + fmt)))

    def scalar(self, fmt: str):
        return self.unpack(fmt)[0]

    def string(self) -> str:
        length = self.scalar('B')
        if not length:
            return ''
        value = self.read(length)
        if value[-1:] != b'\0':
            raise FormatError('Angel string has no terminator')
        return value[:-1].decode('cp1252')


def safe_name(name: str) -> str:
    name = name.replace('\\', '/')
    path = PurePosixPath(name)
    if path.is_absolute() or any(p in ('..', '') for p in path.parts) or ':' in name:
        raise FormatError(f'Unsafe archive path: {name!r}')
    return str(path)


def archive_assets(path: Path) -> dict[str, bytes]:
    """Read MM2 ZIP/AR or MM1 ARES. Refuse genuinely encrypted members."""
    if not zipfile.is_zipfile(path):
        data = path.read_bytes()
        if data[:4] == b'ARES':
            return read_ares(data)
        raise FormatError(f'{path.name}: unsupported AR container {data[:4]!r}')
    assets = {}
    with zipfile.ZipFile(path) as z:
        for info in z.infolist():
            if info.is_dir():
                continue
            name = safe_name(info.filename)
            if info.file_size > 256 * 1024 * 1024:
                raise FormatError(f'Oversized asset: {name}')
            key = name.lower()
            if key in assets:
                raise FormatError(f'Duplicate case-insensitive asset: {name}')
            try:
                assets[key] = z.read(info)
            except (zipfile.BadZipFile, RuntimeError):
                # Some MM2 add-ons deliberately alter local ZIP signatures and
                # set protection bits despite storing plain payloads. Only
                # accept standard plain store/deflate bytes with verified CRC;
                # actual encrypted payloads fail, without guessing passwords.
                assets[key] = verified_zip_payload(path.read_bytes(), info)
    return assets


def verified_zip_payload(data: bytes, info: zipfile.ZipInfo) -> bytes:
    r = Reader(data)
    r.pos = info.header_offset
    header = r.unpack('4s5H3I2H')
    if header[3] != info.compress_type or info.compress_type not in (0, 8):
        raise FormatError(f'Unsupported protected ZIP payload: {info.filename}')
    r.read(header[-2] + header[-1])
    payload = r.read(info.compress_size)
    if info.compress_type == 8:
        try:
            decoder = zlib.decompressobj(-15)
            payload = decoder.decompress(payload, info.file_size + 1)
            if not decoder.eof or decoder.unused_data or decoder.unconsumed_tail:
                raise FormatError(f'Nonstandard or encrypted ZIP payload: {info.filename}')
        except zlib.error as error:
            raise FormatError(f'Encrypted or invalid ZIP payload: {info.filename}') from error
    if len(payload) != info.file_size or zlib.crc32(payload) != info.CRC:
        raise FormatError(f'ZIP payload size/CRC failed: {info.filename}')
    return payload


def read_ares(data: bytes) -> dict[str, bytes]:
    """MM1 resource inode tree, documented by Open1560 stream/vfsystem.h."""
    r = Reader(data)
    if r.read(4) != b'ARES':
        raise FormatError('Invalid ARES magic')
    count, roots, names_size = r.unpack('3I')
    if roots > count or count > 1_000_000:
        raise FormatError('Invalid ARES node count')
    nodes = [r.unpack('3I') for _ in range(count)]
    names = r.read(names_size)
    assets, visited = {}, set()

    def string_at(offset):
        if not 0 <= offset < len(names):
            raise FormatError('Invalid ARES name offset')
        end = names.find(b'\0', offset)
        if end < 0:
            raise FormatError('Unterminated ARES name')
        return names[offset:end].decode('cp1252')

    def walk(start, length, parent):
        if start + length > len(nodes):
            raise FormatError('ARES directory exceeds inode table')
        for index in range(start, start + length):
            if index in visited:
                raise FormatError('ARES directory cycle or overlapping inode')
            visited.add(index)
            offset, size_ext, name_flags = nodes[index]
            name = string_at((name_flags >> 14) & 0x3ffff)
            name = name.replace('\x01', str((name_flags >> 1) & 0x1fff))
            ext = size_ext >> 23
            if ext:
                name += '.' + string_at(ext)
            path = safe_name(parent + name).lower()
            size = size_ext & 0x7fffff
            if name_flags & 1:
                walk(offset, size, path + '/')
            else:
                if offset + size > len(data):
                    raise FormatError(f'ARES file exceeds archive: {path}')
                if path in assets:
                    raise FormatError(f'Duplicate ARES path: {path}')
                assets[path] = data[offset:offset + size]
    walk(0, roots, '')
    return assets


def inventory(path: Path) -> dict:
    result = {'archive': str(path.resolve()), 'size': path.stat().st_size,
              'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
    if not zipfile.is_zipfile(path):
        magic = path.read_bytes()[:4]
        if magic == b'ARES':
            assets = read_ares(path.read_bytes())
            result.update(format='MM1 ARES', supported=True, file_count=len(assets),
                          extensions=dict(Counter(Path(k).suffix.lower() for k in assets)),
                          assets=[{'path': k, 'size': len(v)} for k, v in assets.items()])
        else:
            result.update(format=magic.decode('ascii', 'replace'), supported=False)
        return result
    with zipfile.ZipFile(path) as z:
        files = [i for i in z.infolist() if not i.is_dir()]
        result.update(format='MM2 ZIP/AR', supported=True, file_count=len(files),
                      extensions=dict(Counter(Path(i.filename).suffix.lower() for i in files)),
                      assets=[{'path': safe_name(i.filename), 'size': i.file_size,
                               'compressed_size': i.compress_size, 'encrypted': bool(i.flag_bits & 1)}
                              for i in files])
    return result


def texture_image(data: bytes, extension: str) -> Image.Image:
    if extension.lower() != '.tex':
        return Image.open(io.BytesIO(data)).convert('RGBA')
    r = Reader(data)
    width, height, kind, mips, unknown, flags = r.unpack('5HI')
    if not 0 < width <= 16384 or not 0 < height <= 16384:
        raise FormatError(f'Invalid TEX dimensions: {width}x{height}')
    count = width * height
    if kind in (1, 14, 15, 16):
        palette_count = 16 if kind in (15, 16) else 256
        palette = [r.unpack('4B') for _ in range(palette_count)]
        if palette_count == 16:
            packed = r.read((count + 1) // 2)
            indices = [n for b in packed for n in (b & 15, b >> 4)][:count]
        else:
            indices = r.read(count)
        rgba = bytes(c for i in indices for c in
                     (palette[i][2], palette[i][1], palette[i][0],
                      palette[i][3] if kind in (14, 16) else 255))
        return Image.frombytes('RGBA', (width, height), rgba)
    if kind in (17, 18):
        mode = 'RGB' if kind == 17 else 'RGBA'
        return Image.frombytes(mode, (width, height), r.read(count * len(mode))).convert('RGBA')
    if kind == 6:
        pixels = r.unpack(f'{count}H')
        rgba = bytes(c for p in pixels for c in
                     (((p >> 10) & 31) * 255 // 31, ((p >> 5) & 31) * 255 // 31,
                      (p & 31) * 255 // 31, 255 if p & 0x8000 else 0))
        return Image.frombytes('RGBA', (width, height), rgba)
    if kind in (22, 24, 26):
        # Pillow reads BC textures through a standard DDS wrapper.
        fourcc = {22: b'DXT1', 24: b'DXT3', 26: b'DXT5'}[kind]
        pitch = max(1, (width + 3) // 4) * max(1, (height + 3) // 4) * (8 if kind == 22 else 16)
        dds = b'DDS ' + struct.pack('<7I', 124, 0x81007, height, width, pitch, 0, 1)
        dds += b'\0' * 44 + struct.pack('<2I4s5I', 32, 4, fourcc, 0, 0, 0, 0, 0)
        dds += struct.pack('<5I', 0x1000, 0, 0, 0, 0)
        return Image.open(io.BytesIO(dds + r.read(pitch))).convert('RGBA')
    raise FormatError(f'TEX pixel type {kind} is not supported')


def mm1_texture_image(data: bytes, extension: str) -> Image.Image:
    """Keep MM1 TGA stored-row sampling while retaining native BMS UVs.

    BMS UVs already use the Direct3D top-left convention. MM1 add-on TGA
    assets can mark their rows as bottom-left even though their BMS mapping
    expects the first stored row at V=0 (verified on the Ford GT body/window
    atlas). Pillow honors that descriptor and reverses the rows. Undo only
    that image normalization for MM1; MM2 TEX and ordinary image decoding
    retain their existing convention.
    """
    decoded = texture_image(data, extension)
    if extension.lower() == '.tga' and len(data) >= 18 and not data[17] & 0x20:
        decoded = decoded.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
    return decoded


def read_shaders(r: Reader) -> list[list[dict]]:
    type_and_variants, count = r.unpack('2I')
    byte_colors = bool(type_and_variants & 128)
    variants = type_and_variants & 127
    if not 0 < variants <= 127 or not 0 < count <= 4096:
        raise FormatError('Invalid shader count')
    result = []
    for variant in range(variants):
        materials = []
        for index in range(count):
            texture = r.string()
            if byte_colors:
                colors = [[c / 255 for c in r.unpack('4B')] for _ in range(3)]
                diffuse, ambient, emissive = colors
                specular = [0, 0, 0, 1]
            else:
                diffuse, ambient, specular, emissive = [list(r.unpack('4f')) for _ in range(4)]
            materials.append({'name': texture or f'material_{index}', 'texture': texture or None,
                              'diffuse': diffuse, 'ambient': ambient, 'specular': specular,
                              'emissive': emissive, 'shininess': r.scalar('f')})
        result.append(materials)
    return result


def strip_triangles(indices: list[int]) -> list[tuple[int, int, int]]:
    # Compact Midnight Club strips store winding/end markers in high bits.
    triangles, strip, clockwise = [], [], False
    for raw in indices:
        if not strip:
            clockwise = bool(raw & 0x4000)
        strip.append(raw & 0x3fff)
        if raw & 0x8000:
            for i in range(len(strip) - 2):
                a, b, c = strip[i:i + 3]
                if len({a, b, c}) == 3:
                    triangles.append((a, b, c) if clockwise else (c, b, a))
                    clockwise = not clockwise
            strip = []
    if strip:
        raise FormatError('Unterminated compact triangle strip')
    return triangles


def read_geometry(r: Reader, name: str) -> dict:
    sections, vertices_total, indices_total, sections2, fvf = r.unpack('5I')
    if sections > 65536 or vertices_total > 4_000_000 or indices_total > 16_000_000:
        raise FormatError(f'Invalid geometry counts for {name}')
    if not fvf & 2 or fvf & 0x0c or (fvf >> 8) & 15 > 1:
        raise FormatError(f'Unsupported vertex format {fvf:#x} in {name}')
    positions, normals, uvs, triangles = [], [], [], []
    for section in range(sections):
        strips, flags = r.unpack('2H')
        compact = bool(flags & 0x100)
        count_type = 'H' if compact else 'I'
        material = r.scalar(count_type)
        for strip in range(strips):
            primitive, count = r.unpack(count_type * 2)
            offset = len(positions)
            for _ in range(count):
                positions.append(list(r.unpack('3f')))
                normal = [0, 1, 0]
                uv = [0, 0]
                if fvf & 16:
                    normal = [(v - 128) / 127 for v in r.read(3)] if compact else list(r.unpack('3f'))
                if fvf & 64:
                    r.read(4)
                if fvf & 128:
                    r.read(4)
                if fvf & 256:
                    uv = [v / 128 - 128 for v in r.unpack('2H')] if compact else list(r.unpack('2f'))
                normals.append(normal)
                uvs.append(uv)
            index_count = r.scalar(count_type)
            indices = list(r.unpack(f'{index_count}H'))
            if primitive == 3:
                if index_count % 3:
                    raise FormatError('Incomplete triangle list')
                faces = [indices[i:i + 3] for i in range(0, index_count, 3)]
            elif primitive == 4 and compact:
                faces = strip_triangles(indices)
            else:
                raise FormatError(f'Unsupported primitive {primitive} in {name}')
            for face in faces:
                if any(i >= count for i in face):
                    raise FormatError(f'Triangle index outside vertex strip in {name}')
                if len(set(face)) == 3:
                    triangles.append([*(i + offset for i in face), material])
    if len(positions) != vertices_total:
        raise FormatError(f'Vertex count mismatch in {name}')
    if any(not all(math.isfinite(v) for v in p) for p in positions + uvs):
        raise FormatError(f'Nonfinite position or texture coordinate in {name}')
    normals, repaired = repair_normals(positions, normals, triangles)
    match = re.match(r'^(.*)_(VL|L|M|H)$', name, re.I)
    return {'name': name, 'part': match[1] if match else name,
            'lod': match[2].upper() if match else None,
            'positions': positions, 'normals': normals, 'uvs': uvs,
            'triangles': triangles, 'repaired_normals': repaired}


def repair_normals(positions, normals, triangles):
    """Retain valid authored normals; reconstruct invalid exporter normals."""
    bad = {i for i, n in enumerate(normals)
           if not all(math.isfinite(v) for v in n) or sum(v * v for v in n) < 1e-16}
    accumulated = {i: [0.0, 0.0, 0.0] for i in bad}
    for a, b, c, material in triangles:
        if not bad.intersection((a, b, c)):
            continue
        ab = [positions[b][j] - positions[a][j] for j in range(3)]
        ac = [positions[c][j] - positions[a][j] for j in range(3)]
        cross = [ab[1] * ac[2] - ab[2] * ac[1], ab[2] * ac[0] - ab[0] * ac[2],
                 ab[0] * ac[1] - ab[1] * ac[0]]
        for index in (a, b, c):
            if index in bad:
                for axis in range(3):
                    accumulated[index][axis] += cross[axis]
    for index in bad:
        n = accumulated[index]
        length = math.sqrt(sum(v * v for v in n))
        normals[index] = [v / length for v in n] if length > 1e-10 else [0.0, 1.0, 0.0]
    return normals, len(bad)


def read_pkg(data: bytes) -> dict:
    r = Reader(data)
    version = r.read(4)
    if version not in (b'PKG2', b'PKG3'):
        raise FormatError(f'Unsupported geometry magic {version!r}')
    meshes, variants, references, offset = [], [], [], [0, 0, 0]
    while r.pos < len(data):
        if r.read(4) != b'FILE':
            raise FormatError(f'Missing FILE chunk at {r.pos - 4:#x}')
        name = r.string()
        length = r.scalar('I') if version == b'PKG3' else None
        end = r.pos + length if length is not None else None
        if end is not None and end > len(data):
            raise FormatError(f'Truncated PKG chunk {name}')
        if name.lower() == 'shaders':
            variants = read_shaders(r)
        elif name.lower() == 'offset':
            offset = list(r.unpack('3f'))
        elif name.lower() in ('xref', 'xrefs'):
            for _ in range(r.scalar('I')):
                matrix = list(r.unpack('12f'))
                reference = r.read(32).split(b'\0')[0].decode('cp1252')
                references.append({'name': reference, 'matrix': matrix})
        else:
            meshes.append(read_geometry(r, name))
        if end is not None and r.pos != end:
            raise FormatError(f'PKG chunk {name} consumed {r.pos - end:+d} bytes beyond its length')
    return {'format': version.decode(), 'meshes': meshes, 'variants': variants,
            'offset': offset, 'references': references}


def mm1_normal_table():
    """The 198-direction MM1 quantization lattice (Open1560 packnorm.cpp)."""
    result = [[0, 0, 1], [1, 0, 0], [0, 1, 0], [-1, 0, 0], [0, -1, 0], [0, 0, -1]]
    angles = [(round(math.sin(i * math.pi / 14), 4),
               round(math.cos(i * math.pi / 14), 4)) for i in range(1, 7)]
    for mode in range(12):
        for s, c in angles:
            result.append([[s, 0, c], [0, s, c], [-s, 0, c], [0, -s, c],
                           [s, 0, -c], [0, s, -c], [-s, 0, -c], [0, -s, -c],
                           [c, s, 0], [-s, c, 0], [-c, -s, 0], [s, -c, 0]][mode])
    octant = [[.2279, .2279, .9466], [.4505, .2361, .8610], [.2361, .4505, .8610],
              [.6533, .2450, .7164], [.4691, .4691, .7482], [.2450, .6533, .7164],
              [.8197, .2502, .5153], [.6762, .4815, .5575], [.4815, .6762, .5575],
              [.2502, .8197, .5153], [.9316, .2448, .2685], [.8288, .4740, .2974],
              [.6729, .6729, .3072], [.4740, .8288, .2974], [.2448, .9316, .2685]]
    for signs in ((1, 1, 1), (-1, 1, 1), (-1, -1, 1), (1, -1, 1),
                  (1, 1, -1), (-1, 1, -1), (-1, -1, -1), (1, -1, -1)):
        swap = sum(v < 0 for v in signs) % 2
        for x, y, z in octant:
            if swap:
                x, y = y, x
            result.append([x * signs[0], y * signs[1], z * signs[2]])
    return result


def read_bms(data: bytes, name: str) -> tuple[dict, list[str | None]]:
    """MM1 Binary Mesh Set with adjunct UV/normal data and tri/quad faces."""
    r = Reader(data)
    if r.read(4) != b'3HSM':
        raise FormatError('Invalid MM1 BMS magic')
    origin = list(r.unpack('3f'))
    points_count, adjunct_count, surface_count, index_count = r.unpack('4I')
    radius, radius_squared, bounding_radius = r.unpack('3f')
    texture_count, flags = r.unpack('2B')
    r.read(6)
    if max(points_count, adjunct_count, surface_count, index_count) > 4_000_000:
        raise FormatError('Invalid BMS geometry counts')
    texture_names = [None]
    for _ in range(texture_count):
        texture_names.append(r.read(32).split(b'\0')[0].decode('cp1252'))
        r.read(16)
    points = [list(r.unpack('3f')) for _ in range(points_count)]
    if points_count >= 16:
        r.read(96)
    normal_ids = list(r.read(adjunct_count)) if flags & 2 else []
    uvs = [list(r.unpack('2f')) for _ in range(adjunct_count)] if flags & 1 else [[0, 0] for _ in range(adjunct_count)]
    colors = [list(r.unpack('4B')) for _ in range(adjunct_count)] if flags & 4 else []
    point_ids = r.unpack(f'{adjunct_count}H')
    if flags & 16:
        r.read(16 * surface_count)
    material_ids = list(r.read(surface_count))
    indices = list(r.unpack(f'{index_count}H'))
    if index_count != surface_count * 4 or r.pos != len(data):
        raise FormatError(f'Unexpected BMS face layout in {name}')
    if any(i >= points_count for i in point_ids) or any(i >= adjunct_count for i in indices):
        raise FormatError(f'BMS vertex index out of bounds in {name}')
    if any(i > texture_count for i in material_ids):
        raise FormatError(f'BMS material index out of bounds in {name}')
    positions = [points[i] for i in point_ids]
    normal_table = mm1_normal_table()
    normals = [normal_table[i][:] if i < len(normal_table) else [0, 0, 0] for i in normal_ids]
    if not normal_ids:
        normals = [[0, 0, 0] for _ in positions]
    triangles = []
    for i in range(surface_count):
        a, b, c, d = indices[i * 4:i * 4 + 4]
        material = material_ids[i]
        if len({a, b, c}) == 3:
            triangles.append([a, b, c, material])
        if d != 0 and len({a, c, d}) == 3:
            triangles.append([a, c, d, material])
    normals, repaired = repair_normals(positions, normals, triangles)
    match = re.match(r'^(.*)_(VL|L|M|H)$', name, re.I)
    transform = [[1, 0, 0, origin[0]], [0, 1, 0, origin[1]],
                 [0, 0, 1, origin[2]], [0, 0, 0, 1]]
    mesh = {'name': name.upper(), 'part': match[1].upper() if match else name.upper(),
            'lod': match[2].upper() if match else None, 'positions': positions,
            'normals': normals, 'uvs': uvs, 'triangles': triangles,
            'transform': transform, 'repaired_normals': repaired,
            'source_normal_indices': normal_ids, 'source_point_indices': list(point_ids)}
    if colors:
        mesh['colors'] = [[c[2] / 255, c[1] / 255, c[0] / 255, c[3] / 255] for c in colors]
    return mesh, texture_names


def mm1_model(assets, basename, output, lod):
    prefix = f'bms/{basename}/'.lower()
    meshes, materials, material_lookup = [], [], {}
    for key, data in assets.items():
        if not key.startswith(prefix) or not key.endswith('.bms'):
            continue
        name = Path(key).stem
        if name.upper() in ('H', 'M', 'L', 'VL'):
            continue  # combined preview geometry duplicates body and wheels
        match = re.match(r'^(.*)_(VL|L|M|H)$', name, re.I)
        if match and lod != 'all' and match[2].upper() != lod:
            continue
        mesh, texture_names = read_bms(data, name)
        local_material_map = {}
        for index, texture in enumerate(texture_names):
            texture = texture.lower() if texture else None
            if texture not in material_lookup:
                material_lookup[texture] = len(materials)
                material = {'name': texture or 'untextured', 'texture': None, 'diffuse': [1, 1, 1, 1]}
                if texture:
                    candidates = [f'{folder}/{texture}{ext}' for folder in ('tex', 'tex16a', 'tex16o', 'texp')
                                  for ext in ('.tga', '.dds', '.bmp', '.tex')]
                    texture_key = next((k for k in candidates if k in assets), None)
                    if texture_key:
                        dest = output / 'textures' / (texture + '.png')
                        dest.parent.mkdir(parents=True, exist_ok=True)
                        mm1_texture_image(assets[texture_key], Path(texture_key).suffix).save(dest)
                        material['texture'] = dest.relative_to(output).as_posix()
                        material['source_texture'] = texture_key
                        if Path(texture_key).suffix.lower() == '.tga':
                            material['source_tga_row_order'] = 'stored_rows'
                    else:
                        material['missing_texture'] = True
                materials.append(material)
            local_material_map[index] = material_lookup[texture]
        for triangle in mesh['triangles']:
            triangle[3] = local_material_map[triangle[3]]
        meshes.append(mesh)
    if not meshes:
        raise FormatError(f'No BMS geometry for {basename}')
    return {'version': 1, 'name': basename, 'source_format': 'MM1 BMS',
            'coordinates': {'up': '+Y', 'forward': '-Z', 'units': 'meters',
                            'uv_origin': 'top_left', 'transform_vectors': 'columns'},
            'paintjob': 0, 'paintjob_count': 1, 'meshes': meshes, 'materials': materials,
            'references': [], 'offset': [0, 0, 0]}


def neutral_model(assets: dict[str, bytes], basename: str, output: Path,
                  variant: int = 0, lod: str = 'H') -> dict:
    key = f'geometry/{basename}.pkg'.lower()
    if key not in assets:
        if any(k.startswith(f'bms/{basename}/'.lower()) for k in assets):
            if variant != 0:
                raise FormatError('MM1 BMS reader currently exports the base paintjob only')
            output.mkdir(parents=True, exist_ok=True)
            result = mm1_model(assets, basename, output, lod)
            (output / 'model.json').write_text(json.dumps(result, separators=(',', ':'), allow_nan=False), encoding='utf-8')
            return result
        raise FormatError(f'No {key}; available: {[k for k in assets if k.endswith(".pkg")]}')
    model = read_pkg(assets[key])
    if not 0 <= variant < len(model['variants']):
        raise FormatError(f'Paintjob {variant} outside 0..{len(model["variants"]) - 1}')
    output.mkdir(parents=True, exist_ok=True)
    materials = model['variants'][variant]
    for material in materials:
        texture = material['texture']
        if not texture:
            continue
        candidates = [f'texture/{texture}{ext}'.lower() for ext in ('.tga', '.bmp', '.tex', '.png')]
        texture_key = next((k for k in candidates if k in assets), None)
        if texture_key is None:
            material['source_texture'] = texture
            material['texture'] = None
            material['missing_texture'] = True
            continue
        dest = output / 'textures' / (texture_key.rsplit('/', 1)[-1].rsplit('.', 1)[0] + '.png')
        dest.parent.mkdir(parents=True, exist_ok=True)
        texture_image(assets[texture_key], Path(texture_key).suffix).save(dest)
        material['source_texture'] = texture_key
        material['texture'] = dest.relative_to(output).as_posix()
    meshes = []
    matrix_parts = {'AXLE0', 'AXLE1', 'SHOCK0', 'SHOCK1', 'SHOCK2', 'SHOCK3',
                    'DRIVER', 'ARM0', 'ARM1', 'ARM2', 'ARM3', 'SHAFT2', 'SHAFT3', 'ENGINE'}
    for mesh in model['meshes']:
        if lod != 'all' and mesh['lod'] not in (None, lod):
            continue
        part = mesh['part']
        mtx_key = f'geometry/{basename}_{part}.mtx'.lower()
        transform = [[1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 1, 0], [0, 0, 0, 1]]
        if mtx_key in assets and not (part.upper().startswith('FNDR') and mesh['lod'] != 'H'):
            values = list(Reader(assets[mtx_key]).unpack('12f'))
            if part.upper() in matrix_parts:
                transform = [[values[0], values[3], values[6], values[9]],
                             [values[1], values[4], values[7], values[10]],
                             [values[2], values[5], values[8], values[11]], [0, 0, 0, 1]]
            else:
                for axis in range(3):
                    transform[axis][3] = values[9 + axis]
                mesh['mtx'] = {'min': values[:3], 'max': values[3:6],
                               'pivot': values[6:9], 'origin': values[9:12]}
            mesh['source_mtx'] = mtx_key
        mesh['transform'] = transform
        meshes.append(mesh)
    result = {'version': 1, 'name': basename, 'source_format': model['format'],
              'coordinates': {'up': '+Y', 'forward': '-Z', 'units': 'meters',
                              'uv_origin': 'top_left', 'transform_vectors': 'columns'},
              'paintjob': variant, 'paintjob_count': len(model['variants']),
              'meshes': meshes, 'materials': materials, 'references': model['references'],
              'offset': model['offset']}
    (output / 'model.json').write_text(json.dumps(result, separators=(',', ':'), allow_nan=False), encoding='utf-8')
    return result


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    inv = commands.add_parser('inventory')
    inv.add_argument('directory', type=Path)
    inv.add_argument('--out', type=Path, required=True)
    convert = commands.add_parser('import')
    convert.add_argument('archive', type=Path)
    convert.add_argument('--model', required=True)
    convert.add_argument('--overlay', type=Path, action='append', default=[])
    convert.add_argument('--out', type=Path, required=True)
    convert.add_argument('--paintjob', type=int, default=0)
    convert.add_argument('--lod', choices=['H', 'M', 'L', 'VL', 'all'], default='H')
    extract = commands.add_parser('extract', help='Extract resource assets to an inspection directory')
    extract.add_argument('archive', type=Path)
    extract.add_argument('--out', type=Path, required=True)
    args = parser.parse_args(argv)
    if args.command == 'inventory':
        results = [inventory(path) for path in sorted(args.directory.rglob('*.ar'))]
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(json.dumps(results, indent=2), encoding='utf-8')
        for result in results:
            print(Path(result['archive']).name, result['format'], result.get('file_count', 'unsupported'))
    elif args.command == 'extract':
        assets = archive_assets(args.archive)
        args.out.mkdir(parents=True, exist_ok=True)
        root = args.out.resolve()
        for name, data in assets.items():
            dest = root / safe_name(name)
            if not dest.resolve().is_relative_to(root):
                raise FormatError(f'Asset escapes extraction directory: {name}')
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_bytes(data)
        print(f'{len(assets)} assets -> {root}')
    else:
        assets = archive_assets(args.archive)
        for overlay in args.overlay:
            assets.update(archive_assets(overlay))
        model = neutral_model(assets, args.model, args.out, args.paintjob, args.lod)
        print(f'{len(model["meshes"])} meshes, {sum(len(m["triangles"]) for m in model["meshes"])} triangles, '
              f'{len(model["materials"])} materials, {model["paintjob_count"]} paintjobs -> {args.out / "model.json"}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
