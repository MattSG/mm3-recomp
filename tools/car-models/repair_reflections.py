"""Restore material reflections in prepared imported-car packages.

Only the visible CMP vertex shading word and conversion report are updated.
Positions, normals, UVs, indices, shadows, effects and textures are preserved.
Prepare packages here, then install them through the ordinary Car Workshop
prepare.py command. Original files are saved to a new, required backup folder.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import struct

import mm3_model


def material_shading(source: dict, report: dict) -> tuple[dict, list]:
    materials = list(source["materials"])
    # Reproduce the builder's extra solid materials for averaged repeated UVs.
    averaged = set()
    for face in report.get("averaged_minified_faces", []):
        original = int(face["source_material"])
        if original not in averaged:
            materials.append(materials[original])
            averaged.add(original)
    shading, merged = {}, {}
    for original, slot in report["material_map"].items():
        original, slot = int(original), int(slot)
        channels = mm3_model.source_material_shading(materials[original])
        merged.setdefault(slot, set()).add(channels)
        # Older packages merged identical textures despite different gloss.
        # Preserve their existing packing and choose the reflective variant.
        shading[slot] = max(shading.get(slot, channels), channels)
    return shading, [slot for slot, variants in merged.items() if len(variants) > 1]


def apply_shading(blob: bytes, shading: dict) -> tuple[bytes, dict]:
    model = mm3_model.read_model(blob)
    packed = {int(slot): mm3_model.packed_vertex_shading(channels) for slot, channels in shading.items()}
    meshes, changed, reflective = 0, 0, 0
    for path, node in model.root.walk():
        updated = []
        for block in node.meshes:
            if (not block.payload or mm3_model.is_shadow_mesh(block) or
                    not block.name.rsplit("\\", 1)[-1].startswith("mm3import_")):
                updated.append(block)
                continue
            meta = mm3_model.mesh_metadata(block, model.version)
            payload = bytearray(block.payload)
            for vertex in range(meta["vertex_count"]):
                offset = meta["vertex_offset"] + vertex * 24
                slot = payload[offset + 12] // 11
                if slot not in packed:
                    raise ValueError(f"Imported vertex has no source material mapping: {path}, slot {slot}")
                value = packed[slot]
                previous = struct.unpack_from("<I", payload, offset + 20)[0]
                changed += previous != value
                reflective += bool(value & 0xFFFFFF00)
                struct.pack_into("<I", payload, offset + 20, value)
            updated.append(mm3_model.MeshBlock(block.name, bytes(payload)))
            meshes += 1
        node.meshes = updated
    if not meshes or not reflective:
        raise ValueError("No reflective imported surfaces were found")
    after = model.write()
    if len(after) != len(blob) or mm3_model.read_model(after).write() != after:
        raise ValueError("Reflection repair changed the native model structure")
    return after, dict(meshes=meshes, changed_vertices=changed, reflective_vertices=reflective)


def prepare(package_dir: Path):
    package_dir = package_dir.resolve()
    package = json.loads((package_dir / "package.json").read_text(encoding="utf-8"))
    report_path = package_dir / "conversion-report.json"
    report = json.loads(report_path.read_text(encoding="utf-8"))
    source = json.loads(Path(report["source"]).read_text(encoding="utf-8"))
    shading, merged = material_shading(source, report)
    updates = []
    for item in package["files"]:
        if item["archive"] != "loose" or not item["path"].lower().endswith("/body.cmp"):
            continue
        path = (package_dir / item["source"]).resolve()
        if not path.is_relative_to(package_dir):
            raise ValueError("CMP asset path leaves its package folder")
        before = path.read_bytes()
        after, reflection_report = apply_shading(before, shading)
        suffix = "_lod1" if "_lod1/" in item["path"] else "near"
        report["models"][suffix]["reflections"] = reflection_report
        updates.append((path, before, after))
    if len(updates) != 2:
        raise ValueError(f"Expected exactly near/far CMPs: {package_dir.name}")
    report["material_shading"] = shading
    report["reflection_merged_materials"] = merged
    updates.append((report_path, report_path.read_bytes(),
                    (json.dumps(report, indent=2) + "\n").encode("utf-8")))
    return updates, report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("packages", type=Path, help="One package or the prepared package parent folder")
    parser.add_argument("--backup", type=Path, required=True, help="New backup folder for original package files")
    args = parser.parse_args()
    folders = ([args.packages] if (args.packages / "package.json").is_file()
               else sorted(path.parent for path in args.packages.glob("*/package.json")))
    if not folders:
        parser.error("No prepared packages found")
    if args.backup.exists():
        parser.error("Choose a new backup folder; existing backups are preserved")
    prepared = [(folder.resolve(), *prepare(folder)) for folder in folders]
    for folder, updates, report in prepared:
        for path, before, after in updates:
            backup = args.backup / folder.name / path.relative_to(folder)
            backup.parent.mkdir(parents=True, exist_ok=True)
            backup.write_bytes(before)
    for folder, updates, report in prepared:
        for path, before, after in updates:
            temp = path.with_suffix(path.suffix + ".reflection-repair.tmp")
            temp.write_bytes(after)
            temp.replace(path)
        near = report["models"]["near"]["reflections"]
        print(f"{folder.name}: {near['reflective_vertices']:,} reflective vertices; "
              "near/far geometry, shadows and lights preserved")


if __name__ == "__main__":
    main()
