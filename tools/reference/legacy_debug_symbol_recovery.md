# Historical debug-symbol recovery observations

From fork 1ca3db4. Captured title paths, counts and correctness claims below are
historical, not newly reproduced evidence. The shared recovery mechanism remains.

## Previous guide

# Debug Symbol Recovery (`tools.debug_symbols`)

Recovers **which original source file each function was compiled from** by mining
the `__FILE__` strings that a debug build's assert macros leave in the binary.

No disassembler, no PDB, no external database — it reads the disassembler's own
`functions.json` + `strings.json` and writes meaningful names back.

## Why this works

Nearly every game ships an assert macro shaped like:

```c
#define assert(x)  if (!(x)) _assert_failed(#x, __FILE__, __LINE__)
```

In a **debug build** those calls survive, so the binary contains one string
literal per source file that has an assert in it, and each literal is referenced
from the code of the function containing the assert. Halo's beta build carries
295 of them:

```
c:\halo\SOURCE\ai\actor_combat.c
c:\halo\SOURCE\rasterizer\xbox\rasterizer_xbox_hardware_geometry.c
c:\halo\SOURCE\physics\collision_bsp.c
```

Cross-referencing those strings gives function -> source file, for free.

Retail builds usually compile asserts out, so this tool does nothing useful on
them. It is worth hunting for a debug/beta XBE of your target before starting:
one exists for far more Xbox titles than people expect, and it is worth more than
any amount of manual reversing.

## Two passes

**direct** — a function references a `__FILE__` string, so it came from that
file. If a function references more than one (inlining drags a foreign assert
into a caller), majority vote decides; `.c` beats `.h` on a tie.

**interpolated** — the linker emits each object file's functions contiguously.
An unattributed run of functions bracketed by two functions that agree on the
same file is almost certainly from that file too. Only fills when **both**
endpoints agree, and **never across a section boundary** — different sections are
different link units, where address adjacency means nothing.

Direct evidence always overrides interpolation.

## Usage

```bash
py -3 -m tools.debug_symbols \
    --functions build/disasm/functions.json \
    --strings   build/disasm/strings.json \
    -o build/disasm/debug_symbols.json -v
```

Inspect the result, then merge it into `functions.json` so the recompiler picks
it up:

```bash
py -3 -m tools.debug_symbols --functions build/disasm/functions.json \
                             --strings   build/disasm/strings.json --apply -v
```

`--apply` keeps a one-time `functions.json.bak`, and **only renames functions
still called `sub_XXXXXXXX`** — a name already recovered by `tools.ghidra_naming`,
`tools.func_id`, or a human is never clobbered. Every attributed function also
gets `source_file` and `source_file_origin` fields regardless.

### Options

| Flag | Effect |
|------|--------|
| `--apply` | Merge names into `functions.json` in place (keeps `.bak`) |
| `--include-headers` | Also attribute from `.h`/`.hpp` (asserts in inline functions) |
| `--no-interpolate` | Direct string references only; skip the contiguity fill |

## Output

Functions are renamed `<module>__sub_<ADDR>`, e.g. `actor_combat__sub_0004A120`.
The address is kept so names stay unique and traceable back to the binary, while
the module prefix makes the generated C greppable by subsystem.

```json
{
  "summary": { "attributed_total": 4331, "coverage_percent": 49.77, ... },
  "names":   { "0x0004A120": "actor_combat__sub_0004A120" },
  "attribution": {
    "0x0004A120": { "source_file": "ai/actor_combat.c", "origin": "direct" }
  }
}
```

## Result on Halo (build 2276 `cachebeta.xbe`)

```
Source-path strings found:   295
Functions attributed direct: 2067
...of which ambiguous:       0
Functions interpolated:      2264
Attributed 4331/8702 functions (49.77%) across 295 source files
```

Coverage is concentrated exactly where it should be:

