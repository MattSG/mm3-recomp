# Historical main-stack layout policy

Source: XboxRecomp `b6977ea58650c56959261a1753e516174753e79b`. The fixed main-stack placement and extra
capacity are now selected by the MM3 port. Old constants and descriptions
follow as normalized text; they are not current generic runtime guarantees.

## src/kernel/xbox_memory_layout.h

```c
#define XBOX_STACK_SIZE     (8 * 1024 * 1024)
```

## src/kernel/xbox_memory_layout.h

```c
#define XBOX_STACK_BASE     0x00780000
```

## src/kernel/xbox_memory_layout.h

```c
#define XBOX_STACK_TOP      (XBOX_STACK_BASE + XBOX_STACK_SIZE - 16)
```

## src/kernel/xbox_memory_layout.h

```c
#define XBOX_HEAP_BASE      (XBOX_STACK_BASE + XBOX_STACK_SIZE)  /* 0x00F80000 */
```

## src/kernel/xbox_memory_layout.c

```c
    /* Callback stacks may not cover any loaded section. Keep their whole
     * reserved pool in mapped guest RAM; failure disables pool leases so the
     * existing callers can use their allocation-failure/fallback paths. */
    {
        uint64_t base = ((uint64_t)g_xbox_image_hi + 4095u) & ~UINT64_C(4095);
        if (base < XBOX_STACK_BASE) base = XBOX_STACK_BASE;
        uint64_t end = base + (uint64_t)XBOX_WORKER_STACK_SIZE * XBOX_WORKER_STACK_COUNT;
        xbox_worker_stack_reset(end <= g_memory_size ? (uint32_t)base : 0);
    }

```

## src/kernel/xbox_memory_layout.c

```c
/*
 * Simulated stacks for spawned threads.
 *
 * The main thread owns the top of the XBOX_STACK region and grows down; worker
 * stacks are carved from the bottom upward so the two cannot meet until the
 * whole 8 MB is gone. Xbox VAs, not host memory: recompiled code addresses its
 * stack through MEM32() like any other Xbox pointer.
 */
```

## src/kernel/xbox_memory_layout.c

```c
/* From the heap, not from XBOX_STACK_BASE.
     *
     * The stack region begins at 0x00780000, which is fine only while the
     * title's image ends below that. Half-Life 2's image runs to 0x009B68C0,
     * so the first thread stack (0x00780000..0x00800000) landed inside its
     * .rdata and .data: the worker spawned during engine init wrote its
     * frames over the game's own static data. Nothing faults -- the pages are
     * mapped and writable -- so it shows up later as globals that were
     * correct when written and wrong when read.
     *
     * The heap already starts above the image and knows how big it is, so
     * taking slices from it is correct for any image size instead of only
     * for small ones.
     */
```

## src/kernel/kernel_thread.c

```c
/* Standalone hosts use the conventional runtime stack region. The XBE loader
 * replaces it with an image-aware range before guest workers are started. */
uint32_t g_xbox_worker_stack_base = XBOX_STACK_BASE;
```

## docs/pipeline/04-lifting.md

```text
The stack is initialized to `XBOX_STACK_TOP` (0x00F7FFF0) -- the top of an 8 MB region allocated in Xbox memory space.
```

## docs/technical/register-model.md

```text
The Xbox stack lives in mapped Xbox memory. `g_esp` starts at the top of an 8 MB region:

```c
#define XBOX_STACK_BASE  0x00780000
#define XBOX_STACK_SIZE  (8 * 1024 * 1024)
#define XBOX_STACK_TOP   (XBOX_STACK_BASE + XBOX_STACK_SIZE - 16)

// During init:
g_esp = XBOX_STACK_TOP;  // 0x00F7FFF0
```
```
