"""C compiler selection shared by the contract tests.

The implementation lives with the runtime in tools/xboxrecomp/tests/cc.py so
the fork's own contract tests share it. This module re-exports it.
"""
import importlib.util
from pathlib import Path

_path = Path(__file__).resolve().parents[1] / "tools/xboxrecomp/tests/cc.py"
_spec = importlib.util.spec_from_file_location("xboxrecomp_cc", _path)
_module = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_module)
globals().update({k: v for k, v in vars(_module).items() if not k.startswith("__")})
