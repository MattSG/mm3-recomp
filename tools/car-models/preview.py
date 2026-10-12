#!/usr/bin/env python3
"""Render neutral car geometry and PNG textures without the game or a GPU.

Requires the already installed NumPy and Pillow packages. Model coordinates
and UVs are taken literally, making these views useful for conversion checks.
"""

from __future__ import annotations

import argparse
from collections import Counter
import html
import json
from pathlib import Path
import re

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont


def world_mesh(mesh):
    matrix = np.asarray(mesh.get("transform", np.eye(4)), dtype=float)
    points = np.asarray(mesh["positions"], dtype=float)
    normals = np.asarray(mesh.get("normals", np.zeros_like(points)), dtype=float)
    return (points @ matrix[:3, :3].T + matrix[:3, 3],
            normals @ np.linalg.inv(matrix[:3, :3]),
            np.asarray(mesh.get("uvs", np.zeros((len(points), 2))), dtype=float),
            np.asarray(mesh["triangles"], dtype=int))


def inspect(meshes, materials):
    all_points, rows, material_counts = [], [], Counter()
    for mesh in meshes:
        p, n, uv, tris = world_mesh(mesh)
        all_points.append(p)
        faces = np.cross(p[tris[:, 1]] - p[tris[:, 0]], p[tris[:, 2]] - p[tris[:, 0]])
        area = np.linalg.norm(faces, axis=1)
        mean = n[tris[:, :3]].mean(axis=1)
        score = np.sum(faces * mean, axis=1)
        valid = (area > 1e-10) & (np.linalg.norm(mean, axis=1) > 1e-8)
        material_counts.update(int(value) for value in tris[:, 3])
        rows.append({"name": mesh["name"], "vertices": len(p), "triangles": len(tris),
                     "bounds_min": p.min(axis=0).tolist(), "bounds_max": p.max(axis=0).tolist(),
                     "transform_origin": np.asarray(mesh.get("transform", np.eye(4)))[:3, 3].tolist(),
                     "degenerate_triangles": int(np.sum(area <= 1e-10)),
                     "winding_with_normals": int(np.sum(valid & (score > 0))),
                     "winding_against_normals": int(np.sum(valid & (score < 0))),
                     "uv_min": uv.min(axis=0).tolist(), "uv_max": uv.max(axis=0).tolist()})
    points = np.concatenate(all_points)
    return {"bounds_min": points.min(axis=0).tolist(), "bounds_max": points.max(axis=0).tolist(),
            "dimensions_xyz": np.ptp(points, axis=0).tolist(), "meshes": rows,
            "materials": [{"index": index, "name": materials[index].get("name", str(index)),
                           "texture": materials[index].get("texture"), "triangles": count}
                          for index, count in sorted(material_counts.items())]}


def textures(model, folder):
    result = []
    for material in model["materials"]:
        texture = material.get("texture")
        if texture:
            image = np.asarray(Image.open(folder / texture).convert("RGBA"), dtype=float) / 255
        else:
            image = np.ones((1, 1, 4))
        diffuse = np.asarray(material.get("diffuse", [1, 1, 1, 1]), dtype=float)
        result.append(image * diffuse)
    return result


