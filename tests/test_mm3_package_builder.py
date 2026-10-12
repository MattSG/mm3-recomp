"""Exact solid-UV handling and opt-in minified texture approximation."""
import copy
import hashlib
import io
import json
from pathlib import Path
import re
import sys
import tempfile
import unittest
from unittest.mock import patch

import numpy as np
from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/car-models"))
import build_package as builder


def mesh_fixture():
    return dict(name="BODY_H",
        positions=[[0, 0, 0], [1, 0, 0], [0, 1, 0], [.2, .2, 0], [.3, .1, 0]],
        normals=[[0, 0, 1]] * 5,
        uvs=[[0, 0], [80, 0], [0, 80], [.1, .2], [.3, .4]],
        triangles=[[0, 1, 2, 0], [0, 3, 4, 0], [0, 1, 2, 1], [0, 1, 2, 2]])


def fixture_source(folder):
    folder.mkdir()
    texture = Image.new("RGBA", (2, 2))
    texture.putdata([(10, 30, 50, 255), (50, 70, 90, 255),
                     (90, 110, 130, 255), (130, 150, 170, 255)])
    texture.save(folder / "detail.png")
    Image.new("RGBA", (2, 2), (17, 23, 41, 255)).save(folder / "solid.png")
    source = dict(materials=[
        dict(name="detail", texture="detail.png", diffuse=[.5, .25, .8, .75]),
        dict(name="solid", texture="solid.png", diffuse=[1, 1, 1, 1]),
        dict(name="flat", texture=None, diffuse=[.1, .2, .3, 1])],
        meshes=[mesh_fixture()])
    for name in builder.mm3_model.WHEEL_NODES.values():
        source["meshes"].append(dict(name=name, positions=[[0, 0, 0], [.1, 0, 0], [0, .1, 0]],
            normals=[[0, 0, 1]] * 3, uvs=[[0, 0], [.8, 0], [0, .8]],
            triangles=[[0, 1, 2, 0]], transform=np.eye(4).tolist()))
    path = folder / "model.json"
    path.write_text(json.dumps(source), encoding="utf-8")
    return path, source


def input_bytes(folder):
    return {path.relative_to(folder).as_posix(): path.read_bytes()
            for path in folder.rglob("*") if path.is_file()}


