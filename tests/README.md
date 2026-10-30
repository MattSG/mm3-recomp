# Tests

Each `mm3_*_regression.py` compiles a C contract against the real sources (and, for the
seam tests, the generated code in `src/recomp/gen`) and runs it. Run one with
`python tests/<name>.py`, or all of them from a shell where the compiler is available.

Needs:

- **Windows:** a Visual Studio developer shell (`vcvars64.bat`), so `cl` is on the path, and the
  Visual Studio CMake ahead of any MSYS/devkitPro `cmake` (an MSYS CMake reads `C:/...` as a relative
  path and the seam tests fail with "file failed to open for reading").
- **Python packages:** `unicorn` and `capstone` (the CPU, ABI and tree regressions emulate x86).
  A throwaway environment is enough: `python -m venv .venv-tests && .venv-tests/Scripts/pip install unicorn capstone`.
- **Generated code:** `python scripts/setup.py --iso ...` must have produced `src/recomp/gen` first.

The builder's own tests (`tests/test_build_gui.py`) need nothing else and are the only tests CI runs.
`tools/game_test/` holds the runtime harness (needs the built game and extracted assets).
