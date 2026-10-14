"""Material-specific imported reflections and lossless existing-package repair."""
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/car-models"))
import mm3_model
import repair_reflections


class ReflectionRepairTest(unittest.TestCase):
    def test_shared_legacy_material_chooses_gloss_without_losing_coverage(self):
        source = dict(materials=[dict(name="misc", shininess=.5), dict(name="misc", shininess=0)])
        shading, merged = repair_reflections.material_shading(source, dict(material_map={"0": 1, "1": 1}))
        self.assertEqual(shading, {1: mm3_model.GLOSS_VERTEX_SHADING})
        self.assertEqual(merged, [1])

    def test_all_available_packages_change_only_visible_shading_words(self):
        folders = sorted(path.parent for path in (ROOT / "out/car-models/packages").glob("*/package.json"))
        if not folders:
            self.skipTest("Prepared imported packages are not present")
        for folder in folders:
            with self.subTest(car=folder.name):
                updates, report = repair_reflections.prepare(folder)
                self.assertEqual(len(updates), 3)
                for path, before, after in updates[:2]:
                    self.assertEqual(len(before), len(after))
                    old = mm3_model.read_model(before)
                    new = mm3_model.read_model(after)
                    old_nodes = dict(old.root.walk())
                    new_nodes = dict(new.root.walk())
                    self.assertEqual(old_nodes.keys(), new_nodes.keys())
                    for node_path, original in old_nodes.items():
                        updated = new_nodes[node_path]
                        self.assertEqual(original.prefix, updated.prefix)
                        self.assertEqual(original.terminator, updated.terminator)
                        self.assertEqual(len(original.meshes), len(updated.meshes))
                        for a, b in zip(original.meshes, updated.meshes):
                            self.assertEqual(a.name, b.name)
                            if a.payload == b.payload:
                                continue
                            self.assertFalse(mm3_model.is_shadow_mesh(a))
                            self.assertTrue(a.name.rsplit("\\", 1)[-1].startswith("mm3import_"))
                            meta = mm3_model.mesh_metadata(a, old.version)
                            restored = bytearray(b.payload)
                            for vertex in range(meta["vertex_count"]):
                                offset = meta["vertex_offset"] + vertex * 24 + 20
                                restored[offset:offset + 4] = a.payload[offset:offset + 4]
                            self.assertEqual(bytes(restored), a.payload)
                    # Reapplying the same channels is byte-identical.
                    again, counts = repair_reflections.apply_shading(after, report["material_shading"])
                    self.assertEqual(again, after)
                    self.assertEqual(counts["changed_vertices"], 0)
                self.assertGreater(report["models"]["near"]["reflections"]["reflective_vertices"], 0)


if __name__ == "__main__":
    unittest.main()
