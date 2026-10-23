# Historical native thread callback assumptions

Source: XboxRecomp `760ca88b46d00bde1ffa5f4d4038850e6e574058`.
Old callback typedef, native entry implementation and guide follow as normalized
reference. The native facade now distinguishes start/system routines and its
host-callback contract from guest CPU dispatch.

## src/kernel/kernel.h

```text
/* Thread start routine (Xbox uses __stdcall) */
typedef VOID (__stdcall *PXBOX_SYSTEM_ROUTINE)(PVOID StartContext);
```

## src/kernel/kernel.h

```text
NTSTATUS __stdcall xbox_PsCreateSystemThreadEx(
    PHANDLE ThreadHandle, ULONG ThreadExtraSize, ULONG KernelStackSize,
    ULONG TlsDataSize, PULONG ThreadId, PVOID StartContext1, PVOID StartContext2,
    BOOLEAN CreateSuspended, BOOLEAN DebugStack,
    PXBOX_SYSTEM_ROUTINE StartRoutine);
```

## src/kernel/kernel_thread.c

```text
typedef struct _XBOX_THREAD_START_INFO {
    PXBOX_SYSTEM_ROUTINE StartRoutine;
    PVOID                StartContext;
} XBOX_THREAD_START_INFO;

static DWORD WINAPI xbox_thread_wrapper(LPVOID lpParameter)
{
    XBOX_THREAD_START_INFO info = *(XBOX_THREAD_START_INFO*)lpParameter;

    /* Free the start info before calling the routine - the routine may
     * never return (calling PsTerminateSystemThread instead) */
    HeapFree(GetProcessHeap(), 0, lpParameter);

    xbox_log(XBOX_LOG_DEBUG, XBOX_LOG_THREAD, "Thread %u starting at %p",
        GetCurrentThreadId(), info.StartRoutine);

    info.StartRoutine(info.StartContext);

    xbox_log(XBOX_LOG_DEBUG, XBOX_LOG_THREAD, "Thread %u returned normally",
        GetCurrentThreadId());

    return 0;
}

/* ============================================================================
 * PsCreateSystemThreadEx
 *
 * Xbox signature:
 *   PsCreateSystemThreadEx(
 *     OUT PHANDLE ThreadHandle,
 *     IN ULONG ThreadExtraSize,      // extra bytes in thread object (ignored)
 *     IN ULONG KernelStackSize,      // stack size (0 = default)
 *     IN ULONG TlsDataSize,          // TLS data size (Xbox-specific, ignored)
 *     OUT PULONG ThreadId,           // optional thread ID
 *     IN PVOID StartContext1,        // context passed to StartRoutine
 *     IN PVOID StartContext2,        // alternate context (unused by game code)
 *     IN BOOLEAN CreateSuspended,
 *     IN BOOLEAN DebugStack,         // debug stack (ignored)
 *     IN PXBOX_SYSTEM_ROUTINE StartRoutine
 *   )
 *
 * Maps to: CreateThread with a wrapper for calling convention adaptation.
 * ============================================================================ */

NTSTATUS __stdcall xbox_PsCreateSystemThreadEx(
    PHANDLE ThreadHandle,
    ULONG ThreadExtraSize,
    ULONG KernelStackSize,
    ULONG TlsDataSize,
    PULONG ThreadId,
    PVOID StartContext1,
    PVOID StartContext2,
    BOOLEAN CreateSuspended,
    BOOLEAN DebugStack,
    PXBOX_SYSTEM_ROUTINE StartRoutine)
{
    XBOX_THREAD_START_INFO* info;
    HANDLE hThread;
    DWORD dwThreadId;
    DWORD dwCreationFlags;

    (void)ThreadExtraSize;
    (void)TlsDataSize;
    (void)StartContext2;
    (void)DebugStack;

    if (!ThreadHandle || !StartRoutine)
        return STATUS_INVALID_PARAMETER;

    /* Allocate start info - freed by the wrapper thread */
    info = (XBOX_THREAD_START_INFO*)HeapAlloc(GetProcessHeap(), 0, sizeof(XBOX_THREAD_START_INFO));
    if (!info)
        return STATUS_NO_MEMORY;

    info->StartRoutine = StartRoutine;
    info->StartContext = StartContext1;

    dwCreationFlags = CreateSuspended ? CREATE_SUSPENDED : 0;

    /* Use default stack size if 0 (Xbox default is 64KB) */
    if (KernelStackSize == 0)
        KernelStackSize = 65536;

    hThread = CreateThread(NULL, KernelStackSize, xbox_thread_wrapper, info,
                           dwCreationFlags, &dwThreadId);
    if (!hThread) {
        HeapFree(GetProcessHeap(), 0, info);
        xbox_log(XBOX_LOG_ERROR, XBOX_LOG_THREAD,
            "PsCreateSystemThreadEx: CreateThread failed (error %u)", GetLastError());
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    *ThreadHandle = hThread;
    if (ThreadId)
        *ThreadId = dwThreadId;

    xbox_log(XBOX_LOG_INFO, XBOX_LOG_THREAD,
        "PsCreateSystemThreadEx: created thread %u (handle=%p, routine=%p, suspended=%d)",
        dwThreadId, hThread, StartRoutine, CreateSuspended);

    return STATUS_SUCCESS;
}
```

