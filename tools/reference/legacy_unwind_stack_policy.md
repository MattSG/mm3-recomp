# Historical main-stack-only unwind policy

Source: XboxRecomp `1574479768933a9618debeab3bf7464b180d7713`.
The original bridge uses main-stack bounds, unbounded scratch pushes and a
forced target after an incomplete walk. Preserved normalized reference follows.
The shared runtime now queries owned main/worker/callback stack ranges.

## src/kernel/kernel_bridge.c

```c
static void bridge_RtlUnwind(void)
{
    uint32_t target_frame = STACK_ARG(0);
    uint32_t exc_record   = STACK_ARG(2);
    uint32_t reg          = BRIDGE_MEM32(XBOX_FS_BASE);
    uint32_t prev_reg     = 0;
    uint32_t scratch      = 0;
    int      guard;

    /* An unwind with no record of its own still has to tell the handlers it
     * is an unwind, so synthesise one below the stack pointer. */
    if (!exc_record) {
        g_esp -= 0x50;
        scratch = g_esp;
        memset((uint8_t *)XBOX_TO_NATIVE(scratch), 0, 0x50);
        BRIDGE_MEM32(scratch) = 0xC0000027u;   /* STATUS_UNWIND */
        exc_record = scratch;
    }
    BRIDGE_MEM32(exc_record + 4) |= XBOX_EXCEPTION_UNWINDING
        | (target_frame ? 0u : XBOX_EXCEPTION_EXIT_UNWIND);

    for (guard = 0; guard < XBOX_SEH_MAX_FRAMES; guard++) {
        uint32_t next, handler;

        if (reg == XBOX_SEH_END_OF_CHAIN || reg == 0 || reg == target_frame)
            break;
        if ((reg & 3u) || reg < XBOX_STACK_BASE || reg >= XBOX_STACK_TOP)
            break;                       /* not a stack frame: chain is broken */
        if (prev_reg && reg <= prev_reg)
            break;                       /* not climbing: cycle or corruption */

        next    = BRIDGE_MEM32(reg);
        handler = BRIDGE_MEM32(reg + 4);

        /* Pop before dispatching. The handler may raise, and it must not see
         * its own frame still on the chain. */
        BRIDGE_MEM32(XBOX_FS_BASE) = next;

        if (handler) {
            recomp_func_t fn = recomp_lookup(handler);
            if (!fn) fn = recomp_lookup_manual(handler);
            if (!fn) fn = recomp_lookup_kernel(handler);
            if (fn) {
                /* EXCEPTION_DISPOSITION handler(record, frame, context,
                 * dispatcher) -- cdecl, so the caller pops. */
                g_esp -= 4; BRIDGE_MEM32(g_esp) = 0;
                g_esp -= 4; BRIDGE_MEM32(g_esp) = 0;
                g_esp -= 4; BRIDGE_MEM32(g_esp) = reg;
                g_esp -= 4; BRIDGE_MEM32(g_esp) = exc_record;
                g_esp -= 4; BRIDGE_MEM32(g_esp) = 0;  /* return address */
                fn();
                /* 16, not 20: the handler's own `ret` has already taken the
                 * return address off, leaving just the four arguments for the
                 * caller to drop. Cleaning 20 leaves esp four bytes high, and
                 * every argument the unwound-into frame reads after that comes
                 * from one slot over. */
                g_esp += 16;
            }
        }

        prev_reg = reg;
        reg      = next;
    }

    /* Land on the target even if the walk stopped early: leaving fs:[0] on a
     * discarded frame is worse than losing a __finally. */
    if (target_frame && target_frame != XBOX_SEH_END_OF_CHAIN)
        BRIDGE_MEM32(XBOX_FS_BASE) = target_frame;

    if (scratch)
        g_esp += 0x50;
}

```

## src/kernel/kernel_bridge.c

```c
static void bridge_MmCreateKernelStack(void)
{
    uint32_t size = STACK_ARG(0);
    uint32_t base_va = xbox_HeapAlloc(size, 16);

    g_eax = base_va ? base_va + size : 0;
}

```

## src/kernel/kernel_bridge.c

```c
static void bridge_MmDeleteKernelStack(void)
{
    xbox_HeapFree(STACK_ARG(1));
    g_eax = 0;
}

```
