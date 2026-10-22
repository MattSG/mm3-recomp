# Historical application context initialization

Source: XboxRecomp `1f528671a08809c653bfea4e2a9be2c9b5d5fc0c`. The inherited RenderWare scratch-chain
policy and captured code/TLS observations follow. These are not generic loader
contracts. MM3 startup preserves the two scratch-chain writes while their
application usage remains unaudited; this is compatibility, not a claim that
they implement MM3 native TLS. Relative documentation paths are historical.

## Original `src/kernel/xbox_memory_layout.c` block

```text
        /*
         * fs:[0x28] - Thread local storage / RW engine context.
         * The RW engine reads [fs:[0x28] + 0x28] to get a pointer
         * to its data area. We allocate a fake structure at 0x00760000
         * (in the BSS area) and a data buffer at 0x00700000.
         */
        #define FAKE_TLS_VA     0x00760000  /* Fake TLS structure (in BSS) */
        #define FAKE_RWDATA_VA  0x00700000  /* RW engine data area (in BSS) */

        MEM32_INIT(XBOX_FS_BASE + 0x28, FAKE_TLS_VA);
        /* TLS[0x28] = pointer to RW data area */
        MEM32_INIT(FAKE_TLS_VA + 0x28, FAKE_RWDATA_VA);
```

## Original `src/kernel/xbox_memory_layout.c` block

```text
        fprintf(stderr, "  TIB: fake TIB at VA 0x%X, TLS at 0x%08X, RW data at 0x%08X\n",
                XBOX_FS_BASE, FAKE_TLS_VA, FAKE_RWDATA_VA);

        #undef FAKE_TLS_VA
        #undef FAKE_RWDATA_VA
```

## Original `src/kernel/xbox_memory_layout.c` block

```text
/* Bounds of the title's executable sections, from its own XBE section table.
 *
 * RECOMP_ICALL uses these to decide whether an indirect-call target is code
 * before dispatching it. This used to be a hardcoded "0x00400000..0xFE000000 is
 * not code" test, which is true for Burnout 3 -- its .text ends at 0x002CC200,
 * so everything above 0x400000 really is data -- and false for any title with
 * more code than that. Half-Life 2's .text runs to 0x005F4A6C, so the constant
 * silently discarded every indirect call into the top two thirds of the game,
 * including the one that enters its main. No log, no crash: eax = 0 and carry
 * on, which looks exactly like a function that returned early.
 *
 * Zero until the layout is initialised, which the macro treats as "allow" so
 * nothing breaks before the title is loaded. */
```

## Original `src/kernel/xbox_memory_layout.c` block

```text
         * -(blocksize/4). Wreckless does this at guest 0x000EB57E and arrives
         * at -5 for its 20-byte block, so [fs:[4] + index*4] is the block's
         * first dword. The rounding below mirrors that arithmetic exactly,
         * because fs:[4] has to land where the title's own index says it is.
```

## Original `docs/technical/memory-layout.md` block

```text
### Fake Thread Information Block

The recompiler drops the `fs:` segment prefix from memory accesses like `mov eax, fs:[0x28]`. These become `MEM32(0x28)`, reading from low memory. A fake TIB is populated at address 0x0:

```c
MEM32(0x00) = 0xFFFFFFFF;      // SEH: end of chain
MEM32(0x04) = XBOX_STACK_TOP;  // Stack base (high address)
MEM32(0x08) = XBOX_STACK_BASE; // Stack limit (low address)
MEM32(0x18) = 0x00000000;      // Self pointer
MEM32(0x20) = 0x00000000;      // KPCR Prcb pointer
MEM32(0x28) = FAKE_TLS_VA;     // TLS / RW engine context
```

The RenderWare engine reads `[fs:[0x28] + 0x28]` to find its per-thread data area. A fake structure chain is set up in BSS memory.
```

## Original initialization commentary

```text
    /*
     * Populate the fake Thread Information Block (TIB) at Xbox VA 0x0.
     *
     * The original Xbox code uses fs:[offset] to read per-thread data,
     * but the recompiler drops the fs: segment prefix and generates
     * MEM32(offset) instead. Since we mapped low memory (0x0-0xFFFF),
     * we populate the TIB fields that game code accesses:
     *
     *   fs:[0x00] = SEH exception list (-1 = end of chain)
     *   fs:[0x04] = stack base (top of stack)
     *   fs:[0x08] = stack limit (bottom of stack)
     *   fs:[0x18] = self pointer (TIB address)
     *   fs:[0x20] = KPCR Prcb pointer (→ fake structure)
     *   fs:[0x28] = TLS / RW engine context pointer
     *
     * We use free space in the BSS area for the fake structures.
     */
```
