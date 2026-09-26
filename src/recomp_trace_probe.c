#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "recomp_types.h"

#ifndef _MSC_VER
void __real_recomp_trace_esp(const char *name, const char *tag);

void __wrap_recomp_trace_esp(const char *name, const char *tag)
{
    if (getenv("RECOMP_TRACE_EFD85") &&
        strcmp(name, "sub_001EFD85") == 0 &&
        (strcmp(tag, "after call 0x001BFC1F") == 0 ||
         strcmp(tag, "after call 0x00015C92") == 0)) {
        uint32_t frame = g_esp + 0xFCu;
        uint32_t item = frame - 0xA0u;
        fprintf(stderr,
                "[EFD85_STATE] %s ebp=%08X result=%08X iter=%08X/%08X "
                "item=%08X %08X %08X %08X %08X %08X edi=%08X\n",
                tag, frame, MEM32(frame - 8u), MEM32(item), MEM32(item + 4u),
                MEM32(item + 8u), MEM32(item + 12u), MEM32(item + 16u),
                MEM32(item + 20u), MEM32(item + 24u), g_edi);
    }

    __real_recomp_trace_esp(name, tag);
}
#endif
