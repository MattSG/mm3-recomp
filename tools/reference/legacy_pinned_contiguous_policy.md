# Historical title-specific contiguous allocation heuristic

The previous guest bridge bypassed allocator ownership for a tight low/high
range, derived from a title-specific observation. The generic bounded allocator
now owns constrained allocations and rejects unavailable ranges. This archive
is historical evidence, not current allocation advice or behavior.

```c
/* Contiguous memory is addressed through the physical-memory mirror: physical
 * page P is visible at 0x80000000 + P. Titles that pin buffers at fixed
 * physical addresses check the returned pointer against that, so the address
 * has to be honoured rather than satisfied from the general heap. */
#define XBOX_PHYSICAL_MIRROR_BASE 0x80000000u

    /*
     * A caller that constrains the range to exactly one allocation's worth is
     * demanding a specific physical address, not expressing a preference.
     * Halo does this for its two big pools and asserts on the result
     * (physical_memory_map.c:46) - XPhysicalAlloc passes lowest = the address
     * it wants and highest = lowest + size - 1, then requires
     * 0x80000000 | lowest back. Satisfying that from the heap fails the assert
     * and leaves its whole memory map wrong.
     */
    if (low && high >= low && (high - low + 1) <= size + 0x1000) {
        xbox_va = XBOX_PHYSICAL_MIRROR_BASE + low;

        /* The console hands out zeroed pages here, and titles rely on it:
         * pool headers and free-list roots are assumed clear, so whatever the
         * backing view happened to contain shows up later as structures that
         * are "allocated" but full of garbage. */
        memset((void *)((uintptr_t)xbox_va + g_xbox_mem_offset), 0, size);

        if (KERNEL_LOG_ON_HALF()) {
            fprintf(stderr, "  [KERNEL] MmAllocateContiguousMemoryEx: size=%u "
                    "pinned phys 0x%08X -> Xbox VA 0x%08X (zeroed)\n",
                    size, low, xbox_va);
            fflush(stderr);
        }
        g_eax = xbox_va;
        return;
    }

    if (align < 4096) align = 4096;
    xbox_va = xbox_ContiguousAlloc(size, align);

```

## Historical allocator observations

```c
/* Starts one page in. Physical page 0 is never handed out by the real
 * kernel, and the XDK's USB stack relies on that: XPP carves its host
 * controller structures from a private 0xFE0-byte arena ending at
 * 0x80001000 (sub_00365454 in Burnout 3), with no allocation call at all.
 * Starting at the base gave that same page to the title's first
 * MmAllocateContiguousMemory -- XAPI's launch data page -- and whichever
 * wrote last won. The symptom was timing-dependent: enumeration worked
 * when logging slowed the title down and otherwise stopped in the root
 * port reset, walking a device whose parent pointer had been overwritten. */
    /* Reuse freed pages first-fit by address, carving from the front of the
     * lowest free extent that fits; freed neighbours are coalesced on free.
     * Consecutive requests therefore come out adjacent, as they do from the
     * kernel's page allocator. Titles can build large vertex
     * buffers from consecutive 16 KB requests and addresses them as one span.
     * Best-fit reuse of whole freed blocks scattered those requests, so after
     * the first load the player car was drawn from 16 KB of mesh followed by
     * someone else's textures. */
```
