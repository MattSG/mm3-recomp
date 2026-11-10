/* Car lamp trace. sub_000CF2DA draws one lamp node of a car (ECX = node, stack +4 the lamp's transform, +12 the
 * mask of lamp kinds to draw): +0x34 the kind (0 front, 1 tail, 2 brake, 3 reverse, 4 siren, 5/6 signals,
 * 8 projector, 9 siren env), +4 the owner car, +0x54 the brightness, +0x64 the ARGB colour.
 * MM3_LAMP_TRACE=1 logs each lamp when its state changes; nothing else is changed. */
#include <stdio.h>
#include <stdlib.h>
#include "platform/xbox_winnt.h"
#include "recomp_types.h"

extern void sub_000CF2DA_original(void);

static void trace_lamp(uint32_t node, uint32_t stack)
{
    static struct { uint32_t node; float brightness; uint32_t color, mask; } seen[4096];
    static unsigned lines;
    uint32_t i, mask = MEM32(stack + 12), color = MEM32(node + 0x64);
    float brightness = MEMF(node + 0x54);
    if (lines > 20000) return;
    for (i = 0; i < 4096 && seen[i].node && seen[i].node != node; ++i) {}
    if (i == 4096) return;
    if (seen[i].node == node && seen[i].brightness == brightness && seen[i].color == color && seen[i].mask == mask) return;
    seen[i].node = node; seen[i].brightness = brightness; seen[i].color = color; seen[i].mask = mask;
    fprintf(stderr, "[LAMP] node %08X owner %08X kind %u brightness %.3f color %08X mask %08X tod %u\n", node,
            MEM32(node + 4), MEM32(node + 0x34), brightness, color, mask, MEM32(0x00407C30u));
    fflush(stderr);
    ++lines;
}

void sub_000CF2DA(void)
{
    static int trace = -1;
    if (trace < 0) trace = getenv("MM3_LAMP_TRACE") != NULL;
    if (trace) trace_lamp(g_ecx, g_esp);
    sub_000CF2DA_original();
}
