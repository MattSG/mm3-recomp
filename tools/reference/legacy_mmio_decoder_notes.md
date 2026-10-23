# Historical MMIO decoder extraction notes

Original shared-decoder migration narrative. This records implementation
history, not current caller coverage or instruction-semantic proof.

```text
 * That decoder existed twice before this file -- once in nv2a_mmio_hook.c and
 * once in apu_mmio_hook.c -- as the same opcode table written out against two
 * different pairs of accessors. This is the same logic with the accessors
 * passed in, so a third device does not need a third copy. The two originals
 * still carry their own and can move onto this whenever they are next touched.
 *
```

## Previous NV2A extraction notes and fallback claims

```text
/* Global APU state pointer is declared in apu.h and referenced from main.c
 * (regardless of whether the MMIO hook is active). Keep its definition
 * outside the Win32 guard. */

 * Returns false if the instruction couldn't be decoded (caller
 * should fall back to allocating a zero page).
 * For 0xF0000000-0xFCFFFFFF range. Currently just allocates
 * pages backed by NV2A VRAM where appropriate.
```
