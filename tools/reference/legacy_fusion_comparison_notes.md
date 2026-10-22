# Historical Fusion comparison notes

Source: XboxRecomp 77dea704ca346a6651672323bf8c151a90366166. Captured title examples and earlier analysis claims are preserved here as historical context, not current correctness or compatibility evidence.

## tools/fusion/signatures.py original module notes

XDK library signature database from Fission symbol tables.

Idea: every Xbox game statically links the same XDK libraries (D3D8, D3DX,
DirectSound, XAPI, XGRAPHICS, XNet, CRT). Microsoft's Fission symbol tables name
those functions with exact (guest_start, size). Combined with the game's own XBE
(which we have -- it is our legitimate input), that yields a byte signature per
named function that transfers to *any* XBE: a FLIRT-style name-recovery database
sourced from Microsoft's own symbols.

White-room: the signature bytes come from the GUEST xbe (our input), not from
Microsoft's recompiled output. The Fission table only supplies (name, address,
size) -- facts about the guest binary.

Signatures are masked with capstone so that operands that legitimately differ
between two games (absolute addresses of globals/callees, branch targets) are
wildcards; opcodes and structure are matched. A match requires equal function
length plus agreement on all non-wildcard bytes of the first 32 -- strong enough
that cross-title hits are almost entirely the identical XDK library code.

Usage:
    py -3 -m tools.fusion.signatures build <module.dll> <source.xbe> <out.json>
    py -3 -m tools.fusion.signatures match <sigs.json> <target.xbe> <functions.json> <names.json>
    py -3 -m tools.fusion.signatures selftest <module.dll> <source.xbe> [<funcs.json>]


## tools/fusion/lifter_diff.py original module notes

Lifter validation against Microsoft's recompile (idea 4).

Both Microsoft (Ficl/Fission) and this project lift the *same* guest x86-32
function. Microsoft's lift ships and runs the games, so it is a trusted oracle:
where our recompiled C diverges from what Microsoft's host code does, we very
likely have a lifter bug -- in exactly the classes we keep fixing (flags, x87,
SEH, stack).

Rather than run Ghidra's decompiler over Microsoft's flat jmp-connected arena
(messy, no clean function boundaries), we disassemble their host x86-64 with
capstone and rewrite it back into *guest terms* using their documented register
model (docs/technical/ms-fusion-recompiler.md §3):

    guest eax..ebp   = host eax..ebp        (same names)
    guest esp        = host r14d
    guest RAM base   = host r15
    [greg + r15 + d] = guest memory [greg + d]
    [r15 + d]        = guest global [d]

That reads directly as guest semantics, comparable to our C. The automated
signal is the set of guest memory field offsets each side touches and the set
of guest addresses each calls; a mismatch is a concrete divergence to inspect.

White-room: Microsoft's host bytes are read for analysis only -- nothing is
copied into the toolkit. The output is a diff describing *our* correctness.

    py -3 -m tools.fusion.lifter_diff ms   <module.dll> <name_or_0xaddr>
    py -3 -m tools.fusion.lifter_diff diff <module.dll> <gen_dir> <name_or_0xaddr>


## Captured donor-build example

Side-by-side: MS's lift (guest terms) and ours, for the same guest addr.

    NOTE: this is a *reference aid for manual reading*, not an automated oracle.
    A clean automated diff needs both sides to lift the identical guest bytes,
    which in turn needs Microsoft's exact source XBE. The BC package's XBE is a
    (subtly) different build from a retail-disc XBE -- e.g. Crimson's guest
    0x1C448A is a thunk in the disc build and an inlined body in Microsoft's --
    so an offset mismatch here can be a build difference, not a lifter bug. Read
    the two listings; do not trust a mechanical set-diff.
    

## tools/fusion/coverage_oracle.py original module notes

Coverage oracle: grade our function-boundary detection against Microsoft's.

Microsoft's Fission PrecompiledSymbolTable gives exact (guest_start, size) for
the library functions in a title -- ground-truth function boundaries. For a
title we also recompile (Crimson Skies), compare our tools/disasm functions.json
against those starts:

  hit      MS names a function start and we detect a function there
  missed   MS names a start we don't detect at all (a real function boundary we
           lack) -- split by whether it lands INSIDE a function we did detect
           (a merge/under-segmentation error) or in no detected function (a gap)

The missed-inside set is the actionable one: those are the tail-jump /
mid-function-entry cases our detector still merges. Run it after detector changes
to measure movement against ground truth.

    py -3 -m tools.fusion.coverage_oracle <module.dll> <our_functions.json>

`our_functions.json` is our detection for the *donor* title, so it comes from
running the pipeline over that title's original Xbox default.xbe. Get one out
of the package with tools/fusion/svod.py -- the SVOD container is not sealed:

    py -3 -m tools.fusion.svod extract "<package>" default.xbe donor.xbe
    py -3 -m tools.disasm donor.xbe --text-only


## tools/fusion/__init__.py original module notes

White-room readers for Microsoft's shipped Xbox BC (Ficl/Fission) modules.

These parse *publicly distributed retail binaries* (the recompiled `xefu_*`/`xeo3_*`
DLLs in a BC package) to extract facts useful to this project: a per-title symbol
table, the guest address map, and the source/title identity. No Microsoft code, IR,
or output is copied into or emitted by this toolkit -- see docs/technical/ms-fusion-*.md.


## Original guest-global test observation

```python
def test_guest_global_access():
    # mov ebp, [r15 + 0x32dd8c]  -> guest global, offset 0x32dd8c
    txt, off = _one(b"\x41\x8B\xAF\x8C\xDD\x32\x00")
    assert off == 0x32dd8c and "guest global" in txt
```

## Original donor address example

```python
    # a guest RVA in the symbol table is a VA when added to base; the map/table
    # use RVAs relative to 0, but names sit at file VA = base + rva. The XBE
    # loads at base 0x10000 and the symtab guest_start values already include it
    # (e.g. 0x1c440b), so read directly.
```
