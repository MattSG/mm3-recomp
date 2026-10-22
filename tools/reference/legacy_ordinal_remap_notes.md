# Historical ordinal-remap observations

Source: XboxRecomp `364d9d19307cb8520c559c1a0ea7636e0f3e93bf`. Captured title/XDK claims and the old
bridge-only mapping state follow as normalized reference text. They are not
current generic ABI guarantees. The runtime now shares one explicit mapping
operation; import names and any application mapping remain port-owned policy.

## src/kernel/kernel_bridge.c

```text
/* Per-title ordinal remap; NULL = identity (the kernel's own XDK). Set by
 * xbox_kernel_set_ordinal_remap before init. See kernel.h. */
static const unsigned short *g_ordinal_remap = NULL;
static int g_ordinal_remap_count = 0;

void xbox_kernel_set_ordinal_remap(const unsigned short *map, int count)
{
    g_ordinal_remap = map;
    g_ordinal_remap_count = count;
}
```

## src/kernel/kernel.h

```text
/**
 * Per-title kernel ordinal remap.
 *
 * The bridge routes ordinals with one hardcoded table -- the ordinal ABI of the
 * XDK it was written against (Halo's 3911 / Crimson's 5659). A title built with
 * a different XDK numbers the same kernel functions differently: Burnout 3's
 * XDK 5849 has HalRequestSoftwareInterrupt at 49 where the older XDKs have
 * HalReturnToFirmware. Without this, swapping such a title onto the shared kernel
 * misroutes it (Burnout 3 exited via HalReturnToFirmware during engine setup).
 *
 * `map[title_ordinal] = canonical_ordinal` translates a title's ordinals into
 * the kernel's canonical space before every routing decision. An entry of 0
 * (or a title_ordinal past `count`) means identity -- no title needs a real
 * ordinal 0. Call BEFORE xbox_kernel_bridge_init. A title whose XDK already
 * matches the kernel calls nothing and gets identity, so existing titles are
 * unaffected. Generate the map with tools/kernel_audit/gen_ordinal_remap.py.
 */
```

## tools/kernel_audit/gen_ordinal_remap.py

```text
"""
Generate a per-title kernel ordinal remap for xbox_kernel_set_ordinal_remap.

    py -3 -m tools.kernel_audit.gen_ordinal_remap <analysis.json> --out remap.h

The kernel bridge routes ordinals with one hardcoded table -- the ordinal ABI of
the XDK it was written against. A title built with a different XDK numbers the
same functions differently (Burnout 3's XDK 5849 has HalRequestSoftwareInterrupt
at 49 where the older XDKs have HalReturnToFirmware). The kernel functions are
the same; only the ordinal that names each differs.

This maps a title's ordinals into the kernel's canonical space by NAME: for each
kernel function the title imports, look up which ordinal the kernel's own table
gives that function, and emit `map[title_ordinal] = canonical_ordinal` where they
differ. Names the title's parser could not resolve, or that the kernel's table
does not have, are left identity and reported -- a title that never calls them
boots regardless, and one that does surfaces a real gap rather than a silent
misroute.

The output is a C header a title includes and hands to
xbox_kernel_set_ordinal_remap before xbox_kernel_bridge_init.
"""
```

## tools/kernel_audit/test_gen_ordinal_remap.py

```text
"""
Self-check for the per-title kernel ordinal remap generator.

Run: py -3 tools/kernel_audit/test_gen_ordinal_remap.py

The remap translates a title's XDK ordinals into the kernel's canonical ordinal
space by function name. The properties that matter:

  - a title whose XDK matches the kernel needs NO remap (all identity), and must
    not be told to install one it does not need;
  - a title on a different XDK remaps only the ordinals that differ, and by
    NAME -- the same function at a different number;
  - a name the title imports that the kernel's table does not have, or that the
    title's parser could not resolve, is left identity and reported, never
    silently pointed at whatever sits at that ordinal.

Measured against the real analyses: Halo (XDK 3911) and Crimson (5659) match the
kernel and remap nothing; Burnout 3 (5849) remaps 47 and leaves 3 unresolved.
"""
```