| Section | Attributed | Why |
|---------|-----------|-----|
| `.text` | 4331/7222 (60.0%) | Bungie's own code — has the asserts |
| `D3D`, `DSOUND`, `XNET`, `XPP`, `BINK`, `D3DX` | 0 | Microsoft/RAD libraries, no `c:\halo\SOURCE` asserts |

That 0% across every library section is the correctness check: those functions
are interleaved by address with attributed ones, and the section guard is what
stops interpolation from confidently mislabelling all of them.

## Testing

```bash
py -3 tools/debug_symbols/test_recover.py
```

Covers path extraction (including RCS `$Header:` banners, which look like source
paths but are not `__FILE__` — libtiff alone ships hundreds), the common-root
strip, function ownership lookup, interpolation across section boundaries, and
the guarantee that `--apply` preserves existing names.


## Previous source rationale and examples

```python
from collections import Counter, defaultdict

# A __FILE__ expansion: an optional drive, then at least one directory
# component, then the filename. Anchored at the end of the string so RCS
# keyword junk ("$Header: .../tif_aux.c,v 1.8 92/03/27 ...") does not match --
# that trailing ",v 1.8" is exactly what distinguishes a version-control
# banner from a real __FILE__.
_SOURCE_RE = re.compile(
    r"((?:[A-Za-z]:)?[\\/]?(?:[\w .+-]+[\\/])+[\w .+-]+\.(?:%s))\s*$"
)
_CODE_EXTS = "c|cc|cpp|cxx"
_HEADER_EXTS = "h|hh|hpp|hxx"

# The scanner sometimes glues a preceding string's tail onto the front of a
# literal ("-2>c:\halo\SOURCE\camera\observer.c"). Matching unanchored at the
# front handles that; these are the characters a real path never starts with.
_LEAD_JUNK = re.compile(r"^[^A-Za-z0-9_.\\/]+")


def _compile_pattern(include_headers):
    exts = _CODE_EXTS + ("|" + _HEADER_EXTS if include_headers else "")
    return re.compile(_SOURCE_RE.pattern % exts)


def _extract_path(text, pattern):
    """Return the normalized source path inside `text`, or None."""
    m = pattern.search(text)
    if not m:
        return None
    path = _LEAD_JUNK.sub("", m.group(1)).replace("\\", "/")
    return path or None


def _strip_common_root(paths):
    """Drop the shared build-machine prefix (e.g. 'c:/halo/SOURCE/')."""
    if not paths:
        return {}
    dirs = [p.rsplit("/", 1)[0] + "/" for p in paths]
    root = os.path.commonprefix([d.lower() for d in dirs])
    root = root[: root.rfind("/") + 1] if "/" in root else ""

    # When every path lives in one directory the common prefix swallows it
    # whole, leaving bare filenames and losing the subsystem the file sat in.
    # Back off one component so the immediate parent survives.
    if root and all("/" not in p[len(root):] for p in paths):
        root = root[: root.rstrip("/").rfind("/") + 1]

    n = len(root)
    return {p: (p[n:] if p.lower().startswith(root) else p) for p in paths}


def _module_name(rel_path):
    """'ai/actor_combat.c' -> 'actor_combat', as a valid C identifier.

    Source filenames are not required to be identifier-safe: Halo alone ships
    bungie_net/common/64bit_math.c, and a name starting with a digit does not
    compile.
    """
    stem = os.path.splitext(os.path.basename(rel_path))[0]
    stem = re.sub(r"[^A-Za-z0-9_]", "_", stem)
    if not stem or stem[0].isdigit():
        stem = "_" + stem
    return stem


class _FunctionIndex:
    """Address -> containing function, by binary search over sorted ranges."""

    def __init__(self, functions):
        self.funcs = sorted(functions, key=lambda f: int(f["start"], 16))
        self.starts = [int(f["start"], 16) for f in self.funcs]
        self.ends = [int(f["end"], 16) for f in self.funcs]

    def owner(self, addr):
```

