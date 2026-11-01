import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('builder_release', Path(__file__).resolve().parents[1] / 'scripts/builder_release.py')
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)


class BuilderReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.repo = Path(self.temp.name)
        self.toolkit = self.repo / 'tools/xboxrecomp'
        self.toolkit.mkdir(parents=True)
        for repo in (self.repo, self.toolkit):
            self.git(repo, 'init', '-q')
        self.write(self.toolkit, 'tools/xiso/xdvdfs.py', 'reader v1')
        self.write(self.toolkit, 'src/kernel.c', 'runtime v1')
        self.commit(self.toolkit)
        for name in release.SOURCES:
            self.write(self.repo, name, 'builder v1')
        self.write(self.repo, release.WORKFLOW,
                   "paths: [scripts/**]\n  python-version: '3.12'\n"
                   "  - run: python -m pip install pyinstaller==6.22.3\n"
                   "    run: python -m PyInstaller scripts/build_gui.py\n")
        self.pin()
        self.base = self.commit(self.repo)

    def git(self, repo, *args):
        return subprocess.check_output(['git', '-C', str(repo), *args], text=True, stderr=subprocess.PIPE).strip()

    def write(self, repo, name, text):
        p = repo / name
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(text)

    def commit(self, repo):
        self.git(repo, 'add', '.')
        self.git(repo, '-c', 'user.name=Test', '-c', 'user.email=test@example.invalid',
                 'commit', '-qm', 'fixture')
        return self.git(repo, 'rev-parse', 'HEAD')

    def pin(self):
        self.git(self.repo, 'update-index', '--add', '--cacheinfo',
                 '160000,' + self.git(self.toolkit, 'rev-parse', 'HEAD') + ',tools/xboxrecomp')

    def changed(self):
        return release.inputs(self.repo, self.base) != release.inputs(self.repo, 'HEAD')

    def test_runtime_and_unrelated_script_changes_skip(self):
        self.write(self.toolkit, 'src/kernel.c', 'runtime v2')
        self.write(self.toolkit, 'tools/xiso/test_xdvdfs.py', 'unbundled test')
        self.commit(self.toolkit)
        self.pin()
        self.write(self.repo, 'scripts/play.py', 'unrelated launcher')
        self.commit(self.repo)
        self.assertFalse(self.changed())

    def test_bundled_disc_reader_change_releases(self):
        self.write(self.toolkit, 'tools/xiso/xdvdfs.py', 'reader v2')
        self.commit(self.toolkit)
        self.pin()
        self.commit(self.repo)
        self.assertTrue(self.changed())

    def test_builder_source_change_releases(self):
        self.write(self.repo, 'scripts/build_gui.py', 'builder v2')
        self.commit(self.repo)
        self.assertTrue(self.changed())

    def test_trigger_only_change_skips_but_tool_version_releases(self):
        p = self.repo / release.WORKFLOW
        p.write_text(p.read_text().replace('scripts/**', 'scripts/build_gui.py'))
        self.commit(self.repo)
        self.assertFalse(self.changed())
        p.write_text(p.read_text().replace('6.22.3', '6.23.0'))
        self.commit(self.repo)
        self.assertTrue(self.changed())

    def test_unchanged_rerun_skips(self):
        self.assertFalse(self.changed())
