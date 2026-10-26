import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from scripts import setup


class GenerationReuseTest(unittest.TestCase):
    def test_only_current_complete_generation_is_reused(self):
        cases = (
            (None, True, False, True),
            ("old\n", True, False, True),
            ("current\n", False, False, True),
            ("current\n", True, True, True),
            ("current\n", True, False, False),
        )
        for marker, dispatch, force, regenerate in cases:
            with self.subTest(marker=marker, dispatch=dispatch, force=force):
                with tempfile.TemporaryDirectory() as folder:
                    root = Path(folder)
                    generated = root / "src/recomp/gen"
                    generated.mkdir(parents=True)
                    if dispatch:
                        (generated / "recomp_dispatch.c").touch()
                    if marker is not None:
                        (generated / "generation.sha256").write_text(marker)
                    with patch.object(setup, "REPO", root), \
                         patch.object(setup, "generation_key", return_value="current\n"), \
                         patch.object(setup, "build_env", return_value={}), \
                         patch.object(setup, "run") as run:
                        setup.generate(force, None)
                    self.assertEqual(run.call_count, int(regenerate))


if __name__ == "__main__":
    unittest.main()