## Previous test path examples

```python
"""
Self-check for debug-build symbol recovery.

Run: py -3 tools/debug_symbols/test_recover.py
"""

import json
import os
import sys
import tempfile

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))

from tools.debug_symbols.recover import (  # noqa: E402
    _compile_pattern,
    _extract_path,
    _module_name,
    _strip_common_root,
    _FunctionIndex,
    run,
)

CODE = _compile_pattern(include_headers=False)
WITH_H = _compile_pattern(include_headers=True)


def test_extract_path():
    # Plain __FILE__ expansion.
    assert _extract_path(r"c:\halo\SOURCE\ai\actors.c", CODE) == "c:/halo/SOURCE/ai/actors.c"
    # The string scanner glues a previous string's tail onto the front.
    assert _extract_path(r"-2>c:\halo\SOURCE\camera\observer.c", CODE) == "c:/halo/SOURCE/camera/observer.c"
    assert _extract_path(r"@@c:\halo\SOURCE\camera\dead_camera.c", CODE) == "c:/halo/SOURCE/camera/dead_camera.c"
    # Unix-style path, no drive letter.
    assert _extract_path("/src/engine/render.cpp", CODE) == "/src/engine/render.cpp"

    # RCS keyword banners must NOT match -- they are not __FILE__, and libtiff
    # ships hundreds of them. The trailing ",v 1.8 ..." is the tell.
    rcs = "$Header: /usr/people/sam/tiff/libtiff/RCS/tif_aux.c,v 1.8 92/03/27 14:53:02 sam Exp $"
    assert _extract_path(rcs, CODE) is None

    # A bare filename with no directory is too weak a signal to trust.
    assert _extract_path("actors.c", CODE) is None
    # Headers only when asked for.
    assert _extract_path(r"..\math\real_math.h", CODE) is None
    assert _extract_path(r"..\math\real_math.h", WITH_H) == "../math/real_math.h"
    print("ok  extract_path")


def test_strip_common_root():
    rel = _strip_common_root([
        "c:/halo/SOURCE/ai/actors.c",
        "c:/halo/SOURCE/physics/physics.c",
    ])
    assert rel["c:/halo/SOURCE/ai/actors.c"] == "ai/actors.c"
    assert rel["c:/halo/SOURCE/physics/physics.c"] == "physics/physics.c"
    # One path alone gives the prefix scan nothing to compare against, so it
    # keeps the immediate parent directory rather than stripping to a bare name.
    solo = _strip_common_root(["c:/halo/SOURCE/ai/actors.c"])
    assert solo["c:/halo/SOURCE/ai/actors.c"] == "ai/actors.c"
    # Same when many files share a single directory.
    same_dir = _strip_common_root(["c:/g/SOURCE/ai/a.c", "c:/g/SOURCE/ai/b.c"])
    assert same_dir["c:/g/SOURCE/ai/a.c"] == "ai/a.c"
    print("ok  strip_common_root")


def test_module_name_is_a_valid_c_identifier():
    assert _module_name("ai/actor_combat.c") == "actor_combat"
    # Halo ships bungie_net/common/64bit_math.c -- a leading digit does not
    # compile, and the generated header is where it shows up.
    assert _module_name("bungie_net/common/64bit_math.c") == "_64bit_math"
    # Anything else non-identifier gets folded to underscores.
    assert _module_name("saved games/game-state.c") == "game_state"
    assert _module_name("a/b c+d.cpp") == "b_c_d"
    print("ok  module_name_is_a_valid_c_identifier")


def _fn(start, end, section=".text"):
    return {"start": f"0x{start:08X}", "end": f"0x{end:08X}",
            "name": f"sub_{start:08X}", "section": section}


def test_function_index():
    idx = _FunctionIndex([_fn(0x1000, 0x1100), _fn(0x1200, 0x1300)])
    assert idx.owner(0x1000)["start"] == "0x00001000"
```
