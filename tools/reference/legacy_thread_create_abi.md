# Historical thread-create bridge assumptions

Source: XboxRecomp `0f75ac0cd6e69466cd052be0f05c0e46baff9573`.
The old seven-argument simple API and unconditional return-stack adjustment
are retained as normalized reference text. The generic bridge now follows
five arguments for ordinal254 and distinguishes one/two-context entry calls.
Primary signature evidence: https://github.com/XboxDev/nxdk/blob/master/lib/xboxkrnl/xboxkrnl.h

## src/kernel/kernel_bridge.c

```text
struct bridge_thread_start {
    recomp_func_t fn;
    uint32_t ctx1, ctx2, stack_top;
};

static void bridge_write_handle(uint32_t handle_va, HANDLE h);

static void bridge_run_thread_inline(recomp_func_t fn, uint32_t ctx1,
                                     uint32_t ctx2)
{
    g_esp -= 4; BRIDGE_MEM32(g_esp) = ctx2;
    g_esp -= 4; BRIDGE_MEM32(g_esp) = ctx1;
    g_esp -= 4; BRIDGE_MEM32(g_esp) = 0;
    g_seh_ebp = g_esp;
    fn();
    g_esp += 12;
}

void guest_cpu_join(void);
void guest_cpu_part(void);

static DWORD WINAPI bridge_thread_main(LPVOID param)
{
    struct bridge_thread_start *s = (struct bridge_thread_start *)param;
    recomp_func_t fn = s->fn;
    uint32_t ctx1 = s->ctx1, ctx2 = s->ctx2;

    /* Own register set (RECOMP_TLS), own simulated stack -- and own TIB.
     *
     * The TIB carries the SEH chain head and, through fs:[4], the CRT's
     * per-thread data. Sharing one made "which thread am I" a single answer
     * for every thread, which is how two of them ended up inside _lock() each
     * holding the lock the other wanted. */
    g_is_spawned_thread = 1;
    g_esp = s->stack_top;
    g_thread_stack_top = s->stack_top;
    {
        uint32_t tib = xbox_AllocThreadTib();
        if (tib)
            g_fs_base = tib;
        else
            fprintf(stderr, "  [KERNEL] worker thread has no TIB of its own;"
                            " it shares the main thread's\n");
    }
    free(s);

    guest_cpu_join();
    bridge_run_thread_inline(fn, ctx1, ctx2);
    guest_cpu_part();

    fprintf(stderr, "  [KERNEL] worker thread returned (eax=0x%08X)\n", g_eax);
    fflush(stderr);
    /* The routine returned instead of calling PsTerminateSystemThread; the
     * stack is still ours to give back. */
    xbox_FreeThreadStack(g_thread_stack_top);
    g_thread_stack_top = 0;
    return 0;
}

static HANDLE bridge_spawn_thread(recomp_func_t fn, uint32_t ctx1,
                                  uint32_t ctx2, uint32_t stack_top)
{
    struct bridge_thread_start *s = malloc(sizeof(*s));
    HANDLE th;

    if (!s) return NULL;
    s->fn = fn; s->ctx1 = ctx1; s->ctx2 = ctx2; s->stack_top = stack_top;

    th = CreateThread(NULL, 0, bridge_thread_main, s, 0, NULL);
    if (!th) free(s);
    /* Record the game thread so a host-tick-driven title's watchdog can sample
     * it via xbox_thread_debug_handle. Harmless for default-model titles: they
     * spawn workers too, but never read it back. See kernel_thread.c. */
    else xbox_set_game_thread(th);
    return th;
}
```

## src/kernel/kernel_bridge.c

