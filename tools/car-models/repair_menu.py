"""Rebuild selection backgrounds and icons without rebuilding car models.

Uses each prepared package's source model and selected exterior meshes. All
packages are validated before writing; a new backup folder is required.
Reapply the packages with Car Workshop afterward, with the game closed.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import sys

import mm3_model
import mm3_textures
import preview

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/car-mods"))
from prepare import IDENTITIES, atomic_write, load_model_package, package_source


def prepare(package_dir: Path, preview_dir: Path, *, icons_only=False):
    # Validate links, owned destinations and every source before resolving the
    # package path or rendering. A repair must not follow a package junction.
    package_dir = package_dir.absolute()
    manifest = package_dir / "package.json"
    package = json.loads(manifest.read_text(encoding="utf-8"))
    identity = IDENTITIES.get(package.get("car_id"))
    if package.get("format") != "mm3-car-model-v1" or not identity or package.get("alias") != identity[0]:
        raise ValueError(f"Invalid prepared package identity: {package_dir}")
    alias = identity[0]
    load_model_package(dict(id=package["car_id"], alias=alias,
                            model_package=str(package_dir)), package_dir.parent)
    package_dir = package_dir.resolve()
    sources = [package_source(package_dir, item["source"]) for item in package["files"]]
    if len(set(sources)) != len(sources):
        raise ValueError("Menu repair requires separate source files for each package asset")
    report_path = package_dir / "conversion-report.json"
    report = json.loads(report_path.read_text(encoding="utf-8"))
    source_path = Path(report["source"])
    source = json.loads(source_path.read_text(encoding="utf-8"))
    # Early Panoz packages predate the explicit body_names report field.
    body_names = report.get("body_names", [mesh["name"] for mesh in source["meshes"]
        if re.fullmatch(r"BODY_H|BREAK\d+_H|FNDR[01]_H", mesh["name"])])
    names = set(body_names) | set(mm3_model.WHEEL_NODES.values())
    meshes = [mesh for mesh in source["meshes"] if mesh["name"] in names]
    if set(mesh["name"] for mesh in meshes) != names or "BODY_H" not in names:
        raise ValueError(f"Source is missing selected exterior meshes: {alias}")
    texture_images = preview.textures(source, source_path.parent)
    images = preview.render_menu_icons(meshes, texture_images, report["car"])
    if not icons_only:
        images["Background"] = preview.render_selection_background(meshes, texture_images, report["car"])
    preview_dir.mkdir(parents=True, exist_ok=True)
    updates = []
    for name, archive, target in (
        ("Menu", "Data_dvd.zip", f"Menu/Gfx/Vehicles/{alias}.cdds"),
        ("Small", "Data_dvd.zip", f"Menu/Gfx/Vehicles/{alias}_Small.cdds"),
        ("Background", "loose", f"Data/Shared/Cars/{alias}/bg.cdds"),
    ):
        if icons_only and name == "Background":
            continue
        png = preview_dir / (name + ".png")
        images[name].save(png)
        encoder = (mm3_textures.image_to_background_cdds if name == "Background"
                   else mm3_textures.image_to_menu_cdds)
        blob = encoder(png)
        matches = [item for item in package["files"]
                   if item["archive"] == archive and item["path"].casefold() == target.casefold()]
        if len(matches) > 1:
            raise ValueError(f"Duplicate menu asset: {target}")
        if matches:
            asset = package_source(package_dir, matches[0]["source"])
        else:
            relative = "assets/" + archive.replace(".zip", "") + "/" + target
            asset = package_dir / relative
            if asset.exists():
                raise ValueError(f"Unregistered asset already exists: {asset}")
            for parent in (asset.parent, *asset.parent.parents):
                if parent.exists():
                    info = parent.lstat()
                    if parent.is_symlink() or getattr(info, "st_file_attributes", 0) & 0x400:
                        raise ValueError("New menu asset cannot be reached through symbolic links or junctions")
                if parent == package_dir:
                    break
            package["files"].append(dict(archive=archive, path=target, source=relative))
        updates.append((asset, asset.read_bytes() if asset.exists() else None, blob))
    artwork = dict(report.get("menu_artwork", {}))
    artwork.update(source=str(source_path),
        meshes=sorted(names), background_size=[640, 480],
        menu_size=[128, 64], small_size=[64, 64],
        renderer="filtered source textures, depth-correct alpha, blue selection backdrop")
    artwork["icon_encoding"] = "lossless A8R8G8B8, straight alpha"
    report["menu_artwork"] = artwork
    for path, document in ((manifest, package), (report_path, report)):
        updates.append((path, path.read_bytes(), (json.dumps(document, indent=2) + "\n").encode("utf-8")))
    return updates


def install(prepared, backup: Path):
    if backup.exists():
        raise ValueError("Choose a new backup folder; existing backups are preserved")
    # Save all original package bytes before replacing the first asset.
    for folder, updates in prepared:
        created = []
        for path, before, _ in updates:
            relative = path.relative_to(folder.resolve())
            if before is None:
                created.append(relative.as_posix())
            else:
                original = backup / folder.name / relative
                original.parent.mkdir(parents=True, exist_ok=True)
                original.write_bytes(before)
        metadata = backup / folder.name / "created-files.json"
        metadata.parent.mkdir(parents=True, exist_ok=True)
        metadata.write_text(json.dumps(created, indent=2) + "\n", encoding="utf-8")
    changed = []
    try:
        for folder, updates in prepared:
            for path, before, after in updates:
                changed.append((path, before))
                atomic_write(path, after)
            icons = sum(path.suffix == ".cdds" for path, _, _ in updates)
            print(f"{folder.name}: {icons} menu assets rebuilt", flush=True)
    except (OSError, KeyboardInterrupt):
        for path, before in reversed(changed):
            if before is None:
                path.unlink(missing_ok=True)
            else:
                atomic_write(path, before)
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("packages", type=Path)
    parser.add_argument("--backup", type=Path, required=True)
    parser.add_argument("--preview-dir", type=Path, default=ROOT / "out/car-models/menu-repair")
    parser.add_argument("--icons-only", action="store_true", help="Regenerate only the two icons, preserving selection backgrounds")
    args = parser.parse_args()
    if args.backup.exists():
        parser.error("Choose a new backup folder; existing backups are preserved")
    folders = ([args.packages] if (args.packages / "package.json").is_file()
               else sorted(path.parent for path in args.packages.glob("*/package.json")))
    if not folders:
        parser.error("No prepared packages found")
    prepared = [(folder, prepare(folder, args.preview_dir / folder.name, icons_only=args.icons_only))
                for folder in folders]
    install(prepared, args.backup)


if __name__ == "__main__":
    main()
