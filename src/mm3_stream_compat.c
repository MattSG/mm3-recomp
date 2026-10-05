#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "recomp_types.h"

/* Original 0x145DC copies count bytes from stream+0xC to its first stack
 * argument, advances that cursor, returns the stream, and ret 8 preserves
 * EBX/ESI/EDI/EBP. The lifted 2KB and 64-byte helpers pass ABI checks, but
 * the remaining dispatch/epilogue corrupts the caller's saved registers and
 * returns with ESP 28 bytes too low on the 0x1110-byte Cruise resource.
 * Translate that copy contract directly, without skipping resource data.
 * RECOMP_STREAM_COPY_ORIGINAL selects the generated body for diagnosis. */
void mm3_stream_read_copy(void)
{
    uint32_t stream = g_ecx;
    uint32_t destination = MEM32(g_esp + 4u);
    uint32_t count = MEM32(g_esp + 8u);
    uint32_t source = MEM32(stream + 12u);
    memcpy(&MEM8(destination), &MEM8(source), count);
    MEM32(stream + 12u) = source + count;
    if (MEM32(g_esp) == 0x00170E7Eu && getenv("MM3_ASSET_TRACE")) {
        fprintf(stderr, "[ASSET_COPY_NATIVE] stream=%08X src=%08X dest=%08X bytes=%u match=%d\n",
            stream, source, destination, count,
            memcmp(&MEM8(destination), &MEM8(source), count) == 0);
    }
    g_eax = stream;
    g_ecx = count;
    g_edx = (count & 63u) - 1u;
    g_esp += 12u;
}
