"""Write imported exterior geometry into an MM3 CMP donor hierarchy.

Neutral geometry is measured in metres, Y up, forward -Z, matching mm3view.
Source transforms are conventional column-vector 4x4 matrices. CMP's packed
positions and translations reflect Z. Unknown donor metadata and nodes are
retained; imported wheels receive a dedicated shadow slot. Only explicitly
changed geometry, bounds and wheel translations are rewritten. No game files
are installed here.

Format reference: Daniel Stien's gameformats/mm3/cmpviewer, revision
48b78d1676e4c39eaa2584544533efdf3a4a18ba.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass, field
import json
import math
from pathlib import Path
import struct
from typing import Iterable, Mapping

import numpy as np


VERSIONS = (109, 114, 115)
MAX_PACKED_MATERIAL_INDEX = 23  # A biased uint8 can encode 24 distinct IDs.
# The title's car shader reads colors at c[112 + ID]; c[135] is a reserved
# lighting constant. Thus only IDs 0..22 are usable by this exterior writer.
MAX_MATERIAL_INDEX = 22
# Packed word 5 is fetched as D3DCOLOR: v5=(byte2,byte1,byte0,byte3)/255.
# The retail car shader uses byte2 as constant environment intensity and
# byte1 as its Fresnel multiplier. Byte0 contributes to diffuse coverage;
# byte3 controls an additional reflection/specular term. The old viewer's
# "specular power / ambient" labels do not describe this native shader.
# Start imports fully covered and matte instead of baking in reflections.
MATTE_VERTEX_SHADING = (255, 0, 0, 0)
MAX_MESH_INDICES = 65535
MAX_UV_TILES_PER_TRIANGLE = 4096
MAX_UV_SPLIT_TILES = 100000
WHEEL_NODES = {
    "tireFrontLeft": "WHL0_H",
    "tireFrontRight": "WHL1_H",
    "tireBackLeft": "WHL2_H",
    "tireBackRight": "WHL3_H",
}


class FormatError(ValueError):
    pass


class Reader:
    def __init__(self, data: bytes):
        self.data, self.pos = data, 0

    def take(self, size: int) -> bytes:
        if size < 0 or self.pos + size > len(self.data):
            raise FormatError(f"CMP truncated at {self.pos}, requesting {size} bytes")
        start = self.pos
        self.pos += size
        return self.data[start:self.pos]

    def unpack(self, fmt: str):
        values = struct.unpack("<" + fmt, self.take(struct.calcsize("<" + fmt)))
        return values[0] if len(values) == 1 else values

    def string(self) -> str:
        try:
            end = self.data.index(b"\0", self.pos)
        except ValueError as exc:
            raise FormatError("Unterminated CMP string") from exc
        return self.take(end + 1 - self.pos)[:-1].decode("latin-1")


def cstring(value: str) -> bytes:
    if "\0" in value:
        raise ValueError("Embedded NUL in model name")
    return value.encode("latin-1") + b"\0"


@dataclass
class MeshBlock:
    name: str
    payload: bytes

    @classmethod
    def read(cls, reader: Reader) -> "MeshBlock":
        name = reader.string()
        return cls(name, reader.take(reader.unpack("I")))

    def write(self) -> bytes:
        return cstring(self.name) + struct.pack("<I", len(self.payload)) + self.payload


@dataclass
class Node:
    kind: int
    name: str
    prefix: bytes
    children: list["Node"] = field(default_factory=list)
    meshes: list[MeshBlock] = field(default_factory=list)
    terminator: bytes = b""
    transform_offset: int | None = None
    bounds_offset: int | None = None
    secondary_bounds_offset: int | None = None

    def write(self) -> bytes:
        out = bytearray(self.prefix)
        if self.kind in (0, 1):
            out += struct.pack("<I", len(self.children))
            out += b"".join(child.write() for child in self.children)
        elif self.kind in (2, 6):
            out += b"".join(b"\1" + mesh.write() for mesh in self.meshes)
            out += self.terminator
        return bytes(out)

    def walk(self, path=""):
        path += "/" + self.name
        yield path, self
        for child in self.children:
            yield from child.walk(path)

    def relative_matrix(self) -> np.ndarray:
        if self.transform_offset is None:
            return np.eye(4)
        # Match cmpviewer/mm3view: linear rows as stored, translation Z reflected.
        off = self.transform_offset + 4 + 48
        values = np.array(struct.unpack_from("<12f", self.prefix, off)).reshape(4, 3)
        matrix = np.eye(4)
        matrix[:3, :3] = values[:3]
        matrix[:3, 3] = values[3] * (1, 1, -1)
        return matrix

    def set_translation(self, world: np.ndarray, relative: np.ndarray):
        if self.transform_offset is None:
            raise FormatError(f"{self.name} has no transform")
        raw = bytearray(self.prefix)
        for delta, value in ((4 + 36, world), (4 + 48 + 36, relative)):
            struct.pack_into("<3f", raw, self.transform_offset + delta,
                             *np.asarray(value) * (1, 1, -1))
        self.prefix = bytes(raw)

    def set_bounds(self, bounds: np.ndarray):
        raw = bytearray(self.prefix)
        raw_bounds = native_bounds(bounds)
        for offset in (self.bounds_offset, self.secondary_bounds_offset):
            if offset is not None:
                struct.pack_into("<6f", raw, offset, *raw_bounds.ravel())
        self.prefix = bytes(raw)


@dataclass
class Model:
    version: int
    root: Node
    trailing: bytes = b""

    def write(self) -> bytes:
        return self.root.write() + self.trailing


def read_model(data: bytes) -> Model:
    """Lossless structural parse, including shadow and reference-only blocks."""
    r = Reader(data)
    if r.unpack("I") != 0:
        raise FormatError("Not a CMP root")
    version = r.unpack("I")
    if version not in VERSIONS:
        raise FormatError(f"Unsupported CMP version {version}")
    name = r.string()
    r.take(4)
    bounds_offset = r.pos
    r.take(24 + 4 + 2 + 1)
    if version >= 114:
        r.string()
    transform_offset = r.pos
    r.take(116 + 4 + 1)
    secondary_bounds_offset = r.pos
    r.take(24)
    entries = r.unpack("I")
    r.take(24 + 16 * entries + 4)
    root = Node(0, name, data[:r.pos], transform_offset=transform_offset,
                bounds_offset=bounds_offset, secondary_bounds_offset=secondary_bounds_offset)

    def children(parent: Node):
        count = r.unpack("I")
        if count > 100000:
            raise FormatError("Unreasonable CMP child count")
        for _ in range(count):
            start = r.pos
            kind, name = r.unpack("I"), r.string()
            transform_off = bound_off = None
            if kind == 1:
                transform_off = r.pos - start
                r.take(116 + 4)
                bound_off = r.pos - start
                r.take(24)
            elif kind in (2, 6):
                r.take(16)
                if kind == 6:
                    bound_off = r.pos - start
                    r.take(24)
            elif kind == 4:
                r.take(32 + (8 if version >= 114 else 0))
            elif kind == 5:
                r.take(4)
            elif kind != 3:
                raise FormatError(f"Unsupported CMP node kind {kind}")
            node = Node(kind, name, data[start:r.pos], transform_offset=transform_off,
                        bounds_offset=bound_off)
            parent.children.append(node)
            if kind == 1:
                children(node)
            elif kind in (2, 6):
                for _ in range(3 if kind == 6 else 2):
                    flag = r.take(1)
                    if flag == b"\0":
                        node.terminator = flag
                        break
                    if flag != b"\1":
                        raise FormatError("Unsupported CMP mesh-presence flag")
                    node.meshes.append(MeshBlock.read(r))

    children(root)
    return Model(version, root, data[r.pos:])


def native_bounds(bounds) -> np.ndarray:
    b = np.asarray(bounds, dtype=float).copy()
    b[:, 2] = -b[::-1, 2]
    return b


def _signed_pack(values: np.ndarray, bits: int) -> np.ndarray:
    scale = 1 << (bits - 1)
    q = np.rint(values * scale).astype(np.int64)
    q = np.clip(q, -scale, scale - 1)
    return (q & ((1 << bits) - 1)).astype(np.uint32)


def _three_fields(values: np.ndarray) -> np.ndarray:
    return (_signed_pack(values[:, 0], 11) |
            (_signed_pack(values[:, 1], 11) << 11) |
            (_signed_pack(values[:, 2], 10) << 22))


def normalize_repeat_uv(positions, normals, uvs, triangles):
    """Put authored repeating UVs in 0..1 without changing texture samples.

    Retail car textures use nonnegative UVs, and their native sampler may
    clamp outside 0..1. The signed packed range therefore is not a safe target
    for imported negative UVs. An integer translation changes no repeating
    source texture sample. Triangles crossing an integer seam are clipped,
    interpolating position and normal; shared vertices remain shared when
    their translated attributes agree.
    """
    p, n, uv = (np.asarray(a, dtype=float) for a in (positions, normals, uvs))
    tris = np.asarray(triangles, dtype=np.int64).reshape(-1, 4)
    if uv.min() >= 0 and uv.max() <= 1:
        return p, n, uv, tris
    # Corrupt UV decoding can turn a car triangle into hundreds of repeats
    # along both axes. Reject that work before generating millions of pieces.
    # Ordinary negative/repeated UVs translate or cross only a few tiles.
    triangle_uv = uv[tris[:, :3]]
    minima, maxima = triangle_uv.min(1), triangle_uv.max(1)
    needs_clip = (np.ceil(maxima - 1 - 1e-8) > np.floor(minima + 1e-8)).any(1)
    tile_counts = np.maximum(1, np.ceil(maxima) - np.floor(minima)).prod(1)
    if ((tile_counts[needs_clip] > MAX_UV_TILES_PER_TRIANGLE).any() or
            tile_counts[needs_clip].sum() > MAX_UV_SPLIT_TILES):
        raise ValueError("UV seam clipping crosses too many repeat tiles; check source UV decoding")
    attributes = np.concatenate((p, n, uv), axis=1)
    out, out_tris, by_value = [], [], {}

    def emit(polygon, material):
        indices = []
        for vertex in polygon:
            # Include material: batches require their vertices' own material ID.
            key = (int(material), *np.round(vertex, 12))
            if key not in by_value:
                by_value[key] = len(out)
                out.append(vertex)
            indices.append(by_value[key])
        for i in range(1, len(indices) - 1):
            a, b, c = indices[0], indices[i], indices[i + 1]
            if len({a, b, c}) == 3:
                out_tris.append((a, b, c, int(material)))

    def clip(polygon, coordinate, threshold, below):
        result = []
        if not polygon:
            return result
        last = polygon[-1]
        last_in = last[coordinate] <= threshold if below else last[coordinate] >= threshold
        for current in polygon:
            inside = current[coordinate] <= threshold if below else current[coordinate] >= threshold
            if inside != last_in:
                t = (threshold - last[coordinate]) / (current[coordinate] - last[coordinate])
                result.append(last + t * (current - last))
            if inside:
                result.append(current)
            last, last_in = current, inside
        return result

    for triangle in tris:
        polygon = attributes[triangle[:3]].copy()
        minima, maxima = polygon[:, 6:8].min(0), polygon[:, 6:8].max(0)
        lower = np.ceil(maxima - 1.0 - 1e-8).astype(int)
        upper = np.floor(minima + 1e-8).astype(int)
        if np.all(lower <= upper):
            shift = np.clip(np.rint((minima + maxima) / 2 - .5).astype(int), lower, upper)
            polygon[:, 6:8] -= shift
            emit(polygon, triangle[3])
            continue
        # Split only when one integer shift cannot contain the entire triangle.
        u_start, v_start = math.floor(minima[0]), math.floor(minima[1])
        for tile_u in range(u_start, max(u_start + 1, math.ceil(maxima[0]))):
            for tile_v in range(v_start, max(v_start + 1, math.ceil(maxima[1]))):
                part = list(polygon)
                for coordinate, tile in ((6, tile_u), (7, tile_v)):
                    part = clip(part, coordinate, tile, False)
                    part = clip(part, coordinate, tile + 1, True)
                if len(part) < 3:
                    continue
                part = np.asarray(part)
                part[:, 6:8] -= (tile_u, tile_v)
                emit(part, triangle[3])
    if not out_tris:
        raise ValueError("UV clipping produced no triangles")
    out = np.asarray(out)
    return out[:, :3], out[:, 3:6], out[:, 6:8], np.asarray(out_tris, dtype=np.int64)


def split_mesh_geometry(positions, normals, uvs, triangles):
    """Return native-sized parts without dropping any authored triangles.

    Split after UV seam clipping, because clipping can increase the index
    count. Each independent part has at most 65535 indices and material-split
    vertices. Parts become sibling mesh nodes. A body's second mesh slot is
    its shadow pass, so putting visible parts in successive slots loses faces.
    """
    p, n, uv, tris = normalize_repeat_uv(positions, normals, uvs, triangles)
    if len(tris) * 3 <= MAX_MESH_INDICES:
        return [(p, n, uv, tris)]
    parts = []
    for start in range(0, len(tris), MAX_MESH_INDICES // 3):
        part = tris[start:start + MAX_MESH_INDICES // 3].copy()
        indices, inverse = np.unique(part[:, :3], return_inverse=True)
        part[:, :3] = inverse.reshape(-1, 3)
        parts.append((p[indices], n[indices], uv[indices], part))
    return parts


def mesh_metadata(block: MeshBlock, version: int) -> dict:
    """Locate packed buffers and preserve the donor's runtime metadata."""
    if not block.payload:
        raise FormatError(f"Mesh {block.name} is a reference, not a template")
    r = Reader(block.payload)
    radius = r.take(4)
    bounds = r.take(24)
    vertex_count, index_count = r.unpack("2I")
    flags = r.take(12)
    color = r.take(16 if version >= 115 else 4)
    path = r.string()
    has_indices = r.unpack("B")
    index_flags = b"\0" * 4
    if has_indices:
        index_flags = r.take(4)
        index_length = r.unpack("I")
        r.take(index_length)
    buffer_flags = r.take(5)
    vc2, stride, length = r.unpack("3I")
    vertex_flags = r.take(4)
    vertex_offset = r.pos
    r.take(length)
    if vc2 != vertex_count or stride != 24 or length != vertex_count * 24:
        raise FormatError(f"Invalid packed vertex buffer in {block.name}")
    primitives = []
    for _ in range(r.unpack("I") // 2):
        kind = r.unpack("H")
        if kind == 0x6001:
            r.take(4)
        elif kind != 0x8801:
            raise FormatError(f"Unknown CMP primitive {kind:#x}")
        r.take(14)
        primitives.append(kind)
    material_count_offset = r.pos
    count = r.unpack("I")
    material_offsets = [r.pos + i * 21 + 17 for i in range(count)]
    r.take(count * 21)
    return dict(radius=radius, bounds=bounds, flags=flags, color=color, path=path,
                index_flags=index_flags, buffer_flags=buffer_flags,
                vertex_flags=vertex_flags, vertex_offset=vertex_offset,
                vertex_count=vertex_count, material_offsets=material_offsets)


def remap_mesh_materials(block: MeshBlock, version: int,
                         material_map: Mapping[int, int]) -> MeshBlock:
    """Change retained interior material slots without changing its geometry."""
    meta = mesh_metadata(block, version)
    out = bytearray(block.payload)
    for i in range(meta["vertex_count"]):
        offset = meta["vertex_offset"] + i * 24 + 12
        old = out[offset] // 11
        value = material_map.get(old, old)
        if not 0 <= value <= MAX_MATERIAL_INDEX:
            raise ValueError("Material slot cannot fit in packed MM3 vertices")
        # Low bits are a shader palette base, not part of the material index.
        out[offset] = value * 11 + out[offset] % 11
    for offset in meta["material_offsets"]:
        old = struct.unpack_from("<I", out, offset)[0]
        struct.pack_into("<I", out, offset, material_map.get(old, old))
    return MeshBlock(block.name, bytes(out))


def encode_mesh(name: str, positions, normals, uvs, triangles,
                material_map: Mapping[int, int], template: MeshBlock,
                version: int, *, specular=MATTE_VERTEX_SHADING,
                flip_winding=False, material_palette_offsets=None,
                matrix_palette_offset=4) -> MeshBlock:
    """Indexed triangle-list CMP mesh. Vertices are split at material borders.

    The legacy ``specular`` argument supplies packed word-5 bytes in file
    order: coverage, Fresnel reflection, constant reflection, specular term.
    This defaults to the retail matte behavior, preserving texture colors.
    """
    p = np.asarray(positions, dtype=float)
    n = np.asarray(normals, dtype=float)
    uv = np.asarray(uvs, dtype=float)
    tris = np.asarray(triangles, dtype=np.int64).reshape(-1, 4)
    if p.ndim != 2 or p.shape[1] != 3 or n.shape != p.shape or uv.shape != (len(p), 2):
        raise ValueError("Invalid positions, normals or UV dimensions")
    if not len(p) or not len(tris) or not all(np.isfinite(a).all() for a in (p, n, uv)):
        raise ValueError("Geometry must be nonempty and finite")
    if (tris[:, :3] < 0).any() or (tris[:, :3] >= len(p)).any():
        raise ValueError("Triangle references an invalid vertex")
    p, n, uv, tris = normalize_repeat_uv(p, n, uv, tris)
    if len(tris) * 3 > MAX_MESH_INDICES:
        raise ValueError("CMP primitive offsets support at most 65535 indices per mesh")
    meta = mesh_metadata(template, version)
    groups = {}
    for triangle in tris:
        src_material = int(triangle[3])
        material = int(material_map.get(src_material, src_material))
        if not 0 <= material <= MAX_MATERIAL_INDEX:
            raise ValueError(f"Material slot {material} exceeds MM3 car shader limit 22")
        groups.setdefault(material, []).append(triangle[:3])
    vertex_keys, packed_indices, batches = [], [], []
    for material, group in sorted(groups.items()):
        local = {}
        start_vertex = len(vertex_keys)
        start_index = len(packed_indices)
        for tri in group:
            for src_index in tri[::-1] if flip_winding else tri:
                src_index = int(src_index)
                if src_index not in local:
                    local[src_index] = len(vertex_keys)
                    vertex_keys.append((src_index, material))
                packed_indices.append(local[src_index])
        batches.append((material, start_vertex, len(local), start_index, len(group)))
    if len(vertex_keys) > 65535:
        raise ValueError("CMP mesh exceeds 65535 packed vertices")
    src_indices = np.array([key[0] for key in vertex_keys])
    materials = np.array([key[1] for key in vertex_keys], dtype=np.uint32)
    p, n, uv = p[src_indices], n[src_indices], uv[src_indices]
    n /= np.linalg.norm(n, axis=1, keepdims=True) + 1e-30
    # Include the origin: packed positions have no AABB-min bias.
    lo, hi = np.minimum(p.min(0), 0), np.maximum(p.max(0), 0)
    pad = np.maximum((hi - lo) * 0.001, 0.00001)
    bounds = np.array([lo - pad, hi + pad])
    size = bounds[1] - bounds[0]
    rawp, rawn = p.copy(), n.copy()
    rawp[:, 2] *= -1
    rawn[:, 2] *= -1
    words = np.zeros((len(p), 6), dtype="<u4")
    words[:, 0] = _three_fields(rawp / size)
    words[:, 1] = _three_fields(rawn)
    words[:, 2] = _signed_pack(uv[:, 0], 11) | (_signed_pack(uv[:, 1], 11) << 11)
    # Native shaders multiply normalized palette bytes into table indices.
    # Retail textured vertices use material * 11 + 2, ordinary transforms
    # include a base bias of 4. The bias avoids truncation to the prior entry.
    # mm3view divides these bytes by their strides and therefore cannot detect
    # a missing base offset, despite decoding the same logical material IDs.
    palette_offsets = material_palette_offsets or {}
    bases = np.array([int(palette_offsets.get(int(mid), 2)) for mid in materials], dtype=np.uint32)
    if (bases > 2).any() or not 0 <= matrix_palette_offset <= 255:
        raise ValueError("Unsupported native shader palette base")
    words[:, 3] = materials * 11 + bases + (int(matrix_palette_offset) << 8)
    words[:, 5] = sum((int(value) & 255) << (8 * i) for i, value in enumerate(specular))
    index_buffer = np.asarray(packed_indices, dtype="<u2").tobytes()
    vertex_buffer = words.tobytes()
    out = bytearray(struct.pack("<f", float(np.linalg.norm(p, axis=1).max())))
    out += struct.pack("<6f", *native_bounds(bounds).ravel())
    out += struct.pack("<2I", len(p), len(packed_indices)) + meta["flags"] + meta["color"]
    out += cstring(name) + b"\1" + meta["index_flags"]
    out += struct.pack("<I", len(index_buffer)) + index_buffer + meta["buffer_flags"]
    out += struct.pack("<3I", len(p), 24, len(vertex_buffer)) + meta["vertex_flags"] + vertex_buffer
    out += struct.pack("<I", len(batches) * 2)
    for material, first, count, offset, tri_count in batches:
        out += struct.pack("<5H5H", 0x6001, first, count - 1, offset, tri_count - 1, 0, 0, 0, 0, 0)
    out += struct.pack("<I", len(batches))
    for material, first, count, offset, tri_count in batches:
        out += struct.pack("<4IBI", first, count, offset, tri_count, 0, material)
    if version >= 115:
        out += struct.pack("<I", 0)  # Imported visual mesh has no native number plate.
    return MeshBlock(name, bytes(out))


def _neutral_world(mesh: dict, source_transform: np.ndarray):
    matrix = source_transform @ np.asarray(mesh.get("transform", np.eye(4)), dtype=float)
    p = np.asarray(mesh["positions"], dtype=float)
    n = np.asarray(mesh["normals"], dtype=float)
    return (p @ matrix[:3, :3].T + matrix[:3, 3],
            n @ np.linalg.inv(matrix[:3, :3]),
            np.asarray(mesh["uvs"], dtype=float),
            np.asarray(mesh["triangles"], dtype=np.int64), matrix[:3, 3])


def is_shadow_mesh(block: MeshBlock) -> bool:
    """Recognize the dedicated shadow slot in the supported retail donors."""
    return block.name.replace("/", "\\").rsplit("\\", 1)[-1].lower().endswith("shadow.dob")


def shadow_volume_geometry(positions):
    """Build a closed, bounded stencil proxy from the imported point cloud.

    ObjectStencil.cvsx extrudes vertices according to their facet normal's
    light-facing sign. Surface triangles alone leave holes: the retail meshes
    also join adjacent facets with collapsed edge quads that open into walls.
    Use a convex proxy to avoid open/nonmanifold imported bodywork. At most 482
    deterministic support samples produce 960 caps plus 2,880 wall triangles.
    Concave recesses are approximated; the six coordinate extrema survive.

    Returned faces have native MM3 winding (cross product opposite the outward
    normal), independent of the source's face ordering or smoothed normals.
    """
    from scipy.spatial import ConvexHull

    p = np.asarray(positions, dtype=float).reshape(-1, 3)
    if not len(p) or not np.isfinite(p).all():
        raise ValueError("Shadow positions must be nonempty and finite")
    p = np.unique(p, axis=0)
    # Flat or line-like helper geometry still needs a closed proxy. Add only a
    # sub-millimetre thickness along missing dimensions, not a car-sized box.
    _, singular, basis = np.linalg.svd(p - p.mean(0), full_matrices=len(p) < 3)
    rank = np.count_nonzero(singular > max(np.ptp(p, axis=0).max(), 1) * 1e-9)
    if rank < 3:
        thickness = max(np.ptp(p, axis=0).max() * 1e-5, 1e-4)
        for axis in basis[rank:]:
            p = np.concatenate((p + axis * thickness, p - axis * thickness))
    directions = [[0., -1., 0.], [0., 1., 0.]]
    for latitude in range(1, 16):
        polar = math.pi * latitude / 16
        for longitude in range(32):
            azimuth = 2 * math.pi * longitude / 32
            directions.append([math.sin(polar) * math.cos(azimuth),
                               math.cos(polar), math.sin(polar) * math.sin(azimuth)])
    # Set axis directions exactly, avoiding tie-breaking noise at cos(pi/2).
    directions = np.asarray(directions)
    directions[np.abs(directions) < 1e-14] = 0
    selected = np.unique(np.concatenate([np.argmax(p @ batch.T, axis=0)
                                        for batch in np.array_split(directions, 16)]))
    p = p[selected]
    hull = ConvexHull(p)
    caps = hull.simplices.copy()
    normals = hull.equations[:, :3]
    cross = np.cross(p[caps[:, 1]] - p[caps[:, 0]], p[caps[:, 2]] - p[caps[:, 0]])
    outward = np.einsum("ij,ij->i", cross, normals) > 0
    caps[outward] = caps[outward, ::-1]
    # A distinct vertex for each facet keeps every cap rigid under extrusion.
    points = p[caps].reshape(-1, 3)
    vertex_normals = np.repeat(normals, 3, axis=0)
    faces = np.arange(len(points)).reshape(-1, 3).tolist()
    edges = {}
    for face, cap in enumerate(caps):
        for corner in range(3):
            a, b = int(cap[corner]), int(cap[(corner + 1) % 3])
            local = (face * 3 + corner, face * 3 + (corner + 1) % 3)
            key = (min(a, b), max(a, b))
            edges.setdefault(key, []).append((a, b, *local))
    for adjacent in edges.values():
        if len(adjacent) != 2 or adjacent[0][:2] != adjacent[1][1::-1]:
            raise FormatError("Convex shadow proxy has an unpaired or inconsistent edge")
        (_, _, a, b), (_, _, c, d) = adjacent
        # b and c coincide; a and d coincide. After different facet extrusions
        # this quad closes the gap, with its boundary opposite both caps.
        faces.extend(((b, a, d), (b, d, c)))
    tris = np.column_stack((np.asarray(faces), np.zeros(len(faces), dtype=int)))
    return points, vertex_normals, tris


def encode_shadow_mesh(name: str, positions, normals, triangles,
                       template: MeshBlock, version: int, *, flip_winding=False) -> MeshBlock:
    """Encode a closed convex stencil proxy with exact native attributes.

    The API retains source normals/winding arguments for callers, but a hull
    computes its own outward facet normals and native winding. An empty point
    cloud emits a zero-area placeholder for additional visible split siblings;
    their first sibling owns the complete body/wheel proxy.
    """
    meta = mesh_metadata(template, version)
    offset, count = meta["vertex_offset"], meta["vertex_count"]
    donor_words = np.frombuffer(template.payload, dtype="<u4", count=count * 6,
                               offset=offset).reshape(-1, 6)
    if not count or not np.all(donor_words[:, 2:] == donor_words[0, 2:]):
        raise FormatError(f"Unsupported varying shadow attributes in {template.name}")
    attributes = donor_words[0, 2:].copy()
    if len(positions):
        p, n, tris = shadow_volume_geometry(positions)
    else:
        p, n, tris = np.zeros((3, 3)), np.tile((0, 1, 0), (3, 1)), [[0, 1, 2, 0]]
    encoded = encode_mesh(name, p, n, np.zeros((len(p), 2)), tris, {}, template, version)
    result = bytearray(encoded.payload)
    encoded_meta = mesh_metadata(encoded, version)
    words = np.frombuffer(result, dtype="<u4", count=encoded_meta["vertex_count"] * 6,
                          offset=encoded_meta["vertex_offset"]).reshape(-1, 6)
    words[:, 2:] = attributes
    return MeshBlock(name, bytes(result))


def convert_exterior(template: bytes, source: dict, *, body_names=("BODY_H",),
                     wheel_names: Mapping[str, str] | None = None,
                     material_map: Mapping[int, int] | None = None,
                     interior_material_map: Mapping[int, int] | None = None,
                     source_transform=None, keep_interior=True,
                     flip_winding=True, specular=MATTE_VERTEX_SHADING,
                     material_palette_offsets=None) -> tuple[bytes, dict]:
    """Replace body/wheels in a donor. Returns bytes and a conversion report.

    Visible meshes receive the imported geometry; a closed convex stencil proxy
    follows each complete body and wheel. Geometry exceeding a native 16-bit
    buffer is split into additional sibling mesh nodes under the same animated
    parent; only the first sibling casts the complete component shadow. Other exterior
    mesh nodes receive one valid zero-area triangle so their native gameplay
    references remain intact.
    Source wheel transform origins become the donor's animated wheel pivots.
    A separate call with the far donor handles the game's *_lod1 model.
    Neutral source faces follow their normals (CCW); native MM3 faces require
    the opposite cross-product sign in this coordinate convention, so winding
    is reversed by default. Set flip_winding=False for already native faces.
    """
    model = read_model(template)
    wheel_names = dict(WHEEL_NODES if wheel_names is None else wheel_names)
    material_map = dict(material_map or {})
    interior_material_map = dict(interior_material_map or {})
    transform = np.eye(4) if source_transform is None else np.asarray(source_transform, dtype=float)
    if transform.shape != (4, 4) or not np.isfinite(transform).all():
        raise ValueError("source_transform must be a finite 4x4 matrix")
    source_meshes = {mesh["name"]: mesh for mesh in source["meshes"]}
    if len(source_meshes) != len(source["meshes"]):
        raise ValueError("Duplicate neutral mesh names")
    selected = set(body_names) | set(wheel_names.values())
    missing = selected.copy()
    missing -= source_meshes.keys()
    if missing:
        raise ValueError(f"Missing source meshes: {', '.join(sorted(missing))}")
    # Helper/shadow/light meshes can be empty or carry transforms that are
    # irrelevant to this exterior. Only selected geometry needs conversion.
    global_meshes = {name: _neutral_world(source_meshes[name], transform) for name in selected}
    body_p, body_n, body_uv, body_tri = [], [], [], []
    for name in body_names:
        p, n, uv, tri, _ = global_meshes[name]
        tri = tri.copy()
        tri[:, :3] += sum(len(x) for x in body_p)
        body_p.append(p); body_n.append(n); body_uv.append(uv); body_tri.append(tri)
    body = tuple(np.concatenate(x) for x in (body_p, body_n, body_uv, body_tri))
    geometry_parts = {"body": split_mesh_geometry(*body)}
    geometry_parts.update({name: split_mesh_geometry(*global_meshes[source_name][:4])
                           for name, source_name in wheel_names.items()})
    all_positions = np.concatenate([body[0]] + [global_meshes[n][0] for n in wheel_names.values()])
    report = dict(version=model.version, replaced=[], hidden=[], retained=[], shadows=[],
                  split_meshes=[], wheel_pivots={}, bounds_min=all_positions.min(0).tolist(),
                  bounds_max=all_positions.max(0).tolist())
    found_body, found_wheels = False, set()
    fallback = next((block for _, node in model.root.walk() for block in node.meshes if block.payload), None)
    if fallback is None:
        raise FormatError("Donor has no mesh template")
    shadow_template = next((b for _, node in model.root.walk() for b in node.meshes
                            if b.payload and is_shadow_mesh(b)), None)
    if shadow_template is None:
        raise FormatError("Donor has no native stencil shadow template")

    def visit(node: Node, parent_matrix: np.ndarray, path: str, wheel=None):
        nonlocal found_body
        siblings = []
        path += "/" + node.name
        current_matrix = parent_matrix @ node.relative_matrix()
        if node.kind == 1 and node.name in wheel_names:
            wheel = node.name
            pivot = global_meshes[wheel_names[wheel]][4]
            relative_pivot = np.linalg.solve(parent_matrix[:3, :3], pivot - parent_matrix[:3, 3])
            node.set_translation(pivot, relative_pivot)
            current_matrix = parent_matrix @ node.relative_matrix()
            report["wheel_pivots"][wheel] = pivot.tolist()
        if node.kind == 1 and (node.name.lower() in ("car", "body") or node.name in wheel_names):
            points = (global_meshes[wheel_names[node.name]][0] if node.name in wheel_names
                      else body[0] if node.name.lower() == "body" else all_positions)
            parent_points = ((points - parent_matrix[:3, 3]) @
                             np.linalg.inv(parent_matrix[:3, :3]).T)
            node.set_bounds(np.array([parent_points.min(0), parent_points.max(0)]))
        if node.kind in (2, 6):
            if not node.meshes:
                return []
            retained = keep_interior and "lowdash" in path.lower()
            if retained:
                report["retained"].append(path)
                if interior_material_map:
                    node.meshes = [remap_mesh_materials(block, model.version, interior_material_map)
                                   if block.payload else block for block in node.meshes]
            else:
                if wheel:
                    parts = geometry_parts[wheel]
                    found_wheels.add(wheel)
                elif "/body/" in path.lower():
                    parts = geometry_parts["body"]
                    found_body = True
                else:
                    p = np.zeros((3, 3)); n = np.tile((0, 1, 0), (3, 1))
                    uv = np.zeros((3, 2)); tri = np.array([[0, 1, 2, 0]])
                    parts = [(p, n, uv, tri)]
                    report["hidden"].append(path)
                if wheel or "/body/" in path.lower():
                    report["replaced"].append(path)
                donor_block = next((b for b in node.meshes if b.payload), fallback)
                original_meshes, original_prefix = list(node.meshes), node.prefix
                added_shadow = bool(wheel and len(original_meshes) == 1)
                if added_shadow:
                    original_meshes.append(shadow_template)
                    node.terminator = b"" if node.kind == 2 else b"\0"
                if len(parts) > 1:
                    report["split_meshes"].append(dict(node=path, parts=len(parts),
                        triangles=[int(len(part[3])) for part in parts]))
                for part_index, (p, n, uv, tri) in enumerate(parts):
                    target = node
                    if part_index:
                        name = f"{node.name}_mm3import_part{part_index}"
                        delta = len(cstring(name)) - len(cstring(node.name))
                        prefix = struct.pack("<I", node.kind) + cstring(name)
                        prefix += original_prefix[4 + len(cstring(node.name)):]
                        target = Node(node.kind, name, prefix,
                            terminator=node.terminator,
                            bounds_offset=(node.bounds_offset + delta
                                           if node.bounds_offset is not None else None))
                        siblings.append(target)
                    localp = (p - current_matrix[:3, 3]) @ np.linalg.inv(current_matrix[:3, :3]).T
                    localn = n @ current_matrix[:3, :3]
                    target.set_bounds(np.array([localp.min(0), localp.max(0)]))
                    new_meshes = []
                    for slot, original_mesh in enumerate(original_meshes):
                        directory = donor_block.name.rsplit("\\", 1)[0] if "\\" in donor_block.name else ""
                        shadow = is_shadow_mesh(original_mesh)
                        suffix = "shadow" if shadow else str(slot)
                        name = f"mm3import_{target.name}_{wheel or 'body'}_{suffix}.dob"
                        if directory:
                            name = directory + "\\" + name
                        if shadow:
                            complete = (global_meshes[wheel_names[wheel]][0] if wheel else body[0])
                            shadowp = ((complete - current_matrix[:3, 3]) @
                                       np.linalg.inv(current_matrix[:3, :3]).T) if part_index == 0 else []
                            block = encode_shadow_mesh(name, shadowp, localn, tri,
                                original_mesh, model.version, flip_winding=flip_winding)
                            new_meshes.append(block)
                            report["shadows"].append(dict(node=path.rsplit("/", 1)[0] + "/" + target.name,
                                slot=slot, template=original_mesh.name,
                                proxy="convex_stencil" if part_index == 0 else "empty_split_sibling",
                                added_slot=added_shadow,
                                triangles=struct.unpack_from("<I", block.payload, 32)[0] // 3,
                                vertices=mesh_metadata(block, model.version)["vertex_count"]))
                        else:
                            new_meshes.append(encode_mesh(name, localp, localn, uv, tri,
                                material_map, donor_block, model.version,
                                specular=specular, flip_winding=flip_winding,
                                material_palette_offsets=material_palette_offsets))
                    target.meshes = new_meshes
        children = []
        for child in node.children:
            extras = visit(child, current_matrix, path, wheel)
            children.append(child)
            children.extend(extras)
        node.children = children
        return siblings

    visit(model.root, np.eye(4), "")
    if not found_body or found_wheels != set(wheel_names):
        raise FormatError(f"Donor body/wheel nodes missing: body={found_body}, wheels={sorted(found_wheels)}")
    model.root.set_bounds(np.array([all_positions.min(0), all_positions.max(0)]))
    result = model.write()
    # A lossless structural parse validates offsets and all mesh slots.
    if read_model(result).write() != result:
        raise AssertionError("CMP structural round-trip failed")
    report["bytes"] = len(result)
    report["source_triangles"] = int(len(body[3]) + sum(len(global_meshes[n][3]) for n in wheel_names.values()))
    report["triangles"] = sum(len(part[3]) for parts in geometry_parts.values() for part in parts)
    return result, report


def write_omb(materials: Iterable[dict]) -> bytes:
    """Write material records; texture names should be native +Cars\\...\\x.dds."""
    materials = list(materials)
    if len(materials) > MAX_MATERIAL_INDEX + 1:
        raise ValueError("At most 23 MM3 car shader material slots are available")
    out = bytearray(struct.pack("<BI", 0, len(materials)))
    for material in materials:
        diffuse = material.get("diffuse", material.get("color", (1, 1, 1, 1)))
        if max(diffuse) <= 1:
            diffuse = [round(float(x) * 255) for x in diffuse]
        red, green, blue, alpha = [min(255, max(0, int(x))) for x in diffuse]
        out += cstring(material["name"]) + cstring(material.get("texture") or "No")
        out += struct.pack("<5BI", int(material.get("unknown", 0xCC)), blue, green, red, alpha,
                           int(material.get("mode", 2 if material.get("texture") else 0)))
    return bytes(out)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="neutral JSON mesh package")
    parser.add_argument("template", type=Path, help="donor CMP")
    parser.add_argument("output", type=Path)
    parser.add_argument("--mapping", type=Path, help="JSON source material-id to native slot mapping")
    parser.add_argument("--y-offset", type=float, default=0)
    parser.add_argument("--scale", type=float, default=1)
    parser.add_argument("--body", nargs="+", default=["BODY_H"])
    parser.add_argument("--no-interior", action="store_true")
    winding = parser.add_mutually_exclusive_group()
    winding.add_argument("--flip-winding", action="store_true", default=True,
                         help="reverse neutral CCW faces to native MM3 winding (default)")
    winding.add_argument("--keep-winding", dest="flip_winding", action="store_false",
                         help="source faces already follow MM3's native convention")
    args = parser.parse_args()
    source = json.loads(args.source.read_text(encoding="utf-8"))
    material_map = {int(k): int(v) for k, v in json.loads(args.mapping.read_text()).items()} if args.mapping else {}
    transform = np.diag([args.scale, args.scale, args.scale, 1.0])
    transform[1, 3] = args.y_offset
    blob, report = convert_exterior(args.template.read_bytes(), source,
        body_names=args.body, material_map=material_map, source_transform=transform,
        keep_interior=not args.no_interior, flip_winding=args.flip_winding)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(blob)
    args.output.with_suffix(".report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
