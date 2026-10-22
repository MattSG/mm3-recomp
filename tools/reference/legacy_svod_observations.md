# Historical SVOD corpus observations

Source: XboxRecomp e56c331877be1b526b9e5f77fcb5b92ff20aefb3 `tools/fusion/svod.py` and `test_svod.py`. These are captured debugging observations, not universal package validation or current compatibility evidence.

## Original module notes

Read the SVOD container in a Microsoft Xbox BC package and extract from it.

tools/fusion/coverage_oracle.py grades our function-boundary detection against
Microsoft's `PrecompiledSymbolTable`, which names *guest* functions. To use it
you need the guest binary those names point at -- the donor title's original
Xbox `default.xbe`. That XBE is not in the recompiled DLL; it is inside the
package's SVOD container, which is why the oracle has sat in the tree with no
way to feed it a second argument. This reads the container.

The container is not encrypted. It is a plain XDVDFS filesystem with SHA-1
hash blocks interleaved into it at a fixed stride, so a flat `dd` of the Data
files yields a file that *still parses* -- correct title, build date, base
address, section names, even the right XDK library version -- and is wrong
every 0x1000 bytes. See `xbe_gate()` for the checks that actually catch that.

White-room: the guest XBE is the user's own property, read out of a package
they own. Nothing here ships Microsoft data; this is a reader, and the
repository carries no container, no XBE and no test fixture derived from one.

    py -3 -m tools.fusion.svod ls      <package_dir> [inner_path]
    py -3 -m tools.fusion.svod extract <package_dir> <inner_path> <out>
    py -3 -m tools.fusion.svod verify  <package_dir>

Layout, measured on all four shipped BC titles (Fuzion Frenzy, Blinx, Crimson
Skies, Conker L&R) on 2026-09-19. A package is:

    Content/Game/DefaultPackage             0xB000 PIRS header
    Content/Game/DefaultPackage.data/Data0000..N

Every full Data file is 0xA290000 bytes = 41,616 blocks of 0x1000:

    block 0                             L1 hash block
    then 203 x [1 L0 hash block + 204 data blocks]

    1 + 203 * (1 + 204) == 41,616                      exactly, no slack
    203 * 204           == 41,412 logical blocks per full file

The hash blocks are checkable and that is the cheap proof the stride is right:
for a hash block at physical H, `sha1(data[H+0x1000:H+0x2000])` equals
`data[H:H+20]` -- each hash block's first entry covers the block after it.
`verify_hashes()` does this; it is the first thing to run against a new title.

Inside, `MICROSOFT*XBOX*MEDIA` sits at logical offset 0 (physical 0x2000 of
Data0000). The XDVDFS volume descriptor gives the root directory's sector at
+0x14 and its size at +0x18.

The sector base is per title, and this is the part that bites:

    Fuzion Frenzy    base      32    root dir sector        34
    Crimson Skies    base      32    root dir sector        34
    Blinx            base 880,036    root dir sector 1,693,790
    Conker L&R       base 956,202    root dir sector 1,711,833

    logical_byte = (sector - base) * 2048

Sector numbers in the filesystem are original-disc sectors; the SVOD stream is
a slice of that disc and `base` is where the slice starts. For the two titles
whose slice is the whole game partition it is the XDVDFS constant 32. For the
other two it is not, and assuming 32 silently reads the wrong data rather than
failing. `derive_base()` solves for it instead -- see there for the gate.


## derive_base notes

Find the sector base: the disc sector the logical stream starts at.

    `read(offset, length)` reads the logical stream. Each candidate base is
    tested by actually using it, because a wrong base does not fail loudly:

      1. the root directory table at (root_sector - base) * 2048 must parse;
      2. it must contain `default.xbe` -- every Xbox game partition has one;
      3. that entry's own sector, resolved with the same base, must start
         `XBEH`.

    Step 1 alone is not enough. Conker's directory tables are packed into
    consecutive sectors, so bases of 956,201 through 956,207 all parse into
    plausible-looking entry lists; only 956,202 has a root that also resolves
    default.xbe to an XBE header. Steps 2 and 3 are what make it unambiguous.
    

## header_base_hints notes

Candidate bases read out of the 0xB000 PIRS header, nearest first.

    The XContent SVOD volume descriptor at +0x379 carries a data-block offset
    at +0x1C. On all four titles `2 * <that value as little-endian u24>` lands
    on the true base or two sectors past it:

        Fuzion / Crimson   feature 0x00   value      16   base      32
        Blinx              feature 0x40   value 440,019   base 880,036
        Conker             feature 0x40   value 478,102   base 956,202

    Four samples is not enough to claim the off-by-one on the enhanced-layout
    titles is anything but a unit convention we have not pinned down, so these
    are hints that shorten the search, never an answer. `derive_base()` still
    proves whichever one it takes.
    

## xbe_gate notes

Sanity checks that catch a *structurally plausible* bad extraction.

    A flat contiguous copy of the Data files -- hash blocks and all -- still
    reports the right title, build date, base address, section names and XDK
    library version, because all of that lives in the first 0x1000 bytes and
    the first data block is genuine. Three extractions passed a header check
    on 2026-09-19 and were wrong. These are the tells that fired:

      thunks   the kernel import table is a run of 0x8000xxxx ordinals ending
               in 0, and it must actually be at the VA the header declares
               (+0x158, XOR'd with the retail or debug key). Corruption
               displaces it and the run is not there.
      tls      the TLS directory's zero-fill size is a few bytes; a corrupt
               read gives ~1.9 billion, or reads 0x20202020 out of a text
               block that has landed where the directory should be.

    Returns {check: (bool, detail)}. `thunks` is the strong one -- it is the
    check that failed on every bad extraction and passed on every good one.
    

## Original test_parses_a_handcrafted_table

```python
def test_parses_a_handcrafted_table():
    tab = _table([("default.xbe", 462518, 3219456, 0x21),
                  ("media", 2027275, 104, 0x10),
                  ("coverart.jpg", 428987, 91590, 0x20)])
    by_name = {e.name: e for e in parse_directory_table(tab)}
    assert set(by_name) == {"default.xbe", "media", "coverart.jpg"}
    assert by_name["default.xbe"].sector == 462518
    assert by_name["default.xbe"].size == 3219456
    assert by_name["media"].is_dir
    # attr is not a file/dir discriminator beyond bit 4: the shipped titles use
    # 0x21, 0x20 and 0x80 for plain files. Only 0x10 means directory.
    assert not by_name["default.xbe"].is_dir and not by_name["coverart.jpg"].is_dir
```

## Original test_gate_fails_a_wild_tls_zero_fill

```python
def test_gate_fails_a_wild_tls_zero_fill():
    # the observed corrupt value was ~1.9 billion
    g = xbe_gate(_xbe([0x80000000 | n for n in range(1, 40)] + [0],
                      tls_zerofill=1_986_098_990))
    assert g["thunks"][0] and not g["tls"][0]
```
