#include "host_write_watch.h"
#include "platform/xbox_winnt.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>

#if defined(_WIN32)
/* Raised on the armed thread so the handler can write debug registers. */
#define HOST_WRITE_WATCH_ARM 0xE0484457u
#elif defined(__linux__)
#include <linux/hw_breakpoint.h>
#include <linux/perf_event.h>
#include <sys/syscall.h>
#include <unistd.h>
#ifndef TRAP_PERF
#define TRAP_PERF 6
#endif
#endif

static DWORD s_thread;
static volatile uint32_t *s_word;
static HostWriteWatchFn s_fn;
static void *s_user;
static PVOID s_handler;

static void host_write_watch_disarm(void)
{
    s_word = NULL;
    s_fn = NULL;
    s_user = NULL;
}

#if defined(_WIN32)
static LONG CALLBACK host_write_watch_exception(EXCEPTION_POINTERS *p)
{
    CONTEXT *c = p->ContextRecord;
    uint32_t value;
    if (!s_word || GetCurrentThreadId() != s_thread) return EXCEPTION_CONTINUE_SEARCH;
    if (p->ExceptionRecord->ExceptionCode == HOST_WRITE_WATCH_ARM) {
        /* Do not take slot zero from an attached debugger. */
        if (c->Dr7 & 3u) return EXCEPTION_CONTINUE_EXECUTION;
        c->ContextFlags |= CONTEXT_DEBUG_REGISTERS;
        c->Dr0 = (DWORD64)(uintptr_t)s_word;
        c->Dr7 = (c->Dr7 & ~0xF0003ull) | 0xD0001ull; /* write, four bytes */
        c->Dr6 &= ~1ull;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (p->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP || !(c->Dr6 & 1u))
        return EXCEPTION_CONTINUE_SEARCH;
    value = *s_word;
    c->Dr6 &= ~1ull;
    if (!s_fn(s_user, value, c->Rip)) {
        c->Dr7 &= ~3ull;
        host_write_watch_disarm();
    }
    return EXCEPTION_CONTINUE_EXECUTION;
}

static int host_write_watch_hardware(void)
{
    RaiseException(HOST_WRITE_WATCH_ARM, 0, 0, NULL);
    return 1;
}
#elif defined(__linux__)
static int s_fd = -1;

/* The breakpoint traps after the write. Other single-step users (the AC'97
 * trap flag) also arrive as EXCEPTION_SINGLE_STEP; win32_compat passes the
 * signal's si_code, and TRAP_PERF is ours. */
static LONG CALLBACK host_write_watch_exception(EXCEPTION_POINTERS *p)
{
    const EXCEPTION_RECORD *rec = p->ExceptionRecord;
    uint32_t value;
    if (!s_word || GetCurrentThreadId() != s_thread || s_fd < 0 ||
        rec->ExceptionCode != EXCEPTION_SINGLE_STEP || rec->NumberParameters < 1 ||
        rec->ExceptionInformation[0] != TRAP_PERF)
        return EXCEPTION_CONTINUE_SEARCH;
    value = *s_word;
    if (!s_fn(s_user, value, p->ContextRecord->Rip)) {
        close(s_fd);
        s_fd = -1;
        host_write_watch_disarm();
    }
    return EXCEPTION_CONTINUE_EXECUTION;
}

static int host_write_watch_hardware(void)
{
    struct perf_event_attr attr;
    memset(&attr, 0, sizeof attr);
    attr.type = PERF_TYPE_BREAKPOINT;
    attr.size = sizeof attr;
    attr.bp_type = HW_BREAKPOINT_W;
    attr.bp_addr = (uintptr_t)s_word;
    attr.bp_len = HW_BREAKPOINT_LEN_4;
    attr.sample_period = 1;
    attr.exclude_kernel = 1;
    attr.exclude_hv = 1;
    attr.remove_on_exec = 1;
    attr.sigtrap = 1;           /* synchronous SIGTRAP on the writing thread */
    /* This thread only, any CPU: debug registers are per thread. */
    s_fd = (int)syscall(SYS_perf_event_open, &attr, 0, -1, -1, PERF_FLAG_FD_CLOEXEC);
    if (s_fd < 0) {
        fprintf(stderr, "[HOST_WRITE_WATCH] perf_event breakpoint failed (%s)\n", strerror(errno));
        return 0;
    }
    return 1;
}
#else
static LONG CALLBACK host_write_watch_exception(EXCEPTION_POINTERS *p)
{
    (void)p;
    return EXCEPTION_CONTINUE_SEARCH;
}

static int host_write_watch_hardware(void)
{
    fprintf(stderr, "[HOST_WRITE_WATCH] no hardware watchpoints on this host\n");
    return 0;
}
#endif

int host_write_watch_arm(volatile uint32_t *word, HostWriteWatchFn fn, void *user)
{
    if (s_word || !word || !fn) return 0;
    if (!s_handler) s_handler = AddVectoredExceptionHandler(1, host_write_watch_exception);
    if (!s_handler) return 0;
    s_word = word;
    s_fn = fn;
    s_user = user;
    s_thread = GetCurrentThreadId();
    if (host_write_watch_hardware()) return 1;
    host_write_watch_disarm();
    return 0;
}
