"""Build a prepared MM3 exterior package from an extracted neutral source car.

This creates files for Car Workshop's Model tab; it does not install anything.
Run with the system Python that has NumPy, SciPy and Pillow installed.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import sys

import numpy as np
from PIL import Image

import mm3_model
import mm3_textures

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/mm3view'))
sys.path.insert(0, str(ROOT / 'tools/car-mods'))
from mm3view import Data
from prepare import IDENTITIES, alias_paths


def alignment_offset(template: bytes, source: dict) -> float:
    """Match the average authored wheel height to the native car pose."""
    native = []
    def visit(node, parent):
        matrix = parent @ node.relative_matrix()
        if node.name in mm3_model.WHEEL_NODES:
            native.append(float(matrix[1, 3]))
        for child in node.children:
            visit(child, matrix)
    visit(mm3_model.read_model(template).root, np.eye(4))
    wheels = [mesh for mesh in source['meshes']
              if mesh['name'] in mm3_model.WHEEL_NODES.values()]
    if len(native) != 4 or len(wheels) != 4:
        raise ValueError('Alignment requires four native and four source wheel pivots')
    source_y = [float(np.asarray(mesh['transform'])[1, 3]) for mesh in wheels]
    return float(np.mean(native) - np.mean(source_y))


def collapse_constant_uvs(meshes: list[dict], materials: set[int]) -> int:
    """Solid surfaces have no UV detail; avoid subdividing their repeated UVs.

    Clone affected vertices so a shared vertex used by a detailed material
    keeps its original coordinates. Source files are never changed.
    """
    faces = 0
    for mesh in meshes:
        copies = {}
        for triangle in mesh['triangles']:
            material = int(triangle[3])
            if material not in materials:
                continue
            faces += 1
            for corner in range(3):
                original = int(triangle[corner])
                key = (original, material)
                if key not in copies:
                    copies[key] = len(mesh['positions'])
                    mesh['positions'].append(mesh['positions'][original][:])
                    mesh['normals'].append(mesh['normals'][original][:])
                    mesh['uvs'].append([0.5, 0.5])
                triangle[corner] = copies[key]
    return faces


def average_minified_repeats(source: dict, meshes: list[dict], source_dir: Path) -> list[dict]:
    """Opt-in approximation for exceptionally repeated, heavily minified faces.

    Use the texture's average color, equivalent to its smallest mip level.
    The original texture and UVs remain untouched for every other face.
    """
    replacements, report = {}, []
    for mesh in meshes:
        for face_index, triangle in enumerate(mesh['triangles']):
            uv = np.asarray([mesh['uvs'][i] for i in triangle[:3]], dtype=float)
            span = np.ptp(uv, axis=0)
            if float(np.prod(np.ceil(span) + 1)) <= 1024:
                continue
            original = int(triangle[3])
            material = source['materials'][original]
            texture = material.get('texture')
            if not texture:
                continue
            path = (source_dir / texture).resolve()
            if not path.is_relative_to(source_dir) or not path.is_file():
                raise ValueError(f'Missing or unsafe source texture: {texture}')
            with Image.open(path) as image:
                pixels = np.asarray(image.convert('RGBA'), dtype=float)
                if np.all(np.ptp(pixels.reshape(-1, 4), axis=0) == 0):
                    continue  # Exact constant handling below needs no approximation.
                color = pixels.mean(axis=(0, 1)) / 255
            if original not in replacements:
                flat = dict(material)
                flat['texture'] = None
                flat['diffuse'] = (color * np.asarray(material.get('diffuse', (1, 1, 1, 1)))).tolist()
                replacements[original] = len(source['materials'])
                source['materials'].append(flat)
            triangle[3] = replacements[original]
            report.append(dict(mesh=mesh['name'], triangle=face_index,
                               source_material=original, uv_span=span.tolist(),
                               average_rgba=color.tolist()))
    return report


def build(source_path: Path, output: Path, car_id: int, *, y_offset=None,
          max_texture_size=1024, icons: Path | None = None, body_names=None,
          average_minified=False):
    if car_id not in IDENTITIES:
        raise ValueError('Choose an existing Car Workshop car ID')
    alias, donor, name = IDENTITIES[car_id]
    if donor.lower() not in ('viper', 'koenigsegg', 'lotus'):
        raise ValueError('Validated exterior donors are Viper, Koenigsegg and Lotus')
    source = json.loads(source_path.read_text(encoding='utf-8'))
    source_dir = source_path.resolve().parent
    if body_names is None:
        body_names = [mesh['name'] for mesh in source['meshes']
                      if re.fullmatch(r'BODY_H|BREAK\d+_H|FNDR[01]_H', mesh['name'])]
    if 'BODY_H' not in body_names:
        raise ValueError('The complete exterior must include BODY_H')
    meshes = [mesh for mesh in source['meshes']
              if mesh['name'] in body_names or mesh['name'] in mm3_model.WHEEL_NODES.values()]
    averaged_faces = average_minified_repeats(source, meshes, source_dir) if average_minified else []
    used = sorted({int(tri[3]) for mesh in meshes for tri in mesh['triangles']})
    native_materials, mapping, textures, unique = [], {}, [], {}
    constant_materials = set()
    for source_id in used:
        material = source['materials'][source_id]
        texture = material.get('texture')
        if texture:
            texture_path = (source_dir / texture).resolve()
            if not texture_path.is_relative_to(source_dir) or not texture_path.is_file():
                raise ValueError(f'Missing or unsafe source texture: {texture}')
            with Image.open(texture_path) as image:
                if all(low == high for low, high in image.convert('RGBA').getextrema()):
                    constant_materials.add(source_id)
        else:
            constant_materials.add(source_id)
        signature = (texture, tuple(material.get('diffuse', (1, 1, 1, 1))))
        if signature in unique:
            mapping[source_id] = unique[signature]
            continue
        slot = len(native_materials)
        if slot >= 23:
            raise ValueError('Exterior needs more than 23 materials; merge materials before building')
        unique[signature] = mapping[source_id] = slot
        record = dict(material)
        if texture:
            texture_path = (source_dir / texture).resolve()
            if not texture_path.is_relative_to(source_dir) or not texture_path.is_file():
                raise ValueError(f'Missing or unsafe source texture: {texture}')
            texture_name = f'imported_{slot:02d}'
            record['texture'] = f'+Cars\\{alias}\\{texture_name}.dds'
            # MM3's transparent car pass shades glass separately and does not
            # retain the imported surface RGB. Bake source alpha over a dark
            # base for this first visual port, preserving banners and details.
            record['mode'] = 2
            textures.append((texture_path, texture_name))
        else:
            record['texture'], record['mode'] = 'No', 0
        native_materials.append(record)
    data = Data(ROOT / 'game_files/Data')
    collapsed_faces = collapse_constant_uvs(meshes, constant_materials)
    near_template = data.read(f'cars/{donor}/body.cmp')
    alignment = 'explicit' if y_offset is not None else 'average source/native wheel pivots'
    if y_offset is None:
        y_offset = alignment_offset(near_template, source)
    transform = np.eye(4)
    transform[1, 3] = y_offset
    output.mkdir(parents=True, exist_ok=True)
    files, reports = [], {}

    def asset(archive, path, blob):
        source_name = 'assets/' + archive.replace('.zip', '') + '/' + path
        target = output / source_name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(blob)
        files.append(dict(archive=archive, path=path, source=source_name))

    for suffix in ('', '_lod1'):
        template = alias_paths(data.read(f'cars/{donor}{suffix}/body.cmp'), donor, alias)
        blob, report = mm3_model.convert_exterior(template, source,
            material_map=mapping, body_names=body_names,
            source_transform=transform, flip_winding=True, keep_interior=False,
            specular=(255, 0, 0, 0))
        asset('loose', f'Data/Shared/Cars/{alias}{suffix}/body.cmp', blob)
        reports[suffix or 'near'] = report
        material_blob = mm3_model.write_omb(native_materials)
        for paint in range(3):
            asset('Data_hd.zip', f'Cars/{alias}{suffix}/materialSet{paint:02d}.omb', material_blob)
    for texture_path, texture_name in textures:
        with Image.open(texture_path) as image:
            rgba = image.convert('RGBA')
            opaque = Image.new('RGBA', rgba.size, (8, 10, 12, 255))
            opaque.alpha_composite(rgba)
            baked_path = output / 'baked-textures' / (texture_name + '.png')
            baked_path.parent.mkdir(parents=True, exist_ok=True)
            opaque.convert('RGB').save(baked_path)
        blob = mm3_textures.image_to_cdds(baked_path, max_size=max_texture_size)
        asset('Data_dvd.zip', f'Cars/{alias}/{texture_name}.cdds', blob)
        print(f'Texture {texture_name}: {len(blob):,} bytes', flush=True)
    if icons:
        for source_name, suffix in [('Menu.png', ''), ('Small.png', '_Small')]:
            icon = icons / source_name
            if not icon.is_file():
                icon = next((icons / (prefix + '_' + source_name)
                             for prefix in ('Car', 'Panoz')
                             if (icons / (prefix + '_' + source_name)).is_file()), icon)
            blob = mm3_textures.image_to_cdds(icon, max_size=128)
            asset('Data_dvd.zip', f'Menu/Gfx/Vehicles/{alias}{suffix}.cdds', blob)
    package = dict(format='mm3-car-model-v1', alias=alias, car_id=car_id, files=files)
    (output / 'package.json').write_text(json.dumps(package, indent=2) + '\n', encoding='utf-8')
    report = dict(car=name, source=str(source_path.resolve()), material_map=mapping,
                  native_materials=len(native_materials), textures=len(textures),
                  y_offset=y_offset, alignment=alignment, body_names=body_names,
                  constant_uv_materials=sorted(constant_materials), constant_uv_faces=collapsed_faces,
                  averaged_minified_faces=averaged_faces,
                  opaque_glass=True, donor_exterior_cockpit=False, models=reports)
    (output / 'conversion-report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    for credit in source_dir.glob('*[Rr]eadme*.txt'):
        (output / credit.name).write_bytes(credit.read_bytes())
    print(f'{name}: {len(files)} prepared files -> {output / "package.json"}', flush=True)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path, help='mm2_source.py output model.json')
    parser.add_argument('output', type=Path, help='prepared package folder')
    parser.add_argument('--car-id', type=int, default=9004)
    parser.add_argument('--y-offset', type=float, help='Override automatic wheel-height alignment')
    parser.add_argument('--body', action='append', help='Source body mesh name (repeatable)')
    parser.add_argument('--average-minified-repeats', action='store_true',
                        help='Approximate faces spanning over 1024 UV cells using the texture average')
    parser.add_argument('--max-texture-size', type=int, default=1024)
    parser.add_argument('--icons', type=Path, help='preview.py icon output directory')
    args = parser.parse_args()
    build(args.source, args.output, args.car_id, y_offset=args.y_offset,
          max_texture_size=args.max_texture_size, icons=args.icons, body_names=args.body,
          average_minified=args.average_minified_repeats)


if __name__ == '__main__':
    main()
