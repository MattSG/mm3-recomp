"""C compiler selection shared by the contract tests.

The implementation lives with the runtime in tools/xboxrecomp/tests/cc.py so
the fork's own contract tests share it. This module re-exports it.
"""
import importlib.util
import os
from pathlib import Path

_path = Path(__file__).resolve().parents[1] / "tools/xboxrecomp/tests/cc.py"
_spec = importlib.util.spec_from_file_location("xboxrecomp_cc", _path)
_module = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_module)
globals().update({k: v for k, v in vars(_module).items() if not k.startswith("__")})


def cmake_program():
    """Return a CMake that accepts Windows drive paths in -D and script args.

    MSYS2's cmake rewrites drive-letter paths relative to its working
    directory, so skip it. CMAKE in the environment overrides the search.
    """
    override = os.environ.get("CMAKE")
    if override:
        return override
    name = "cmake.exe" if os.name == "nt" else "cmake"
    for entry in os.environ.get("PATH", "").split(os.pathsep):
        candidate = Path(entry) / name
        if candidate.is_file() and "msys" not in str(candidate).lower():
            return str(candidate)
    return "cmake"
