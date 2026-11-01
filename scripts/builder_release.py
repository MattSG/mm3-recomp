"""Compare the packaged builder's sources, disc reader and packaging recipe."""
import re
import subprocess
import sys
from pathlib import Path

SOURCES = ('scripts/build_gui.py', 'scripts/extract_disc.py', 'scripts/setup.py',
           'scripts/devenv.py', 'scripts/generate_mm3.py')
WORKFLOW = '.github/workflows/release-builder.yml'


def recipe(workflow):
    # Trigger/gating edits do not change the executable. Tool versions and the
    # PyInstaller command do; include their literal recipe in the comparison.
    return tuple(line.strip() for line in workflow.splitlines() if re.match(
        r'\s*(python-version:|- run: python -m pip install pyinstaller|run: python -m PyInstaller)', line))


def inputs(repo, ref):
    def git(*args, cwd=repo):
        return subprocess.check_output(['git', '-C', str(cwd), *args], text=True).strip()
    toolkit = repo / 'tools/xboxrecomp'
    revision = git('rev-parse', ref + ':tools/xboxrecomp')
    if subprocess.run(['git', '-C', str(toolkit), 'cat-file', '-e', revision],
                      stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode:
        subprocess.run(['git', '-C', str(toolkit), 'fetch', 'origin', revision], check=True)
    return (git('ls-tree', '-r', ref, '--', *SOURCES),
            git('ls-tree', '-r', revision, '--', 'tools/__init__.py',
                'tools/xiso/__init__.py', 'tools/xiso/__main__.py',
                'tools/xiso/xdvdfs.py', cwd=toolkit),
            recipe(git('show', ref + ':' + WORKFLOW)))


if __name__ == '__main__':
    repo = Path(__file__).resolve().parents[1]
    print(str(inputs(repo, sys.argv[1]) != inputs(repo, 'HEAD')).lower())
