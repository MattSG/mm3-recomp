# Historical memory header observations

Source: XboxRecomp `d6fa49695f29a68ab40a2e49dd7b64cfc19ae643`, `src/kernel/xbox_memory_layout.h`.
Captured title measurements, old bootstrap assumptions and superseded API
comments follow unchanged as normalized text. They are not current guarantees.
The shared declarations and numeric layout defaults are unchanged by this
comment migration; their broader ownership/correctness review remains separate.

```text
/**
 * Xbox Memory Layout Compatibility
 *
 * The Xbox has 64MB of unified memory shared between CPU and GPU.
 * Memory is identity-mapped (physical == virtual for most of it).
 * Game code and data are linked to specific address ranges which vary
 * per game. Section addresses are parsed dynamically from the XBE header
 * at runtime, so this module works with ANY Xbox game.
 *
 * On Windows, we:
 * 1. Create a 64MB file mapping (CreateFileMapping)
 * 2. Map the base view; reserve, but do not map, the space above it
 * 3. Parse the XBE section table and copy sections to their Xbox VAs
 * 4. Set up simulated stack, heap, TIB, and kernel data area
 *
 * An address above the mapped range is not an alias of RAM. It is unmapped,
 * so a stray guest access faults and the fault handler reports the address.
 */
```

```text
/* Xbox physical memory. 64 MB is the retail default; debug/beta builds ship for
 * 128 MB devkits and allocate accordingly (Halo's cachebeta pre-allocates ~57 MB
 * plus its debug arrays, which only fits on a devkit). Runtime-overridable via
 * xbox_SetTotalRam() before xbox_MemoryLayoutInit(); see g_xbox_total_ram. */
```

```text
/* Actual mapped RAM for this run. Defaults to XBOX_TOTAL_RAM; a title with a
 * devkit build calls xbox_SetTotalRam(XBOX_DEVKIT_RAM) before init. Heap top
 * derives from this, not from the compile-time constant. */
```

```text
/* How much guest address space to map, when that must exceed RAM.
 *
 * These are not the same quantity and conflating them is a bug. RAM is what
 * the console has and what the heap is carved out of; the mapped range is how
 * much guest address space is backed by distinct host pages. Anything a title
 * allocates above the mapped range is not backed by anything: it faults, which
 * is what an unmapped virtual address does on the console too.
 *
 * A port can request a larger backed virtual range when guest reservations
 * exceed installed RAM. These pages must remain distinct rather than aliasing
 * live RAM allocations. The port supplies the required range before init.
 *
 * Raising g_xbox_total_ram instead does not work: the heap top and anything
 * the guest is told about memory derive from that, so the title sizes itself
 * differently and faults during CRT init. Growing only the mapping leaves both
 * alone.
 *
 * Zero means "same as RAM", which is the existing behaviour for every title
 * that does not ask. Set before xbox_MemoryLayoutInit(). */
```

```text
/* NOTE: Section addresses (.text, .rdata, .data, etc.) are NOT hardcoded.
 * They are parsed from the XBE header at runtime in xbox_MemoryLayoutInit().
 * This allows the toolkit to work with ANY Xbox game without modification. */
```

```text
/**
 * Initialize the Xbox memory layout.
 *
 * Reserves the virtual address range 0x00010000 through 0x0076F000
 * and maps the XBE sections to their expected addresses:
 * - .rdata: copied from XBE, read-only
 * - .data: initialized portion copied from XBE, BSS zeroed
 *
 * Note: .text is NOT mapped here - the recompiled code is native
 * Windows code and doesn't need to be at the original address.
 * The data sections DO need to be at their original addresses
 * because the recompiled code references globals by absolute address.
 *
 * @param xbe_data  Pointer to the loaded XBE file contents.
 * @param xbe_size  Size of the XBE file.
 * @return TRUE on success, FALSE on failure.
 */
```

```text
/** Size of the simulated Xbox stack (8 MB).
 *  Increased from 1 MB because failed RECOMP_ICALL indirect calls
 *  can leak stdcall args onto the stack each frame. An 8 MB stack
 *  provides enough headroom for extended gameplay sessions. */
```