class PackageBuilderTest(unittest.TestCase):
    def test_equal_textures_with_different_gloss_keep_separate_surface_slots(self):
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp) / "source"
            path, source = fixture_source(folder)
            source["materials"][1] = dict(source["materials"][0], shininess=.5)
            source["materials"][0]["shininess"] = 0
            path.write_text(json.dumps(source), encoding="utf-8")
            captured = []

            def capture(template, source, **kwargs):
                captured.append(kwargs)
                return b"test-cmp", dict(bytes=8, triangles=8)

            with patch.object(builder, "Data") as data, \
                    patch.object(builder, "alias_paths", side_effect=lambda blob, donor, alias: blob), \
                    patch.object(builder.mm3_model, "convert_exterior", side_effect=capture), \
                    patch.object(builder.mm3_textures, "image_to_cdds", return_value=b"test-cdds"), \
                    patch("sys.stdout", new=io.StringIO()):
                data.return_value.read.return_value = b"donor"
                report = builder.build(path, Path(tmp) / "package", 9004, y_offset=0)
            self.assertNotEqual(report["material_map"][0], report["material_map"][1])
            for kwargs in captured:
                shading = kwargs["material_shading"]
                self.assertEqual(shading[report["material_map"][0]], builder.mm3_model.MATTE_VERTEX_SHADING)
                self.assertEqual(shading[report["material_map"][1]], builder.mm3_model.GLOSS_VERTEX_SHADING)

    def test_constant_faces_clone_shared_vertices_without_changing_detail(self):
        mesh = mesh_fixture()
        before = copy.deepcopy(mesh)
        # Two solid materials share all three vertices with a detailed face.
        self.assertEqual(builder.collapse_constant_uvs([mesh], {1, 2}), 2)
        self.assertEqual(mesh["triangles"][:2], before["triangles"][:2])
        self.assertEqual(mesh["uvs"][:5], before["uvs"])
        self.assertEqual(mesh["positions"][:5], before["positions"])
        self.assertEqual(mesh["normals"][:5], before["normals"])
        self.assertTrue(set(mesh["triangles"][2][:3]).isdisjoint(mesh["triangles"][3][:3]))
        for face_index in (2, 3):
            for original, cloned in zip(before["triangles"][face_index][:3],
                                        mesh["triangles"][face_index][:3]):
                self.assertGreaterEqual(cloned, 5)
                self.assertEqual(mesh["positions"][cloned], before["positions"][original])
                self.assertEqual(mesh["normals"][cloned], before["normals"][original])
                self.assertEqual(mesh["uvs"][cloned], [.5, .5])

    def test_average_changes_only_heavy_nonconstant_face_and_multiplies_diffuse(self):
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp) / "source"
            path, source = fixture_source(folder)
            files_before = input_bytes(folder)
            before = copy.deepcopy(source)
            body = source["meshes"][0]
            report = builder.average_minified_repeats(source, source["meshes"], folder.resolve())
            self.assertEqual([(r["mesh"], r["triangle"], r["source_material"]) for r in report],
                             [("BODY_H", 0, 0)])
            self.assertEqual(body["triangles"][1:], before["meshes"][0]["triangles"][1:])
            self.assertEqual(body["triangles"][0][:3], before["meshes"][0]["triangles"][0][:3])
            for field in ("positions", "normals", "uvs"):
                self.assertEqual(body[field], before["meshes"][0][field])
            self.assertEqual(source["materials"][:3], before["materials"])
            self.assertEqual(source["meshes"][1:], before["meshes"][1:])
            flat = source["materials"][body["triangles"][0][3]]
            self.assertIsNone(flat["texture"])
            mean = np.array([70, 90, 110, 255]) / 255
            np.testing.assert_allclose(report[0]["average_rgba"], mean, atol=1e-14)
            np.testing.assert_allclose(flat["diffuse"], mean * [.5, .25, .8, .75], atol=1e-14)
            builder.collapse_constant_uvs([body], {body["triangles"][0][3]})
            detailed = body["triangles"][1]
            self.assertEqual(detailed, before["meshes"][0]["triangles"][1])
            self.assertEqual([body["uvs"][i] for i in detailed[:3]],
                             [before["meshes"][0]["uvs"][i] for i in detailed[:3]])
            self.assertEqual(input_bytes(folder), files_before)

    def test_build_approximation_is_opt_in_and_source_files_stay_unchanged(self):
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp) / "source"
            path, original = fixture_source(folder)
            files_before, captured = input_bytes(folder), []

            def capture(template, source, **kwargs):
                captured.append(copy.deepcopy(source))
                return b"test-cmp", dict(bytes=8, triangles=8)

            with patch.object(builder, "Data") as data, \
                    patch.object(builder, "alias_paths", side_effect=lambda blob, donor, alias: blob), \
                    patch.object(builder.mm3_model, "convert_exterior", side_effect=capture), \
                    patch.object(builder.mm3_textures, "image_to_cdds", return_value=b"test-cdds"), \
                    patch("sys.stdout", new=io.StringIO()):
                data.return_value.read.return_value = b"donor"
                default_report = builder.build(path, Path(tmp) / "default", 9004, y_offset=0)
                enabled_report = builder.build(path, Path(tmp) / "enabled", 9004,
                                               y_offset=0, average_minified=True)
            self.assertEqual(default_report["averaged_minified_faces"], [])
            self.assertEqual(len(enabled_report["averaged_minified_faces"]), 1)
            # Both near/far builds see the same choice; detailed material/UVs stay authored.
            for source in captured[:2]:
                self.assertEqual(source["meshes"][0]["triangles"][:2], original["meshes"][0]["triangles"][:2])
                self.assertEqual(source["meshes"][0]["uvs"][:5], original["meshes"][0]["uvs"])
            for source in captured[2:]:
                self.assertNotEqual(source["meshes"][0]["triangles"][0][3], 0)
                self.assertEqual(source["meshes"][0]["triangles"][1], original["meshes"][0]["triangles"][1])
                self.assertEqual(source["meshes"][0]["uvs"][:5], original["meshes"][0]["uvs"])
            self.assertEqual(input_bytes(folder), files_before)

    def test_f2007_approximation_is_exactly_two_carbon_faces(self):
        path = ROOT / "out/car-models/source/ferrari-f2007/model.json"
        report_path = ROOT / "out/car-models/packages/Form1/conversion-report.json"
        if not path.is_file():
            self.skipTest("Extracted Ferrari F2007 fixture is not present")
        source = json.loads(path.read_text(encoding="utf-8"))
        meshes = [mesh for mesh in source["meshes"]
                  if re.fullmatch(r"BODY_H|BREAK\d+_H|FNDR[01]_H|WHL[0-3]_H", mesh["name"])]
        inputs = {path} | {path.parent / material["texture"] for material in source["materials"]
                          if material.get("texture")}
        hashes = {file: hashlib.sha256(file.read_bytes()).digest() for file in inputs}
        faces_before = {mesh["name"]: copy.deepcopy(mesh["triangles"]) for mesh in meshes}
        report = builder.average_minified_repeats(source, meshes, path.parent.resolve())
        self.assertEqual([(face["mesh"], face["triangle"], face["source_material"]) for face in report],
                         [("BODY_H", 20205, 7), ("BODY_H", 20206, 7)])
        changed = [(mesh["name"], index) for mesh in meshes
                   for index, triangle in enumerate(mesh["triangles"])
                   if triangle != faces_before[mesh["name"]][index]]
        self.assertEqual(changed, [("BODY_H", 20205), ("BODY_H", 20206)])
        self.assertEqual({file: hashlib.sha256(file.read_bytes()).digest() for file in inputs}, hashes)
        if report_path.is_file():
            prepared = json.loads(report_path.read_text(encoding="utf-8"))
            self.assertEqual(prepared["averaged_minified_faces"], report)


if __name__ == "__main__":
    unittest.main()
