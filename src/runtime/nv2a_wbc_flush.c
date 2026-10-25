/* XDK write-back-cache kick for the NV2A command stream.
 *
 * The retail routine fences guest writes, sets NV_PFB_WBC_FLUSH and waits for
 * the executor to acknowledge by clearing it. A byte-lag threshold cannot prove
 * the executor has consumed a reused push-buffer or produced a result the guest
 * is about to read, so every kick waits for the acknowledgement. */
#include "nv2a_wbc_flush.h"
#include "nv2a_regs.h"
#include "platform/xbox_winnt.h"
#include "xbox_memory_layout.h"
#include <stdio.h>
#include <xmmintrin.h>

/* Device object: the NV2A register block pointer lives at device+0x17C8. */
#define NV2A_DEVICE_REGS_OFFSET 0x17C8u
/* PFB base plus NV_PFB_WBC: the flush word is in the register block. */
#define NV2A_PFB_WBC_ADDRESS (0x00100000u + NV_PFB_WBC)

static uint32_t wbc_guest32(uint32_t va)
{
    return *(volatile uint32_t *)((uintptr_t)va + (uintptr_t)xbox_GetMemoryOffset());
}

static void wbc_guest32_store(uint32_t va, uint32_t value)
{
    *(volatile uint32_t *)((uintptr_t)va + (uintptr_t)xbox_GetMemoryOffset()) = value;
}

/* The ack thread clears the bit between segments. A dead thread would spin
 * forever with no sign in the log, so report the wait every two seconds. It
 * keeps waiting: leaving early would let the title reuse push-buffer space the
 * executor has not read yet. */
static void wbc_wait_ack(uint32_t flush_va)
{
    LARGE_INTEGER freq, start, now;
    LONGLONG last;
    unsigned reports = 0;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    last = start.QuadPart;
    while (wbc_guest32(flush_va) & NV_PFB_WBC_FLUSH) {
        QueryPerformanceCounter(&now);
        if (now.QuadPart - last >= 2 * freq.QuadPart) {
            last = now.QuadPart;
            fprintf(stderr, "[KICK_WAIT] ack bit 0x10000 at 0x%08X still set after %.1f s (report %u)\n",
                    flush_va, (double)(now.QuadPart - start.QuadPart) / freq.QuadPart,
                    ++reports);
        }
    }
}

void xbox_Nv2aWbcFlush(uint32_t device_ptr_va, uint32_t *object_out, uint32_t *value_out)
{
    uint32_t object = wbc_guest32(wbc_guest32(device_ptr_va) + NV2A_DEVICE_REGS_OFFSET);
    uint32_t flush = object + NV2A_PFB_WBC_ADDRESS;
    uint32_t value;
    _mm_sfence();
    value = wbc_guest32(flush) | NV_PFB_WBC_FLUSH;
    wbc_guest32_store(flush, value);
    wbc_wait_ack(flush);
    if (object_out) *object_out = object;
    if (value_out) *value_out = value;
}