```text
/** Base VA of the stack area (above last XBE section). */
/* Where the fake TIB lives -- the linear address fs: is based at.
 *
 * Deliberately not 0. The TIB used to sit on page zero, because the lifter
 * dropped the fs prefix and fs:[N] became linear [N]. That made a null
 * dereference read or write the TIB instead of faulting: a null check of the
 * form `cmp byte [ecx], 0` saw the exception-chain head's 0xFF and passed, and
 * a store through a null pointer quietly overwrote that head. Both then
 * surfaced somewhere else entirely. With the TIB up here, page zero is left
 * unmapped and either mistake faults where it happens.
 *
 * Sits below every XBE's image base (0x00010000), so it displaces nothing.
 *
 * 0x4000 rather than 0x1000 because protection is applied at *host* page
 * granularity. Apple Silicon pages are 16 KB, so RECOMP_TRAP_NULL asking to
 * protect guest page zero actually covers guest 0..0x3FFF -- which reached a
 * TIB at 0x1000 and killed the run, so the guard disabled itself on every
 * such host and the diagnostic quietly did nothing. At 0x4000 the largest
 * page any supported host uses fits below the TIB and the guard installs.
 * Nothing else lives in the low 64 KB, and no guest code names the address:
 * fs: resolves through g_fs_base.
 *
 * Per-thread, because a TIB is. It used to be one constant address for the
 * whole process, which meant every guest thread shared one SEH chain head
 * and -- through fs:[4] -- one CRT per-thread data block. Half-Life 2
 * deadlocked on that: two threads in _lock() each holding the CRT lock the
 * other wanted, because the bookkeeping that decides who owns what was
 * shared between them.
 *
 * XBOX_TIB_MAIN is where the first thread's TIB is built; every spawned
 * thread gets its own from xbox_AllocThreadTib() and points g_fs_base at
 * it. */
```

```text
/** Exclusive top of the dynamic heap: the end of RAM for this run. Runtime,
 *  not a macro, because RAM size is now configurable (retail 64 MB vs devkit
 *  128 MB). The total mapped region (data + stack + heap) equals RAM so the
 *  engine's memory probing stops at the correct boundary. */
/* The heap runs to the end of the *mapped* range, not the end of RAM.
 *
 * When a title maps more address space than it has RAM, a large
 * MEM_RESERVE has to come from somewhere. Carving it out of a separate
 * arena above the heap looked tidy and was wrong: the guest CRT's own
 * bookkeeping never learns about that region, so realloc's block lookup
 * fails for a pointer in it and the copy is skipped -- a grown buffer
 * comes back empty with the old one still intact. Half-Life 2 loses a
 * 129-node CUtlRBTree that way, and the tree then self-cycles.
 *
 * Letting the ordinary heap serve the whole mapped range keeps every
 * allocation inside one allocator the guest already understands.
 */
```

## Original truncated-section handling and captured run counts

```c
            /* Zero the full virtual size first (handles BSS) */
            memset(XBOX_VA(sec_va), 0, sec_vsize);

            /*
             * Copy initialized data from XBE.
             *
             * A section whose raw data runs past the end of the buffer is a
             * truncated or corrupt image, not a BSS section, and it must not
             * be counted among the sections loaded. Reporting it as loaded is
             * how a 4MB title read through a 1MB buffer produced "Loaded
             * 17/17 sections" with every byte of every section still zero --
             * including the kernel thunk table, which then resolved 0 imports
             * and looked like a title that calls no kernel functions.
             */
            int have_data = (copy_size == 0) ||
                            (sec_raw_off + copy_size <= xbe_size);
            if (copy_size > 0 && have_data) {
                memcpy(XBOX_VA(sec_va), xbe + sec_raw_off, copy_size);
            } else if (!have_data) {
                fprintf(stderr,
                        "  WARNING: section %u (%s) raw data 0x%08X+%u runs past "
                        "the %zu-byte image -- left zeroed\n",
                        si, sec_name, sec_raw_off, copy_size, xbe_size);
                sections_short++;
            }
```
