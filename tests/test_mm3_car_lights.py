"""Imported lamp alignment preserves mesh data and native effect handles."""
import copy
import json
from pathlib import Path
import struct
import sys
import unittest

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/car-models"))
import mm3_lights
from mm3_model import FormatError, MeshBlock, Model, Node, read_model


def transformation():
    identity = (1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0)
    return struct.pack("<I28f", 0, *(identity * 2), 0, 0, 0, 0)


def template_block(version):
    return MeshBlock("retain_geometry.dob", b"unchanged mesh payload")


def model_for(version, block):
    prefix = bytearray(struct.pack("<II", 0, version) + b"root\0")
    prefix += struct.pack("<I6fIHB", 123, -1, -1, -1, 1, 1, 1, 456, 7, 8)
    if version >= 114:
        prefix += b"donor/path\0"
    offset = len(prefix)
    prefix += transformation() + struct.pack("<IB6fI6fI", 9, 10, -1, -1, -1, 1, 1, 1,
                                             0, 0, 0, 0, 0, 0, 0, 0)
    root = Node(0, "root", bytes(prefix), transform_offset=offset, bounds_offset=17)
    root.children.append(Node(2, "mesh", struct.pack("<I", 2) + b"mesh\0" + b"\0" * 16,
                              meshes=[block], terminator=b"\0"))
    return Model(version, root).write()


def group(name, children, position=(0, 0, 0)):
    prefix = struct.pack("<I", 1) + name.encode() + b"\0"
    offset = len(prefix)
    prefix += transformation() + struct.pack("<i6f", -1, -1, -1, -1, 1, 1, 1)
    node = Node(1, name, prefix, children, transform_offset=offset, bounds_offset=offset + 120)
    node.set_translation(np.asarray(position), np.asarray(position))
    return node


def effect(name, kind, version=115):
    payload = struct.pack("<2I5f4B", kind, 1, .1, .8, .3, .025, .3, 0, 0, 255, 255)
    if version >= 114:
        payload += struct.pack("<2f", .9, .035)
    return Node(4, name, struct.pack("<I", 4) + name.encode() + b"\0" + payload)


def world_positions(model):
    found = {}
    def visit(node, parent):
        current = parent @ node.relative_matrix()
        if node.kind == 4:
            found[node.name] = current[:3, 3]
        for child in node.children:
            visit(child, current)
    visit(model.root, np.eye(4))
    return found