def texture_mipmaps(texture):
    """Filter associated RGB so transparent texels cannot stain icon edges."""
    current = np.asarray(texture, dtype=float).copy()
    current[..., :3] *= current[..., 3, None]
    levels = [current]
    while current.shape[0] > 1 or current.shape[1] > 1:
        height, width = current.shape[:2]
        padded = np.pad(current, ((0, height % 2), (0, width % 2), (0, 0)), mode="edge")
        current = padded.reshape((height + 1) // 2, 2, (width + 1) // 2, 2, 4).mean(axis=(1, 3))
        levels.append(current)
    return levels


def sample_texture(mipmaps, coords, *, repeat=False, level=0):
    """Bilinear/trilinear sampling with the source's top-left UV origin."""
    level = float(np.clip(level, 0, len(mipmaps) - 1))
    lower = int(np.floor(level))

    def bilinear(texture):
        height, width = texture.shape[:2]
        xy = coords * np.asarray([width, height]) - 0.5
        base = np.floor(xy).astype(int)
        fraction = xy - base
        x0, y0 = base[:, 0], base[:, 1]
        x1, y1 = x0 + 1, y0 + 1
        if repeat:
            x0, x1, y0, y1 = x0 % width, x1 % width, y0 % height, y1 % height
        else:
            x0, x1 = np.clip(x0, 0, width - 1), np.clip(x1, 0, width - 1)
            y0, y1 = np.clip(y0, 0, height - 1), np.clip(y1, 0, height - 1)
        fx, fy = fraction[:, 0, None], fraction[:, 1, None]
        top = texture[y0, x0] * (1 - fx) + texture[y0, x1] * fx
        bottom = texture[y1, x0] * (1 - fx) + texture[y1, x1] * fx
        return top * (1 - fy) + bottom * fy

    rgba = bilinear(mipmaps[lower])
    if level > lower:
        rgba = rgba * (1 - level + lower) + bilinear(mipmaps[lower + 1]) * (level - lower)
    rgba[:, :3] /= np.maximum(rgba[:, 3, None], 1e-12)
    return rgba


def texture_level(screen, uv, texture):
    """Estimate the texture footprint of one output pixel on this triangle."""
    edges = screen[1:] - screen[0]
    gradient = np.linalg.solve(edges, uv[1:] - uv[0])
    footprint = np.linalg.norm(gradient * np.asarray([texture.shape[1], texture.shape[0]]), axis=1).max()
    return max(0.0, float(np.log2(max(footprint, 1e-12))))


def edge_coverage(weights, start, end, denominator):
    # One triangle owns a shared edge. Including it in both leaves darker
    # diagonal seams when glass or a filtered cutout is blended twice.
    delta = (end - start) * (1 if denominator > 0 else -1)
    top_left = delta[1] < 0 or (abs(delta[1]) < 1e-12 and delta[0] > 0)
    return (weights > 1e-9) | ((np.abs(weights) <= 1e-9) & top_left)


def render(meshes, texture_images, direction, size, label, name, *, transparent_background=False):
    width, height = size
    world = [world_mesh(mesh) for mesh in meshes]
    points = np.concatenate([mesh[0] for mesh in world])
    center = (points.min(axis=0) + points.max(axis=0)) / 2
    eye = np.asarray(direction, dtype=float)
    forward = -eye / np.linalg.norm(eye)
    right = np.cross(forward, [0, 1, 0])
    right /= np.linalg.norm(right)
    up = np.cross(right, forward)
    projected = np.stack(((points - center) @ right, (points - center) @ up), axis=-1)
    span = np.ptp(projected, axis=0)
    margin_x, margin_y = ((width * 0.05, height * 0.06) if transparent_background else (65, 90))
    scale = min((width - margin_x * 2) / span[0], (height - margin_y * 2) / span[1])
    canvas = np.empty((height, width, 3), dtype=float)
    # A neutral background makes missing surfaces and transparent glass visible.
    grad = np.linspace(0.94, 0.86, height)[:, None, None]
    canvas[:] = 0 if transparent_background else grad * np.asarray([0.97, 0.99, 1.0])
    coverage = np.zeros((height, width))
    depth_buffer = np.full((height, width), np.inf)
    light = np.asarray([-0.4, 0.8, -0.35], dtype=float)
    light /= np.linalg.norm(light)
    triangles = []
    mipmaps = [texture_mipmaps(texture) for texture in texture_images]
    opaque_materials = [bool(np.any(texture[..., 3] >= 0.999)) for texture in texture_images]
    # Bilinear filtering also creates blended pixels on binary cutout edges.
    transparent_materials = [bool(np.any(texture[..., 3] < 0.999) and np.any(texture[..., 3] > 0.03))
                             for texture in texture_images]
    for p, normals, uv, tris in world:
        local = p - center
        screen = np.stack((local @ right * scale + width / 2,
                           -local @ up * scale + height / 2 + (0 if transparent_background else 15)), axis=-1)
        depths = local @ forward
        for tri in tris:
            indices, material = tri[:3], int(tri[3])
            triangle = (float(depths[indices].mean()), screen[indices], depths[indices],
                        normals[indices], uv[indices], material)
            if opaque_materials[material]:
                triangles.append((False, *triangle))
            if transparent_materials[material]:
                triangles.append((True, *triangle))
    # Depth-test opaque pixels even when their atlas also contains glass.
    # Blend only partial-alpha pixels from back to front without depth writes.
    triangles.sort(key=lambda item: (item[0], -item[1] if item[0] else item[1]))
    for transparent, _, screen, depths, normals, uv, material in triangles:
        x0, y0 = np.floor(screen.min(axis=0)).astype(int)
        x1, y1 = np.ceil(screen.max(axis=0)).astype(int)
        x0, y0, x1, y1 = max(0, x0), max(0, y0), min(width - 1, x1), min(height - 1, y1)
        if x0 > x1 or y0 > y1:
            continue
        a, b, c = screen
        denominator = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1])
        if abs(denominator) < 1e-9:
            continue
        xs, ys = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
        wa = ((b[1] - c[1]) * (xs - c[0]) + (c[0] - b[0]) * (ys - c[1])) / denominator
        wb = ((c[1] - a[1]) * (xs - c[0]) + (a[0] - c[0]) * (ys - c[1])) / denominator
        wc = 1 - wa - wb
        local_depth = wa * depths[0] + wb * depths[1] + wc * depths[2]
        view_depth = depth_buffer[y0:y1 + 1, x0:x1 + 1]
        mask = (edge_coverage(wa, b, c, denominator) & edge_coverage(wb, c, a, denominator)
                & edge_coverage(wc, a, b, denominator) & (local_depth < view_depth + 1e-6))
        if not np.any(mask):
            continue
        weights = np.stack((wa[mask], wb[mask], wc[mask]), axis=-1)
        coords = weights @ uv
        # The neutral format explicitly declares a top-left UV origin. Never
        # flip V here: doing so would disguise a conversion orientation bug.
        texture = texture_images[material]
        repeat = bool(np.any(uv < -1e-6) or np.any(uv > 1 + 1e-6))
        rgba = sample_texture(mipmaps[material], coords, repeat=repeat,
                              level=texture_level(screen, uv, texture))
        valid = ((rgba[:, 3] > 0.03) & (rgba[:, 3] < 0.999) if transparent
                 else rgba[:, 3] >= 0.999)
        if not np.any(valid):
            continue
        ni = weights @ normals
        ni /= np.maximum(np.linalg.norm(ni, axis=1)[:, None], 1e-12)
        shading = 0.65 + 0.35 * np.maximum(ni @ light, 0)
        shaded = np.clip(rgba[:, :3] * shading[:, None], 0, 1)
        iy, ix = np.nonzero(mask)
        iy, ix = iy[valid] + y0, ix[valid] + x0
        alpha = rgba[valid, 3, None]
        canvas[iy, ix] = shaded[valid] * alpha + canvas[iy, ix] * (1 - alpha)
        coverage[iy, ix] = rgba[valid, 3] + coverage[iy, ix] * (1 - rgba[valid, 3])
        if not transparent:
            depth_buffer[iy, ix] = local_depth[mask][valid]
    if transparent_background:
        straight = canvas / np.maximum(coverage[..., None], 1e-12)
        rgba = np.concatenate((np.clip(straight, 0, 1), coverage[..., None]), axis=-1)
        return Image.fromarray(np.rint(rgba * 255).astype(np.uint8))
    image = Image.fromarray(np.rint(np.clip(canvas, 0, 1) * 255).astype(np.uint8))
    draw = ImageDraw.Draw(image)
    try:
        font = ImageFont.truetype("C:/Windows/Fonts/segoeui.ttf", 22)
        small = ImageFont.truetype("C:/Windows/Fonts/segoeui.ttf", 16)
    except OSError:
        font = small = ImageFont.load_default()
    draw.text((24, 20), f"{name} — {label}", font=font, fill="#253044")
    draw.text((24, height - 34), "Source geometry • top-left UVs • +Y up • front −Z • orthographic reference", font=small, fill="#526174")
    return image


