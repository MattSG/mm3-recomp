"""Align native lamp and road-light effects to imported source lamps.

MM3 light nodes contain effect settings, while their parent transform supplies
the position. Replacing only exterior meshes leaves those effects on the donor
body. Positions here use the neutral source convention: metres, +Y up, -Z front.
"""
from __future__ import annotations

import json
from itertools import permutations
from pathlib import Path
import struct

import numpy as np

import mm3_model


REAR_LIGHT_TYPES = {"backlight": 1, "brakelight": 2, "reverselight": 3}
LIGHT_TYPES = {"headlight": 0, **REAR_LIGHT_TYPES, "headlightenv": 8}


def _helper_anchors(source: dict, names: tuple[str, ...]) -> list[list[float]]:
    """Find lamp centers from the author's dedicated light surface meshes."""
    meshes = {mesh["name"]: mesh for mesh in source["meshes"]}
    mesh = next((meshes[name] for name in names
                 if name in meshes and meshes[name]["positions"]), None)
    if mesh is None:
        return []
    positions = mm3_model._neutral_world(mesh, np.eye(4))[0]
    # Some racing cars have one central lamp, while road cars have two sides.
    # Deduplicate authored vertices so UV seams do not bias the lamp center.
    points = np.unique(positions, axis=0)
    if np.max(np.abs(points[:, 0])) < .1:
        return [points.mean(axis=0).tolist()]
    triangles = np.asarray(mesh.get("triangles", []), dtype=int)
    if triangles.size:
        # A helper can also contain an auxiliary center brake bar. Exclude
        # center-crossing surface components from the main side-lamp centers
        # (the Ford GT combines both rings and its high brake bar in TLIGHT).
        points, inverse = np.unique(positions, axis=0, return_inverse=True)
        adjacent = {index: {index} for index in range(len(points))}
        for triangle in inverse[triangles[:, :3]]:
            for index in triangle:
                adjacent[index].update(triangle)
        remaining, side_points = set(adjacent), []
        while remaining:
            pending = [remaining.pop()]
            component = set(pending)
            while pending:
                for index in adjacent[pending.pop()] - component:
                    component.add(index); remaining.discard(index); pending.append(index)
            surface = points[list(component)]
            if not (surface[:, 0].min() < -.1 and surface[:, 0].max() > .1):
                side_points.append(surface)
        if side_points:
            points = np.concatenate(side_points)
    return [points[points[:, 0] * side > .1].mean(axis=0).tolist()
            for side in (-1, 1) if np.any(points[:, 0] * side > .1)]


def source_anchors(source: dict) -> tuple[dict, dict]:
    """Return authored lamp anchors and evidence, before body-height alignment.

    Three sources bake their lamps into BODY_H instead of providing TLIGHT.
    Their measured lamp positions are recorded separately from game tuning.
    Unknown sources without tail helpers are left unchanged, never guessed.
    """
    overrides = json.loads(Path(__file__).with_name("light_anchors.json").read_text(encoding="utf-8"))
    selected = overrides.get(source.get("name"))
    tail = _helper_anchors(source, ("TLIGHT_H", "TLIGHT"))
    head = _helper_anchors(source, ("HLIGHT_H", "HLIGHT"))
    if not head:
        # These helpers are authored lamp centers/cards, not body surfaces.
        # Read both numbered helpers: choosing only the first hides one side.
        head = sorted((point for names in (("HEADLIGHT0_H", "HEADLIGHT0"),
                                           ("HEADLIGHT1_H", "HEADLIGHT1"))
                       for point in _helper_anchors(source, names)), key=lambda point: point[0])
    if not tail and not head and selected is None:
        return {}, {}
    brake = _helper_anchors(source, ("BLIGHT_H", "BLIGHT"))
    # A high-mounted central brake strip supplements the main side lamps.
    # A donor with only two brake effects keeps its main side lamps first.
    if brake and max(abs(point[0]) for point in brake) < .3:
        brake = tail + [np.mean(brake, axis=0).tolist()]
    reverse = _helper_anchors(source, ("RLIGHT_H", "RLIGHT"))
    anchors = ({"backlight": tail, "brakelight": brake or tail,
                "reverselight": reverse or tail} if tail else {})
    evidence = {"method": "dedicated source light meshes", "source": source.get("name"),
                "reverse_fallback_to_tail": not bool(reverse)}
    if selected is not None:
        anchors.update({name: selected[name] for name in REAR_LIGHT_TYPES if name in selected})
        evidence.update(method="measured source lamp surfaces", notes=selected["notes"])
    if head:
        anchors["headlight"] = head
        evidence["headlight_method"] = "dedicated source light meshes"
    elif selected is not None and "headlight" in selected:
        anchors["headlight"] = selected["headlight"]
        evidence["headlight_method"] = "measured source lamp surfaces or confirmed absence"
    return anchors, evidence


def _effect_offset(node: mm3_model.Node) -> int:
    return 4 + len(mm3_model.cstring(node.name))