```text
static void bridge_PsCreateSystemThreadEx(void)
{
    uint32_t xbox_handle_ptr = STACK_ARG(0);
    uint32_t start_context1  = STACK_ARG(5);
    uint32_t start_context2  = STACK_ARG(6);
    uint32_t start_routine   = STACK_ARG(9);
    /* In SPAWN mode there is no privileged "first call": every thread is real,
     * so the entry can return. In INLINE mode the first call runs the game. */
    int is_first_call = (g_thread_mode == XBOX_THREAD_MODE_INLINE)
                        && (g_thread_call_count == 0);
    g_thread_call_count++;

    fprintf(stderr, "  [KERNEL] PsCreateSystemThreadEx #%d: routine=0x%08X ctx1=0x%08X ctx2=0x%08X\n",
            g_thread_call_count, start_routine, start_context1, start_context2);
    fflush(stderr);

    /* Default handle: 0xBEEF0001, an untagged sentinel. It is replaced below
     * by a real table token when a worker is spawned on its own host thread.
     * It stays for the threads that run inline (the main thread, and workers
     * when no stack is free or RECOMP_WORKERS=inline). Those have no host
     * thread object, so there is no real handle to give; the value is safe
     * because bridge_resolve_handle passes it through unchanged and Win32
     * rejects it, so a wait on it fails at once instead of hanging. */
    if (xbox_handle_ptr) {
        BRIDGE_MEM32(xbox_handle_ptr) = 0xBEEF0001u;
    }

    /* Call the start routine synchronously through the recomp dispatch.
     * Xbox thread start routines receive two parameters:
     *   void ThreadRoutine(PVOID StartContext1, PVOID StartContext2)
     * We push both onto the simulated stack (right-to-left).
     *
     * First call: the game's main thread entry point. Must run synchronously
     * and inherit the current register state (this IS the game starting).
     *
     * Subsequent calls: worker threads. Must save/restore ALL global registers
     * because on real Xbox each thread has its own register set. Without this,
     * the worker clobbers the caller's g_esi, g_ebx, etc. */
    if (start_routine) {
        recomp_func_t fn = recomp_lookup(start_routine);
        if (!fn) fn = recomp_lookup_manual(start_routine);
        if (fn) {
            if (is_first_call) {
                /* Main game thread: run directly, inheriting register state */
                g_esp -= 4; BRIDGE_MEM32(g_esp) = start_context2;
                g_esp -= 4; BRIDGE_MEM32(g_esp) = start_context1;
                g_esp -= 4; BRIDGE_MEM32(g_esp) = 0;
                fn();
                g_esp += 12;
                fprintf(stderr, "  [KERNEL] PsCreateSystemThreadEx: main thread returned (g_eax=0x%08X)\n", g_eax);
                fflush(stderr);
            } else {
                /* Worker thread: a real one.
                 *
                 * This used to run the routine synchronously and restore the
                 * caller's registers afterwards, which is fine only for a
                 * worker that finishes. Halo's cache/file worker does not -- it
                 * blocks on an event waiting for requests, so CreateThread
                 * never returned and startup deadlocked before the main loop.
                 *
                 * Now that the register set is thread-local (RECOMP_TLS), a
                 * spawned thread gets its own, and the caller's is untouched by
                 * construction rather than by save/restore. */
                /* RECOMP_WORKERS=inline runs a title's worker routines on
                 * the calling thread instead of spawning one.
                 *
                 * Not a mode to ship a title in -- a worker that blocks
                 * waiting for requests never returns, and the caller never
                 * gets control back. It is a bisecting tool: when something
                 * only goes wrong with two guest threads running, this says so
                 * in one run, and the answer separates a concurrency bug from
                 * everything else it might have been. */
                const char *inline_workers = getenv("RECOMP_WORKERS");
                uint32_t stack_top;

                if (inline_workers && !strcmp(inline_workers, "inline")) {
                    fprintf(stderr, "  [KERNEL] RECOMP_WORKERS=inline: running "
                            "worker 0x%08X on this thread\n", start_routine);
                    fflush(stderr);
                    bridge_run_thread_inline(fn, start_context1, start_context2);
                    return;
                }

                stack_top = xbox_AllocThreadStack();

                if (!stack_top) {
                    fprintf(stderr, "  [KERNEL] PsCreateSystemThreadEx: out of "
                            "thread stacks, running worker 0x%08X inline\n",
                            start_routine);
                    fflush(stderr);
                    bridge_run_thread_inline(fn, start_context1, start_context2);
                } else {
                    HANDLE th = bridge_spawn_thread(fn, start_context1,
                                                    start_context2, stack_top);
                    fprintf(stderr, "  [KERNEL] PsCreateSystemThreadEx: spawned "
                            "worker 0x%08X (ctx=0x%08X, stack top 0x%08X)\n",
                            start_routine, start_context1, stack_top);
                    fflush(stderr);
                    if (xbox_handle_ptr && th) {
                        bridge_write_handle(xbox_handle_ptr, th);
                    }
                }
            }
        } else {
            fprintf(stderr, "  [KERNEL] PsCreateSystemThreadEx: start routine 0x%08X not found in dispatch!\n",
                    start_routine);
        }
    }

    g_eax = 0; /* STATUS_SUCCESS */
}
```

