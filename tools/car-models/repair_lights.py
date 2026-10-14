"""Repair lamp and road-light effects in prepared packages.

Only CMP light transforms/settings and conversion reports are changed; visible
geometry, shadows, materials and textures remain byte-identical. A backup folder
is required, and existing backups are never overwritten.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np

import mm3_lights
import mm3_model


def prepare(package_dir: Path):
    package = json.loads((package_dir / "package.json").read_text(encoding="utf-8"))
    report_path = package_dir / "conversion-report.json"
    report = json.loads(report_path.read_text(encoding="utf-8"))
    source_path = Path(report["source"])
    source = json.loads(source_path.read_text(encoding="utf-8"))
    anchors, evidence = mm3_lights.source_anchors(source)
    if not anchors:
        raise ValueError(f"No authored source lamps for {package_dir.name}")
    transform = np.eye(4); transform[1, 3] = report["y_offset"]
    updates = []
    for item in package["files"]:
        if item["archive"] != "loose" or not item["path"].lower().endswith("/body.cmp"):
            continue
        path = (package_dir / item["source"]).resolve()
        if not path.is_relative_to(package_dir.resolve()):
            raise ValueError("CMP asset path leaves its package folder")
        before = path.read_bytes()
        after, light_report = mm3_lights.align_lights(before, anchors, source_transform=transform)
        old_nodes = {path: node for path, node in mm3_model.read_model(before).root.walk()}
        new_nodes = {path: node for path, node in mm3_model.read_model(after).root.walk()}
        if len(before) != len(after) or old_nodes.keys() != new_nodes.keys():
            raise ValueError("Light repair changed native node structure")
        for name, old in old_nodes.items():
            if old.meshes != new_nodes[name].meshes:
                raise ValueError(f"Light repair changed mesh geometry: {name}")
        suffix = "_lod1" if "_lod1/" in item["path"] else "near"
        report["models"][suffix]["lights"] = light_report
        updates.append((path, before, after))
    if len(updates) != 2:
        raise ValueError(f"Expected exactly near/far CMPs: {package_dir.name}")
    report["light_anchors"], report["light_alignment"] = anchors, evidence
    updates.append((report_path.resolve(), report_path.read_bytes(),
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
    prepared = [(folder, *prepare(folder)) for folder in folders]
    # Validate every package before saving backups or modifying outputs.
    for folder, updates, report in prepared:
        for path, before, after in updates:
            backup = args.backup / folder.name / path.relative_to(folder.resolve())
            backup.parent.mkdir(parents=True, exist_ok=True)
            backup.write_bytes(before)
    for folder, updates, report in prepared:
        for path, before, after in updates:
            temp = path.with_suffix(path.suffix + ".light-repair.tmp")
            temp.write_bytes(after)
            temp.replace(path)
        near = report["models"]["near"]["lights"]
        print(f"{folder.name}: {len(near['moved'])} lamp/road effects aligned, "
              f"{len(near['disabled'])} unused effects disabled; near/far geometry preserved")


if __name__ == "__main__":
    main()
