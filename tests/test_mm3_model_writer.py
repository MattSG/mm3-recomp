"""Packed CMP compatibility and geometry-preservation tests (no game process)."""
import importlib.util
from pathlib import Path
import struct
import sys
import unittest

import numpy as np


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "car-models"))
from mm3_model import (MeshBlock, Model, Node, convert_exterior, encode_mesh,
                       normalize_repeat_uv, read_model, remap_mesh_materials,
                       write_omb)
sys.path.insert(0, str(ROOT / "tools" / "mm3view"))
from mm3view import read_cmp, mesh_triangles, read_omb


def template_block(version):
    out = bytearray(struct.pack("<f6f2I3I", 1, -1, -1, -1, 1, 1, 1, 3, 3, 0, 1, 1))
    out += struct.pack("<4f" if version >= 115 else "<f", *([1] * (4 if version >= 115 else 1)))
    out += b"template.dob\0\1" + struct.pack("<II3H2HB4I", 0, 6, 0, 1, 2, 1, 32, 0, 3, 24, 72, 8)
    out += b"\0" * 72
    out += struct.pack("<I5H5HI4IBI", 2, 0x6001, 0, 2, 0, 0, 0, 0, 0, 0, 0,
                       1, 0, 3, 0, 1, 0, 0)
    if version >= 115:
        out += struct.pack("<I", 0)
    return MeshBlock("template", bytes(out))


def transformation():
    identity = (1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0)
    return struct.pack("<I28f", 0, *(identity * 2), 0, 0, 0, 0)


def model_for(version, block):
    root = bytearray(struct.pack("<II", 0, version) + b"root\0")
    root += struct.pack("<I6fIHB", 123, -1, -1, -1, 1, 1, 1, 456, 7, 8)
    if version >= 114:
        root += b"donor/path\0"
    transform_offset = len(root)
    root += transformation() + struct.pack("<IB6fI6fI", 9, 10, -1, -1, -1, 1, 1, 1,
                                         0, 0, 0, 0, 0, 0, 0, 0)
    node = Node(0, "root", bytes(root), transform_offset=transform_offset, bounds_offset=17)
    mesh = Node(2, "mesh", struct.pack("<I", 2) + b"mesh\0" + b"\0" * 16,
                meshes=[block], terminator=b"\0")
    node.children.append(mesh)
    return Model(version, node).write()


