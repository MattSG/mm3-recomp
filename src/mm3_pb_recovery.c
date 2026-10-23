/* Inherited MM3 compatibility, not hardware command decoding. After a malformed
 * submission the original runtime searched undecoded data for a plausible fence.
 * Keep that policy here so other consumers never execute guessed commands. */
#include "mm3_pb_recovery.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <kernel.h>
#include <xbox_memory_layout.h>
#include <nv2a_pb.h>

static int mm3_pb_word_in_window(uint32_t va)
{
    return (va & 3u) == 0 && va - XBOX_CONTIG_BASE <= XBOX_CONTIG_SIZE - 4u;
}

void mm3_pb_recover_stop(uint32_t from, uint32_t to, const char *reason)
{
    const uint8_t *mem = (const uint8_t *)xbox_GetMemoryOffset();
    static unsigned reports;
    uint32_t last, releases;
    int found = 0;
    (void)reason;
    if (from >= to || !mm3_pb_word_in_window(from) ||
        !mm3_pb_word_in_window(to - 4u)) return;
    last = nv2a_pb_exec_semaphore_last_value();
    /* ponytail: inherited odd/128 heuristic can mistake payload for commands;
     * remove this compatibility recovery when malformed submissions are fixed. */
    for (uint32_t va = from; va + 4u < to; va += 4u) {
        uint32_t word, value;
        memcpy(&word, mem + va, 4);
        if (word != 0x00041D70u) continue;
        memcpy(&value, mem + va + 4u, 4);
        if ((value & 1u) && (int32_t)(value - last) > 0 && value - last <= 128u) {
            last = value;
            found = 1;
        }
    }
    if (!found) return;
    releases = nv2a_pb_exec_semaphore_releases();
    nv2a_pb_exec_method(0, 0x1D70u, last);
    if (reports++ < 8)
        fprintf(stderr, "[MM3_PB] stop recovery fence=0x%08X released=%d\n",
            last, nv2a_pb_exec_semaphore_releases() != releases);
}
