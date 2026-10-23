#include "mm3_pb_recovery.h"
#include <kernel.h>
#include <xbox_memory_layout.h>
#include <nv2a_pb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *ram;
static uint32_t last, releases, calls, dispatched;
ptrdiff_t xbox_GetMemoryOffset(void)
{ return (ptrdiff_t)ram - (ptrdiff_t)XBOX_CONTIG_BASE; }
uint32_t nv2a_pb_exec_semaphore_last_value(void) { return last; }
uint32_t nv2a_pb_exec_semaphore_releases(void) { return releases; }
void nv2a_pb_exec_method(uint32_t subch, uint32_t method, uint32_t value)
{
    if (subch != 0 || method != 0x1D70) abort();
    calls++; dispatched = value; last = value; releases++;
}
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)

int main(void)
{
    uint32_t *words;
    const uint32_t from = XBOX_CONTIG_BASE + 0x100;
    ram = calloc(1, XBOX_CONTIG_SIZE);
    CHECK(ram);
    words = (uint32_t *)(ram + 0x100);
    last = 5;
    words[0] = 0x04000001; /* malformed jump: not a real packet header */
    words[1] = 0x00041D70; words[2] = 7;
    words[3] = 0x00041D70; words[4] = 9;
    mm3_pb_recover_stop(from, from + 20, "test");
    CHECK(calls == 1 && last == 9 && dispatched == 9);
    /* Even, backwards and overly distant candidates are not accepted. */
    calls = 0;
    words[2] = 10; words[4] = 0x9001;
    mm3_pb_recover_stop(from, from + 20, "test");
    CHECK(calls == 0 && last == 9);
    words[2] = 7; words[4] = 7;
    mm3_pb_recover_stop(from, from + 20, "test");
    CHECK(calls == 0);
    /* No reads for wrapped, unaligned, out-of-window or empty spans. */
    mm3_pb_recover_stop(from + 20, from, "test");
    mm3_pb_recover_stop(from + 1, from + 20, "test");
    mm3_pb_recover_stop(from, from + 19, "test");
    mm3_pb_recover_stop(XBOX_CONTIG_BASE - 4, from, "test");
    mm3_pb_recover_stop(from, XBOX_CONTIG_BASE + XBOX_CONTIG_SIZE + 4, "test");
    mm3_pb_recover_stop(from, from, "test");
    CHECK(calls == 0);
    /* A header without its value is not read past PUT. */
    words[0] = 0x00041D70; words[1] = 11;
    mm3_pb_recover_stop(from, from + 4, "test");
    CHECK(calls == 0);
    mm3_pb_recover_stop(from, from + 8, "test");
    CHECK(calls == 1 && last == 11);
    free(ram);
    puts("PASS: MM3 owns bounded fence recovery heuristic");
    return 0;
}