## src/kernel/README.md

```text
## Threading

Xbox threads use `PsCreateSystemThreadEx` with NT-style parameters. We create Win32 threads with a wrapper that sets up the register context:

```c
// The kernel bridge creates a thread that:
// 1. Looks up the start routine via recomp_lookup()
// 2. Sets up ESP for the new thread
// 3. Calls the recompiled function
NTSTATUS xbox_PsCreateSystemThreadEx(
    PHANDLE ThreadHandle,
    ULONG   ThreadExtraSize,
    ULONG   KernelStackSize,
    ULONG   TlsDataSize,
    PULONG  ThreadId,
    PVOID   StartContext1,
    PVOID   StartContext2,
    BOOLEAN CreateSuspended,
    BOOLEAN DebuggerThread,
    PVOID   StartRoutine        // Xbox VA → resolved via recomp_lookup()
);
```
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
                bridge_run_thread_inline(fn, start_context1, start_context2, 2);
                fprintf(stderr, "  [KERNEL] PsCreateSystemThreadEx: main thread returned (g_eax=0x%08X)\n", g_eax);
                fflush(stderr);
            } else {
                /* Spawned guests own independent registers and stack. */
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
                    bridge_run_thread_inline(fn, start_context1, start_context2, 2);
                    return;
                }

                stack_top = xbox_AllocThreadStackSized(STACK_ARG(2));

                if (!stack_top) {
                    if (xbox_handle_ptr) BRIDGE_MEM32(xbox_handle_ptr)=0;
                    g_eax=0xC0000017u; /* STATUS_NO_MEMORY */
                    return;
                } else {
                    HANDLE th = bridge_spawn_thread(fn, start_context1,
                                                    start_context2, stack_top, 2);
                    fprintf(stderr, "  [KERNEL] PsCreateSystemThreadEx: spawned "
                            "worker 0x%08X (ctx=0x%08X, stack top 0x%08X)\n",
                            start_routine, start_context1, stack_top);
                    fflush(stderr);
                    if (!th) {
                        if (xbox_handle_ptr) BRIDGE_MEM32(xbox_handle_ptr)=0;
                        g_eax=0xC000009Au; /* STATUS_INSUFFICIENT_RESOURCES */
                        return;
                    }
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
static void bridge_PsCreateSystemThread(void)
{
    uint32_t xbox_handle_ptr = STACK_ARG(0);
    uint32_t start_context1  = STACK_ARG(3);
    uint32_t start_context2  = 0;
    uint32_t start_routine   = STACK_ARG(2);
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
                bridge_run_thread_inline(fn, start_context1, 0, 1);
            } else {
                const char *inline_workers = getenv("RECOMP_WORKERS");
                uint32_t stack_top;

                if (inline_workers && !strcmp(inline_workers, "inline")) {
                    bridge_run_thread_inline(fn, start_context1, start_context2, 1);
                    g_eax = 0;
                    return;
                }
                stack_top = xbox_AllocThreadStack();
                if (!stack_top) {
                    if (xbox_handle_ptr) BRIDGE_MEM32(xbox_handle_ptr)=0;
                    g_eax=0xC0000017u; /* STATUS_NO_MEMORY */
                    return;
                } else {
                    HANDLE th = bridge_spawn_thread(fn, start_context1,
                                                    start_context2, stack_top, 1);
                    if (!th) {
                        if (xbox_handle_ptr) BRIDGE_MEM32(xbox_handle_ptr)=0;
                        g_eax=0xC000009Au; /* STATUS_INSUFFICIENT_RESOURCES */
                        return;
                    }
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

## src/kernel/kernel_thread.c

```text
NTSTATUS __stdcall xbox_PsTerminateSystemThread(NTSTATUS ExitStatus)
{
    xbox_log(XBOX_LOG_DEBUG, XBOX_LOG_THREAD,
        "PsTerminateSystemThread: thread %u exiting with status 0x%08X",
        GetCurrentThreadId(), ExitStatus);

    ExitThread((DWORD)ExitStatus);

    /* ExitThread never returns, but the compiler needs this */
    return STATUS_SUCCESS;
}

```