class ImportedRearLightTest(unittest.TestCase):
    def test_front_pair_and_road_light_move_together_and_remain_idempotent(self):
        model = read_model(model_for(115, template_block(115)))
        model.root.children.extend([
            group("left", [effect("left", 0)], (-.6, .4, -1.8)),
            group("left_extra", [effect("left_extra", 0)], (-.8, .4, -1.8)),
            group("right", [effect("right", 0)], (.6, .4, -1.8)),
            group("right_extra", [effect("right_extra", 0)], (.8, .4, -1.8)),
            group("beam", [effect("beam", 8)], (0, .4, -1.9)),
            group("tail", [effect("tail", 1)], (0, .4, 2))])
        original = {path: node for path, node in model.root.walk()}
        anchors = {"headlight": [[-.75, .6, -2.2], [.75, .6, -2.2]]}
        tf = np.eye(4); tf[1, 3] = -.2
        blob, report = mm3_lights.align_lights(model.write(), anchors, source_transform=tf)
        after = read_model(blob)
        moved = [item["to"] for item in report["moved"] if item["type"] == "headlight"]
        np.testing.assert_allclose(sorted(moved), [[-.75, .4, -2.2], [.75, .4, -2.2]])
        np.testing.assert_allclose(world_positions(after)["beam"], [0, .4, -2.3], atol=1e-6)
        self.assertEqual(len(report["disabled"]), 2)
        for path, node in after.root.walk():
            self.assertEqual(node.meshes, original[path].meshes)
            if node.kind == 4 and node.name not in ("left_extra", "right_extra", "left", "right"):
                self.assertEqual(node.prefix, original[path].prefix)
        self.assertEqual(mm3_lights.align_lights(blob, anchors, source_transform=tf)[0], blob)

    def test_confirmed_no_front_lamps_disables_headlight_and_road_light_handles(self):
        model = read_model(model_for(115, template_block(115)))
        model.root.children.extend(group(name, [effect(name, kind)], (.6, .4, -2))
                                   for name, kind in (("head", 0), ("beam", 8), ("tail", 1)))
        before = model.write()
        blob, report = mm3_lights.align_lights(before, {"headlight": []})
        self.assertEqual(len(report["disabled"]), 2)
        after = read_model(blob)
        self.assertEqual([path for path, _ in model.root.walk()], [path for path, _ in after.root.walk()])
        for _, node in after.root.walk():
            if node.kind == 4 and node.name != "tail":
                offset = 4 + len(node.name) + 1
                for delta in (12, 20, 32, 36):
                    self.assertEqual(struct.unpack_from("<f", node.prefix, offset + delta)[0], 0)
        self.assertEqual(mm3_lights.align_lights(blob, {"headlight": []})[0], blob)

    def test_front_beam_does_not_accumulate_float32_rounding_on_repair(self):
        model = read_model(model_for(115, template_block(115)))
        model.root.children.extend([
            group("left", [effect("left", 0)], (-.6, .4, -1.8)),
            group("right", [effect("right", 0)], (.6, .4, -1.8)),
            group("beam", [effect("beam", 8)], (0, .4, -1.9))])
        anchors = {"headlight": [[-.7629214823246002, .6775417476887703, -2.1886097490787506],
                                  [.7641407400369644, .6777004972100258, -2.1947839856147766]]}
        blob, _ = mm3_lights.align_lights(model.write(), anchors)
        for _ in range(10):
            self.assertEqual(mm3_lights.align_lights(blob, anchors)[0], blob)

    def test_numbered_front_helpers_keep_both_sides_when_authored_order_is_reversed(self):
        source = {"meshes": [dict(name=name, positions=[[x, .6, -2]], normals=[[0, 0, -1]],
                                  uvs=[[0, 0]], triangles=[])
                             for name, x in (("HEADLIGHT0_H", .7), ("HEADLIGHT1_H", -.7))]}
        anchors, _ = mm3_lights.source_anchors(source)
        self.assertEqual(anchors, {"headlight": [[-.7, .6, -2], [.7, .6, -2]]})

    def test_world_relative_height_and_z_conversion_preserve_meshes(self):
        for version in (109, 114, 115):
            with self.subTest(version=version):
                model = read_model(model_for(version, template_block(version)))
                lamps = [group("left", [effect("left", 1, version)], (-.7, .4, 1.9)),
                         group("right", [effect("right", 1, version)], (.7, .4, 1.9)),
                         group("head", [effect("head", 0, version)], (.7, .4, -1.9))]
                car = group("car", lamps, (3, .2, -1))
                model.root.children.append(car)
                before = model.write()
                originals = {path: node for path, node in read_model(before).root.walk()}
                source_tf = np.eye(4); source_tf[1, 3] = -.3
                blob, report = mm3_lights.align_rear_lights(before,
                    {"backlight": [[-1, .8, 2.4], [1, .8, 2.4]]}, source_transform=source_tf)
                after = read_model(blob)
                positions = world_positions(after)
                np.testing.assert_allclose(positions["left"], [-1, .5, 2.4], atol=1e-6)
                np.testing.assert_allclose(positions["right"], [1, .5, 2.4], atol=1e-6)
                self.assertEqual(len(report["moved"]), 2)
                self.assertEqual(report["disabled"], [])
                for path, node in after.root.walk():
                    old = originals[path]
                    self.assertEqual(node.meshes, old.meshes, path)
                    if node.kind == 4 or node.name not in ("left", "right"):
                        self.assertEqual(node.prefix, old.prefix, path)
                    elif node.transform_offset is not None:
                        off = node.transform_offset
                        xyz = [-1 if node.name == "left" else 1, .5, 2.4]
                        np.testing.assert_allclose(struct.unpack_from("<3f", node.prefix, off + 40),
                                                   np.asarray(xyz) * [1, 1, -1], atol=1e-6)
                        np.testing.assert_allclose(struct.unpack_from("<3f", node.prefix, off + 88),
                            (np.asarray(xyz) - [3, .2, -1]) * [1, 1, -1], atol=1e-6)

    def test_center_f1_lamp_disables_surplus_effects_without_removing_handles(self):
        for version in (109, 114, 115):
            with self.subTest(version=version):
                model = read_model(model_for(version, template_block(version)))
                lamps = [group(name, [effect(name, 2, version)], (x, .7, 2))
                         for name, x in (("outer_left", -.6), ("center", 0), ("outer_right", .6))]
                model.root.children.extend(lamps)
                names = [path for path, node in model.root.walk()]
                blob, report = mm3_lights.align_rear_lights(model.write(),
                    {"brakelight": [[-.004, .12, 2.652]]})
                after = read_model(blob)
                self.assertEqual([path for path, node in after.root.walk()], names)
                self.assertEqual(len(report["moved"]), 1)
                self.assertTrue(report["moved"][0]["node"].endswith("/center/center"))
                self.assertEqual(len(report["disabled"]), 2)
                for path, node in after.root.walk():
                    if node.kind != 4 or node.name == "center":
                        continue
                    off = 4 + len(node.name) + 1
                    for delta in (12, 20) + ((32, 36) if version >= 114 else ()):
                        self.assertEqual(struct.unpack_from("<f", node.prefix, off + delta)[0], 0)
                    self.assertEqual(struct.unpack_from("<I", node.prefix, off)[0], 2)
                np.testing.assert_allclose(world_positions(after)["center"], [-.004, .12, 2.652], atol=1e-6)
                self.assertEqual(mm3_lights.align_rear_lights(blob,
                    {"brakelight": [[-.004, .12, 2.652]]})[0], blob)

    def test_reapplying_pair_keeps_active_handles_when_surplus_donor_lamps_remain(self):
        model = read_model(model_for(115, template_block(115)))
        model.root.children.extend(group(name, [effect(name, 3)], (x, .4, 2))
            for name, x in (("left_outer", -.7), ("left_inner", -.5),
                            ("right_inner", .5), ("right_outer", .7)))
        anchors = {"reverselight": [[-.4, .2, 2.2], [.4, .2, 2.2]]}
        blob, _ = mm3_lights.align_rear_lights(model.write(), anchors)
        self.assertEqual(mm3_lights.align_rear_lights(blob, anchors)[0], blob)

    def test_shared_body_transform_is_rejected(self):
        model = read_model(model_for(115, template_block(115)))
        model.root.children.append(group("body", [effect("lamp", 1), effect("other", 0)]))
        with self.assertRaisesRegex(FormatError, "shares its transform"):
            mm3_lights.align_rear_lights(model.write(), {"backlight": [[0, .5, 2]]})

    def test_rotated_scaled_ancestor_keeps_requested_world_position(self):
        model = read_model(model_for(115, template_block(115)))
        parent = group("car", [group("lamp", [effect("lamp", 1)])], (3, .2, -1))
        prefix = bytearray(parent.prefix)
        # Quarter-turn yaw and nonuniform scale in the ancestor linear matrix.
        matrix = np.array([[0, 0, 2], [0, 3, 0], [-1, 0, 0]], dtype=float)
        for delta in (4, 52):
            struct.pack_into("<9f", prefix, parent.transform_offset + delta, *matrix.ravel())
        parent.prefix = bytes(prefix)
        model.root.children.append(parent)
        blob, _ = mm3_lights.align_rear_lights(model.write(), {"backlight": [[-.7, .4, 2.3]]})
        np.testing.assert_allclose(world_positions(read_model(blob))["lamp"], [-.7, .4, 2.3], atol=1e-6)

    def test_ford_helper_center_brake_bar_does_not_pull_side_ring_anchors_inward(self):
        source = {"name": "custom", "meshes": [dict(name="TLIGHT",
            positions=[[-.8, .7, 2], [-.7, .7, 2], [-.75, .8, 2],
                       [.8, .7, 2], [.7, .7, 2], [.75, .8, 2],
                       [-.15, .9, 2.1], [.15, .9, 2.1], [0, 1, 2.1]],
            normals=[[0, 0, 1]] * 9, uvs=[[0, 0]] * 9,
            triangles=[[0, 1, 2, 0], [3, 4, 5, 0], [6, 7, 8, 0]])]}
        anchors, _ = mm3_lights.source_anchors(source)
        np.testing.assert_allclose(anchors["backlight"], [[-.75, 2.2 / 3, 2], [.75, 2.2 / 3, 2]])

    def test_source_helpers_deduplicate_uv_seams_and_keep_authored_transform(self):
        tf = np.eye(4); tf[:3, 3] = [0, .3, 2]
        source = {"name": "custom", "meshes": [dict(name="TLIGHT_H",
            positions=[[-.6, .1, 0], [-.8, .1, 0], [-.6, .1, 0], [.6, .1, 0], [.8, .1, 0]],
            normals=[[0, 0, 1]] * 5, uvs=[[0, 0]] * 5, triangles=[], transform=tf.tolist())]}
        before = copy.deepcopy(source)
        anchors, evidence = mm3_lights.source_anchors(source)
        np.testing.assert_allclose(anchors["backlight"], [[-.7, .4, 2], [.7, .4, 2]])
        self.assertEqual(anchors["brakelight"], anchors["backlight"])
        self.assertEqual(anchors["reverselight"], anchors["backlight"])
        self.assertEqual(source, before)
        self.assertEqual(evidence["method"], "dedicated source light meshes")

    def test_unknown_sources_do_not_guess_effect_positions(self):
        self.assertEqual(mm3_lights.source_anchors({"meshes": []}), ({}, {}))
        blob = model_for(115, template_block(115))
        self.assertEqual(mm3_lights.align_rear_lights(blob, {})[0], blob)

    def test_roster_source_anchors_cover_every_imported_car(self):
        source_root = ROOT / "out/car-models/source"
        sources = list(source_root.glob("*/model.json"))
        if not sources:
            self.skipTest("Extracted source cars are not present")
        for path in sources:
            with self.subTest(source=path.parent.name):
                anchors, _ = mm3_lights.source_anchors(json.loads(path.read_text(encoding="utf-8")))
                self.assertTrue(anchors["backlight"])
                self.assertTrue(anchors["brakelight"])
                self.assertIn("headlight", anchors)
                if path.parent.name == "ferrari-f2007":
                    self.assertEqual(anchors["headlight"], [])
                else:
                    self.assertEqual(len(anchors["headlight"]), 2)
                    self.assertTrue(all(point[2] < -1.7 for point in anchors["headlight"]))
                for name, points in anchors.items():
                    if name == "headlight":
                        continue
                    for point in points:
                        self.assertGreater(point[2], 1.8, "Rear anchor must be behind the car, +Z")


if __name__ == "__main__":
    unittest.main()