def render_menu_icons(meshes, texture_images, name, *, supersample=4):
    """Return the two transparent menu thumbnails at native dimensions."""
    images = {}
    for suffix, dimensions in (("Menu", (128, 64)), ("Small", (64, 64))):
        image = render(meshes, texture_images, (4, 2, -6),
                       tuple(value * supersample for value in dimensions), "", name,
                       transparent_background=True)
        images[suffix] = image.resize(dimensions, Image.Resampling.LANCZOS)
    return images


def render_selection_background(meshes, texture_images, name, *, size=(640, 480), supersample=2):
    """Compose source front/rear portraits in the menu's dark blue palette."""
    width, height = size
    xs, ys = np.meshgrid(np.linspace(0, 1, width), np.linspace(0, 1, height))
    glow = np.exp(-((xs - .65) / .8) ** 2 - ((ys - .43) / .25) ** 2)
    background = np.asarray([.017, .032, .083]) + glow[..., None] * np.asarray([.025, .071, .155])
    image = Image.fromarray(np.rint(background * 255).astype(np.uint8)).convert("RGBA")
    bands = Image.new("RGBA", size)
    draw = ImageDraw.Draw(bands)
    draw.polygon([(0, height * .48), (width, height * .24), (width, height * .27),
                  (0, height * .54)], fill=(34, 92, 181, 22))
    draw.polygon([(0, height * .69), (width, height * .59), (width, height * .62),
                  (0, height * .72)], fill=(42, 105, 205, 28))
    image = Image.alpha_composite(image, bands)
    # Match the native layout: a complete rear view on the right and a
    # quieter, cropped front portrait on the left. No donor artwork is used.
    portraits = (((-4, 2, 6), (.76, .74), (.20, .13), 1.0),
                 ((4, 2, -6), (.76, .71), (-.30, .20), .74))
    for direction, fraction, position, opacity in portraits:
        dimensions = (round(width * fraction[0]), round(height * fraction[1]))
        portrait = render(meshes, texture_images, direction,
                          tuple(value * supersample for value in dimensions), "", name,
                          transparent_background=True).resize(dimensions, Image.Resampling.LANCZOS)
        rgba = np.asarray(portrait, dtype=float) / 255
        luminance = rgba[..., :3] @ np.asarray([.2126, .7152, .0722])
        tone = np.power(luminance, .8)
        rgba[..., :3] = np.asarray([.02, .06, .14]) + tone[..., None] * np.asarray([.11, .31, .65])
        rgba[..., 3] *= opacity
        blue = Image.fromarray(np.rint(np.clip(rgba, 0, 1) * 255).astype(np.uint8))
        x, y = round(width * position[0]), round(height * position[1])
        shadow = Image.new("RGBA", size)
        ImageDraw.Draw(shadow).ellipse((x + dimensions[0] * .08, y + dimensions[1] * .72,
                                       x + dimensions[0] * .95, y + dimensions[1] * .91),
                                      fill=(0, 3, 12, 110))
        image = Image.alpha_composite(image, shadow.filter(ImageFilter.GaussianBlur(width * .015)))
        image.alpha_composite(blue, (x, y))
    return image.convert("RGB")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("model", type=Path)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--name", default="Panoz GTR-1")
    parser.add_argument("--include", default=r"(?:BODY|BREAK\d+|FNDR[01]|WHL[0-3])_H")
    parser.add_argument("--width", type=int, default=1200)
    parser.add_argument("--height", type=int, default=800)
    parser.add_argument("--icons", action="store_true", help="Also render native menu icons and a 640x480 blue selection background")
    parser.add_argument("--icon-prefix", default="Car")
    args = parser.parse_args()
    model = json.loads(args.model.read_text(encoding="utf-8"))
    meshes = [mesh for mesh in model["meshes"] if re.fullmatch(args.include, mesh["name"])]
    if not meshes:
        parser.error("No meshes match --include")
    args.out.mkdir(parents=True, exist_ok=True)
    report = inspect(meshes, model["materials"])
    (args.out / "source-check.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    texture_images = textures(model, args.model.parent)
    views = (("front-three-quarter", (4, 2, -6), "Front three-quarter"),
             ("rear-three-quarter", (-4, 2, 6), "Rear three-quarter"),
             ("side", (-8, 1.0, 0), "Side"))
    for filename, direction, label in views:
        render(meshes, texture_images, direction, (args.width, args.height), label, args.name).save(args.out / (filename + ".png"))
        print(f"Rendered {filename}")
    if args.icons:
        for suffix, image in render_menu_icons(meshes, texture_images, args.name).items():
            image.save(args.out / f"{args.icon_prefix}_{suffix}.png")
            print(f"Rendered transparent {args.icon_prefix}_{suffix}.png {image.size}")
        render_selection_background(meshes, texture_images, args.name).save(args.out / f"{args.icon_prefix}_Background.png")
        print(f"Rendered {args.icon_prefix}_Background.png (640, 480)")
    images = "".join(f'<figure><img src="{filename}.png"><figcaption>{label}</figcaption></figure>'
                     for filename, _, label in views)
    page = ('<!doctype html><meta charset="utf-8"><title>' + html.escape(args.name) +
            ' source reference</title><style>body{font:16px system-ui;background:#eef1f5;color:#253044;margin:24px}'
            'main{max-width:1200px;margin:auto}img{width:100%;height:auto}figure{margin:24px 0}figcaption{padding:8px}</style>'
            '<main><h1>' + html.escape(args.name) + ' source reference</h1>'
            '<p>Literal source geometry and textures, rendered without the game. Both sides of triangles are drawn. '
            'Mesh transforms and top-left UVs are preserved. Damage, glows and shadow helpers are omitted.</p>' + images + '</main>')
    (args.out / "index.html").write_text(page, encoding="utf-8")
    print(json.dumps({"dimensions_xyz": report["dimensions_xyz"], "meshes": len(meshes), "materials": len(report["materials"])}))


if __name__ == "__main__":
    main()