## src/kernel/kernel_bridge.c

```text
/* --- PsCreateSystemThread (ordinal 254, 7 args = 28 bytes)
 * NTSTATUS PsCreateSystemThread(PHANDLE ThreadHandle, ULONG KernelStackSize,
 *                               ULONG TlsDataSize, PULONG ThreadId,
 *                               PVOID StartContext1, PVOID StartContext2,
 *                               PXBOX_SYSTEM_ROUTINE StartRoutine);
 * Mirror of PsCreateSystemThreadEx (ordinal 255) with the ThreadExtraSize
 * argument dropped: same first-call-inline / worker-spawn behaviour.
 * Requires bridge_spawn_thread/bridge_run_thread_inline/g_thread_mode from
 * Section A of kernel_bridge.c. */
static void bridge_PsCreateSystemThread(void)
{
    uint32_t xbox_handle_ptr = STACK_ARG(0);
    uint32_t start_context1  = STACK_ARG(4);
    uint32_t start_context2  = STACK_ARG(5);
    uint32_t start_routine   = STACK_ARG(6);
    int is_first_call = (g_thread_mode == XBOX_THREAD_MODE_INLINE)
                        && (g_thread_call_count == 0);

    g_thread_call_count++;

    /* Fake handle until the worker path can write a real one. */
    if (xbox_handle_ptr)
        BRIDGE_MEM32(xbox_handle_ptr) = 0xBEEF0002;

    if (start_routine) {
        recomp_func_t fn = recomp_lookup(start_routine);
        if (!fn)
            fn = recomp_lookup_manual(start_routine);
        if (fn) {
            if (is_first_call) {
                g_esp -= 4; BRIDGE_MEM32(g_esp) = start_context2;
                g_esp -= 4; BRIDGE_MEM32(g_esp) = start_context1;
                g_esp -= 4; BRIDGE_MEM32(g_esp) = 0;
                fn();
                g_esp += 12;
            } else {
                const char *inline_workers = getenv("RECOMP_WORKERS");
                uint32_t stack_top;

                if (inline_workers && !strcmp(inline_workers, "inline")) {
                    bridge_run_thread_inline(fn, start_context1, start_context2);
                    g_eax = 0;
                    return;
                }
                stack_top = xbox_AllocThreadStack();
                if (!stack_top) {
                    bridge_run_thread_inline(fn, start_context1, start_context2);
                } else {
                    HANDLE th = bridge_spawn_thread(fn, start_context1,
                                                    start_context2, stack_top);
                    if (xbox_handle_ptr && th)
                        bridge_write_handle(xbox_handle_ptr, th);
                }
            }
        } else {
            fprintf(stderr, "  [KERNEL] PsCreateSystemThread: start routine 0x%08X not found in dispatch!\n",
                    start_routine);
        }
    }
    g_eax = 0;  /* STATUS_SUCCESS */
}
```

## src/kernel/kernel_bridge.c

```text
    case 254: return 28;  /* PsCreateSystemThread (7) */
```