class MM3ModelWriterTest(unittest.TestCase):
    def test_versions_and_quantized_geometry(self):
        positions = np.array([[-.8, -.2, -1.5], [.9, .7, 1.4], [.4, -.1, .3]])
        normals = np.tile([0, 1, 0], (3, 1))
        for version in (109, 114, 115):
            with self.subTest(version=version):
                block = encode_mesh("new", positions, normals, [[0, 0], [.75, .8], [.2, .4]],
                                    [[0, 1, 2, 9]], {9: 22}, template_block(version), version)
                blob = model_for(version, block)
                self.assertEqual(read_model(blob).write(), blob)
                path, mesh, xforms, matrices = read_cmp(blob)[0]
                rows = mesh_triangles(mesh, xforms, matrices)[22]
                np.testing.assert_allclose(rows[:, :3], positions, atol=.006)
                np.testing.assert_allclose(rows[:, 3:6], normals, atol=.002)
                self.assertEqual(mesh["prims"], [((False, 0, 0), 22)])
                np.testing.assert_array_equal(mesh["verts"][:, 3] & 255, [244] * 3)
                np.testing.assert_array_equal(mesh["verts"][:, 3] >> 8 & 255, [4] * 3)

    def test_repeat_uv_clipping_preserves_surface_and_samples(self):
        p = np.array([[0, 0, 0], [1, 0, 0], [0, 1, 0]], dtype=float)
        n = np.tile([0, 0, 1], (3, 1))
        uv = np.array([[-2.2, -.2], [3.4, -.2], [-2.2, 2.6]])
        outp, outn, outuv, tris = normalize_repeat_uv(p, n, uv, [[0, 1, 2, 7]])
        self.assertGreater(len(tris), 1)
        self.assertGreaterEqual(outuv.min(), -1e-8)
        self.assertLessEqual(outuv.max(), 1 + 1e-8)
        triangle_positions = outp[tris[:, :3]]
        area = np.linalg.norm(np.cross(triangle_positions[:, 1] - triangle_positions[:, 0],
                                      triangle_positions[:, 2] - triangle_positions[:, 0]), axis=1).sum() / 2
        self.assertAlmostEqual(area, .5, places=10)
        authored_uv = uv[0] + outp[:, :1] * (uv[1] - uv[0]) + outp[:, 1:2] * (uv[2] - uv[0])
        delta = authored_uv - outuv
        np.testing.assert_allclose(delta, np.rint(delta), atol=1e-8)

    def test_negative_wheel_uvs_are_safe_for_native_clamp_sampler(self):
        p = np.array([[0, 0, 0], [1, 0, 0], [0, 1, 0]], dtype=float)
        n = np.tile([0, 0, 1], (3, 1))
        # Actual source wheel range is approximately U=.02..975, V=-.98..-.026.
        uv = np.array([[.02, -.98], [.975, -.026], [.2, -.4]])
        outp, outn, outuv, tris = normalize_repeat_uv(p, n, uv, [[0, 1, 2, 20]])
        np.testing.assert_array_equal(outp, p)
        np.testing.assert_array_equal(outn, n)
        np.testing.assert_array_equal(tris, [[0, 1, 2, 20]])
        np.testing.assert_allclose(outuv, uv + [0, 1], atol=1e-12)
        # Interior barycentric samples use the same repeated texture texels,
        # including where negative source coordinates would clamp to row0.
        barycentric = np.array([[1/3, 1/3, 1/3], [.6, .2, .2], [.2, .2, .6]])
        source_samples = np.mod(barycentric @ uv, 1)
        native_samples = np.clip(barycentric @ outuv, 0, 1)
        np.testing.assert_allclose(native_samples, source_samples, atol=1e-12)

    def test_corrupt_runaway_uvs_fail_before_expensive_seam_clipping(self):
        with self.assertRaisesRegex(ValueError, "source UV decoding"):
            normalize_repeat_uv([[0, 0, 0], [1, 0, 0], [0, 1, 0]],
                [[0, 0, 1]] * 3, [[0, 0], [200, 0], [0, 200]], [[0, 1, 2, 0]])

    def test_native_material_remap(self):
        template = template_block(115)
        block = encode_mesh("new", [[0, 0, 0], [1, 0, 0], [0, 1, 0]],
            [[0, 0, 1]] * 3, [[0, 0], [1, 0], [0, 1]], [[0, 1, 2, 11]], {}, template, 115)
        remapped = remap_mesh_materials(block, 115, {11: 22})
        decoded_mesh = read_cmp(model_for(115, remapped))[0][1]
        np.testing.assert_array_equal(decoded_mesh["verts"][:, 3] & 255, [244] * 3)
        rows = mesh_triangles(decoded_mesh, [], {})
        self.assertEqual(list(rows), [22])
        before = mesh_triangles(read_cmp(model_for(115, block))[0][1], [], {})[11]
        np.testing.assert_array_equal(before, rows[22])

    def test_material_limit_and_omb(self):
        blob = write_omb([dict(name="import", texture="+Cars\\Panoz\\imported_00.dds",
                               diffuse=[1, .5, .25, 1], mode=2)])
        material = read_omb(blob)[0]
        self.assertEqual(material["color"], (255, 128, 64, 255))
        self.assertEqual(material["mode"], 2)
        with self.assertRaisesRegex(ValueError, "23"):
            write_omb([dict(name="x")] * 24)
        with self.assertRaisesRegex(ValueError, "limit"):
            encode_mesh("x", [[0, 0, 0], [1, 0, 0], [0, 1, 0]], [[0, 0, 1]] * 3,
                        [[0, 0], [1, 0], [0, 1]], [[0, 1, 2, 23]], {}, template_block(115), 115)

    def test_palette_index_survives_shader_normalization(self):
        # Captured native shader: ARL(v3.z * c190.z), where packed D3DCOLOR
        # v3.z is the low byte / 255 and c190.z is 23 (sub_000D5497).
        # Pure index*11 truncated many IDs to the previous material.
        triangles = [[0, 1, 2, material] for material in range(23)]
        block = encode_mesh("palette", [[0, 0, 0], [1, 0, 0], [0, 1, 0]],
            [[0, 0, 1]] * 3, [[0, 0], [1, 0], [0, 1]], triangles, {}, template_block(115), 115)
        mesh = read_cmp(model_for(115, block))[0][1]
        encoded = mesh["verts"][:, 3] & 255
        shader_index = np.floor(encoded.astype(float) / 255 * 23 + .001).astype(int)
        np.testing.assert_array_equal(shader_index, np.repeat(np.arange(23), 3))

    def test_default_shading_preserves_opaque_texture_without_environment_mix(self):
        block = encode_mesh("matte", [[0, 0, 0], [1, 0, 0], [0, 1, 0]],
            [[0, 0, 1]] * 3, [[0, 0], [1, 0], [0, 1]], [[0, 1, 2, 0]], {},
            template_block(115), 115)
        words = read_cmp(model_for(115, block))[0][1]["verts"][:, 5]
        channels = np.array([(words >> shift) & 255 for shift in (16, 8, 0, 24)]).T / 255
        # Native D3DCOLOR fetch and captured retail car shader instructions
        # 30/37/40: reflection=fresnel*v5.y+v5.x, specular=light*v5.w,
        # and diffuse coverage=v5.z+(1-c138.w). The previous default supplied
        # .75 constant reflection and only .157 diffuse coverage.
        for fresnel in (0, .25, 1):
            np.testing.assert_array_equal(fresnel * channels[:, 1] + channels[:, 0], 0)
        np.testing.assert_array_equal(channels[:, 3], 0)
        for illumination in (0, .5, 1):
            self.assertTrue((channels[:, 2] + (1 - illumination) >= 1).all())

    def test_retail_templates_roundtrip(self):
        folder = ROOT / "game_files" / "Data" / "Shared" / "Cars"
        if not folder.is_dir():
            self.skipTest("Retail model files are not present")
        for alias in ("Viper", "Viper_lod1", "Panoz", "Panoz_lod1",
                      "Koenigsegg", "Koenigsegg_lod1", "Lotus", "Lotus_lod1"):
            path = folder / alias / "body.cmp"
            if not path.is_file():
                continue
            blob = path.read_bytes()
            self.assertEqual(read_model(blob).write(), blob, alias)

    def test_large_body_parts_all_draw_at_every_lod(self):
        file = ROOT / "game_files/Data/Shared/Cars/Koenigsegg/body.cmp"
        if not file.is_file():
            self.skipTest("Retail Koenigsegg donor is not present")
        original = read_model(file.read_bytes())
        body_transform = next(node for path, node in original.root.walk()
                              if node.kind == 1 and node.name == "body")
        body_template = next(node for node in body_transform.children if node.kind == 2)
        # Exceeds both the 65535-index and 65535-independent-vertex limits.
        # A compact grid keeps quantization error small in every mesh part.
        faces = 23000
        base = np.column_stack((np.arange(faces) % 200, np.arange(faces) // 200,
                                np.zeros(faces))) * .01
        positions = (base[:, None, :] + np.array([[0, 0, 0], [.005, 0, 0], [0, .005, 0]])).reshape(-1, 3)
        body = dict(name="BODY_H", positions=positions.tolist(),
                    normals=np.tile([0, 0, 1], (faces * 3, 1)).tolist(),
                    uvs=np.tile([[0, 0], [.8, 0], [0, .8]], (faces, 1)).tolist(),
                    triangles=np.column_stack((np.arange(faces * 3).reshape(-1, 3),
                                               np.arange(faces) % 2)).tolist())
        source = dict(meshes=[body])
        for name in ("WHL0_H", "WHL1_H", "WHL2_H", "WHL3_H"):
            source["meshes"].append(dict(name=name, positions=[[0, 0, 0], [.1, 0, 0], [0, .1, 0]],
                normals=[[0, 0, 1]] * 3, uvs=[[0, 0], [.8, 0], [0, .8]],
                triangles=[[0, 1, 2, 0]]))
        blob, report = convert_exterior(file.read_bytes(), source, keep_interior=False)
        self.assertEqual(read_model(blob).write(), blob)
        after = read_model(blob)
        group = next(node for path, node in after.root.walk()
                     if node.kind == 1 and node.name == "body")
        nodes = [node for node in group.children if node.kind in (2, 6)]
        self.assertEqual(len(nodes), 2)
        self.assertEqual(report["split_meshes"][0]["triangles"], [21845, 1155])
        # Original body identity and metadata remain available to game logic.
        self.assertEqual(nodes[0].name, body_template.name)
        self.assertEqual(nodes[0].prefix, body_template.prefix)
        self.assertEqual(nodes[1].prefix[4 + len(nodes[1].name) + 1:],
                         body_template.prefix[4 + len(body_template.name) + 1:])
        for lod in range(len(body_template.meshes)):
            self.assertEqual(sum(struct.unpack_from("<I", node.meshes[lod].payload, 32)[0] // 3
                                 for node in nodes), faces)
            for node in nodes:
                self.assertEqual(len(node.meshes), len(body_template.meshes))
                vertex_count, index_count = struct.unpack_from("<2I", node.meshes[lod].payload, 28)
                self.assertLessEqual(vertex_count, 65535)
                self.assertLessEqual(index_count, 65535)
        decoded = [(m, x, matrices) for path, m, x, matrices in read_cmp(blob) if "/body/" in path]
        self.assertEqual(sum(sum(len(rows) // 3 for rows in mesh_triangles(m, x, matrices).values())
                             for m, x, matrices in decoded), faces)
        # Lowercase car in this donor must also receive the imported culling box.
        car = next(node for path, node in after.root.walk() if node.name == "car" and node.kind == 1)
        bounds = struct.unpack_from("<6f", car.prefix, car.bounds_offset)
        self.assertGreater(bounds[3], 1.9)

    def test_exterior_replaces_all_lods_and_preserves_gameplay_nodes(self):
        model = read_model(model_for(115, template_block(115)))

        def group(name, child):
            prefix = struct.pack("<I", 1) + name.encode() + b"\0"
            transform_offset = len(prefix)
            prefix += transformation() + struct.pack("<i6f", -1, -1, -1, -1, 1, 1, 1)
            return Node(1, name, prefix, [child], transform_offset=transform_offset,
                        bounds_offset=transform_offset + 120)

        def mesh(name):
            return Node(2, name, struct.pack("<I", 2) + name.encode() + b"\0" + b"\0" * 16,
                        meshes=[template_block(115), MeshBlock("template", b"")])

        body = group("body", mesh("bodyShape"))
        wheels = [group(name, mesh(name + "Shape")) for name in
                  ("tireFrontLeft", "tireFrontRight", "tireBackLeft", "tireBackRight")]
        hidden = group("hood", mesh("hoodShape"))
        light = Node(4, "headlight", struct.pack("<I", 4) + b"headlight\0" + b"\x17" * 40)
        car = group("Car", body)
        car.children.extend(wheels + [hidden, light])
        model.root.children = [car]
        source = dict(meshes=[])
        for i, name in enumerate(("BODY_H", "WHL0_H", "WHL1_H", "WHL2_H", "WHL3_H")):
            transform = np.eye(4)
            if i:
                transform[:3, 3] = ((-1 if i % 2 else 1), .4, (-1 if i < 3 else 1))
            source["meshes"].append(dict(name=name,
                positions=[[0, 0, 0], [.1, 0, 0], [0, .1, 0]], normals=[[0, 0, 1]] * 3,
                uvs=[[0, 0], [.8, 0], [0, .8]], triangles=[[0, 1, 2, 0]], transform=transform.tolist()))
        tf = np.eye(4); tf[1, 3] = -.3
        converted, report = convert_exterior(model.write(), source, source_transform=tf,
                                             flip_winding=False)
        after = read_model(converted)
        self.assertEqual(len(report["replaced"]), 5)
        self.assertEqual(len(report["hidden"]), 1)
        self.assertEqual(dict(after.root.walk())["/root/Car/headlight"].prefix, light.prefix)
        for path, node in after.root.walk():
            if node.kind == 2:
                self.assertEqual(len(node.meshes), 2)
                self.assertTrue(all(block.payload for block in node.meshes), path)
        np.testing.assert_allclose(report["wheel_pivots"]["tireFrontLeft"], [-1, .1, -1])
        decoded = read_cmp(converted)
        wheel = next((m, x, matrices) for path, m, x, matrices in decoded if "tireFrontLeft/" in path)
        rows = mesh_triangles(*wheel)[0]
        np.testing.assert_allclose(rows[0, :3], [-1, .1, -1], atol=.002)

        # The same conventional CCW source should default to the retail face
        # convention after conversion, rather than exposing inside surfaces.
        converted, _ = convert_exterior(model.write(), source, source_transform=tf)
        native_meshes = read_cmp(converted)
        for path, mesh, xforms, matrices in native_meshes:
            if "/body/" not in path and "/tire" not in path:
                continue
            rows = mesh_triangles(mesh, xforms, matrices)[0]
            cross = np.cross(rows[1, :3] - rows[0, :3], rows[2, :3] - rows[0, :3])
            self.assertLess(float(cross @ rows[0, 3:6]), 0, path)

    def test_retail_front_faces_oppose_cross_product(self):
        file = ROOT / "game_files" / "Data" / "Shared" / "Cars" / "Viper" / "body.cmp"
        if not file.is_file():
            self.skipTest("Retail Viper is not present")
        positive, negative = 0, 0
        for path, mesh, xforms, matrices in read_cmp(file.read_bytes()):
            if "/body/" not in path and "/tire" not in path:
                continue
            for rows in mesh_triangles(mesh, xforms, matrices).values():
                triangles = rows.reshape(-1, 3, 12)
                cross = np.cross(triangles[:, 1, :3] - triangles[:, 0, :3],
                                 triangles[:, 2, :3] - triangles[:, 0, :3])
                dot = np.einsum("ij,ij->i", cross, triangles[:, :, 3:6].mean(1))
                positive += int((dot > 1e-8).sum())
                negative += int((dot < -1e-8).sum())
        self.assertGreater(negative, 1000)
        self.assertLess(positive, negative * .01)

    def test_unused_empty_helper_does_not_affect_selected_exterior(self):
        file = ROOT / "game_files/Data/Shared/Cars/Viper/body.cmp"
        if not file.is_file():
            self.skipTest("Retail Viper is not present")
        source = dict(meshes=[])
        for name in ("BODY_H", "WHL0_H", "WHL1_H", "WHL2_H", "WHL3_H"):
            source["meshes"].append(dict(name=name,
                positions=[[0, 0, 0], [.1, 0, 0], [0, .1, 0]],
                normals=[[0, 0, 1]] * 3, uvs=[[0, 0], [.8, 0], [0, .8]],
                triangles=[[0, 1, 2, 0]]))
        before, _ = convert_exterior(file.read_bytes(), source, keep_interior=False)
        source["meshes"].append(dict(name="UNUSED_HELPER_H", positions=[],
            normals=[], uvs=[], triangles=[], transform=[[0] * 4] * 4))
        after, _ = convert_exterior(file.read_bytes(), source, keep_interior=False)
        self.assertEqual(after, before)
        with self.assertRaisesRegex(ValueError, "Missing source meshes: ABSENT_H"):
            convert_exterior(file.read_bytes(), source, body_names=("ABSENT_H",))


if __name__ == "__main__":
    unittest.main()