def align_lights(blob: bytes, anchors: dict, *, source_transform=None) -> tuple[bytes, dict]:
    """Move effect parent transforms without touching any mesh payload.

    Keep all native node names and types, since runtime code can reference them.
    Surplus effects have zero brightness and size so they cannot float around a
    single-center F1 lamp or duplicate the same lamp's glow. Other effects keep
    their donor settings. Both stored world and relative translations are set.
    """
    if not anchors:
        return blob, {"moved": [], "disabled": []}
    unknown = anchors.keys() - LIGHT_TYPES.keys()
    if unknown:
        raise ValueError(f"Unknown light types: {sorted(unknown)}")
    transform = np.eye(4) if source_transform is None else np.asarray(source_transform, dtype=float)
    if transform.shape != (4, 4) or not np.isfinite(transform).all():
        raise ValueError("source_transform must be a finite 4x4 matrix")
    positions = {}
    for name, points in anchors.items():
        points = np.asarray(points, dtype=float)
        if not points.size:
            positions[name] = np.empty((0, 3))
        elif points.ndim != 2 or points.shape[1] != 3 or not np.isfinite(points).all():
            raise ValueError(f"{name} anchors must be finite XYZ triples")
        else:
            positions[name] = points @ transform[:3, :3].T + transform[:3, 3]
    model = mm3_model.read_model(blob)
    groups = {name: [] for name in anchors}
    if "headlight" in groups:
        groups.setdefault("headlightenv", [])

    def visit(node, parent, parent_matrix, ancestor_matrix, path):
        matrix = parent_matrix @ node.relative_matrix()
        path += "/" + node.name
        if node.kind == 4:
            kind = struct.unpack_from("<I", node.prefix, _effect_offset(node))[0]
            name = next((name for name, value in LIGHT_TYPES.items() if value == kind), None)
            if name in groups:
                if parent is None or parent.kind != 1 or parent.transform_offset is None:
                    raise mm3_model.FormatError(f"Light effect has no movable transform: {path}")
                # Effect parents must be dedicated helpers, not shared bodies.
                if len(parent.children) != 1:
                    raise mm3_model.FormatError(f"Light effect shares its transform: {path}")
                groups[name].append((node, parent, ancestor_matrix, matrix[:3, 3].copy(), path))
        for child in node.children:
            visit(child, node, matrix, parent_matrix, path)

    visit(model.root, None, np.eye(4), np.eye(4), "")
    if "headlight" in positions and "headlightenv" not in positions:
        # Native HeadLightEnv projects the player's illumination onto the road.
        # Carry the donor's small forward offset from its lamp midpoint to the
        # new lamp midpoint; keeping the old nose leaves the beam inside the
        # imported body. Retain every donor orientation and effect setting.
        lamps = [entry for entry in groups["headlight"]
                 if struct.unpack_from("<f", entry[0].prefix, _effect_offset(entry[0]) + 12)[0] > 0]
        road_lamps = groups["headlightenv"]
        if len(positions["headlight"]) and lamps and road_lamps:
            old_center = np.mean([entry[3] for entry in lamps], axis=0)
            new_center = np.mean(positions["headlight"], axis=0)
            delta = new_center - old_center
            # CMP transforms round to float32. Do not accumulate their tiny
            # rounding residual in the shared beam on every repair/rebuild.
            if np.max(np.abs(delta)) < 1e-6:
                delta[:] = 0
            positions["headlightenv"] = np.asarray([entry[3] + delta
                                                   for entry in road_lamps])
        else:
            positions["headlightenv"] = np.empty((0, 3))
    report = {"moved": [], "disabled": []}
    for name, entries in groups.items():
        targets = positions[name]
        count = min(len(entries), len(targets))
        selected = {}
        if count:
            # Prefer the original left/right or center handles. Source anchor
            # order also prioritizes main lamps before an auxiliary brake bar.
            enabled = [i for i, entry in enumerate(entries)
                       if struct.unpack_from("<f", entry[0].prefix, _effect_offset(entry[0]) + 12)[0] > 0]
            candidates = enabled if len(enabled) >= count else list(range(len(entries)))
            original = np.array([entries[i][3] for i in candidates])
            old_x, new_x = original[:, 0], targets[:count, 0]
            old_x = (old_x - (old_x.min() + old_x.max()) / 2) / max(np.ptp(old_x), 1e-6)
            new_x = (new_x - (new_x.min() + new_x.max()) / 2) / max(np.ptp(new_x), 1e-6)
            # Validated donors have at most seven rear effects of one type.
            # Enumerating these tiny assignments keeps this repair usable
            # without the geometry writer's optional SciPy shadow dependency.
            rows = min(permutations(range(len(candidates)), count),
                       key=lambda rows: sum(abs(old_x[row] - new_x[column])
                                            for column, row in enumerate(rows)))
            selected = {candidates[row]: column for column, row in enumerate(rows)}
        for index, (effect, node, parent_matrix, old, path) in enumerate(entries):
            if index in selected:
                position = targets[selected[index]]
                relative = np.linalg.solve(parent_matrix[:3, :3], position - parent_matrix[:3, 3])
                node.set_translation(position, relative)
                report["moved"].append({"node": path, "type": name,
                    "from": old.tolist(), "to": position.tolist()})
            else:
                prefix = bytearray(effect.prefix)
                offset = _effect_offset(effect)
                # LightNode: brightness and size; versions 114+ also store
                # brightness/size for the secondary effect at offsets 32/36.
                for delta in (12, 20) + ((32, 36) if model.version >= 114 else ()):
                    struct.pack_into("<f", prefix, offset + delta, 0)
                effect.prefix = bytes(prefix)
                report["disabled"].append({"node": path, "type": name,
                    "reason": "no corresponding source lamp"})
    return model.write(), report


def align_rear_lights(blob: bytes, anchors: dict, *, source_transform=None) -> tuple[bytes, dict]:
    """Compatibility entry point for existing rear-only repair callers."""
    return align_lights(blob, anchors, source_transform=source_transform)
