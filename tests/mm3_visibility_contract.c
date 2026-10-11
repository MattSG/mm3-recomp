/* The oracle is extracted from the actual generated leaf by the runner.
 * Check all guest registers, all x87 slots/status and the complete guest memory
 * image, not just the visible/rejected result. */
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include "recomp_types.h"

ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi;
RECOMP_TLS uint32_t g_ebp, g_seh_ebp;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top, g_fp_cmp;
RECOMP_TLS uint16_t g_fp_cc, g_fp_control_word;

#include "visibility_oracle.inc"
#include "../src/mm3_visibility.c"

static unsigned char memory[8192], initial[8192], expected_memory[8192];
static uint32_t rng = 0x12345678;
static uint32_t next_random(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}
static double random_number(void)
{
    return ((double)(int)(next_random() % 200001) - 100000) / 257.0;
}

typedef struct {
    uint32_t ax, cx, dx, sp, bx, si, di, bp, seh;
    double fp[8];
    int top, cmp;
    uint16_t cc, cw;
} State;

static State snapshot(void)
{
    State state;
    memset(&state, 0, sizeof state);
    state.ax = g_eax; state.cx = g_ecx; state.dx = g_edx; state.sp = g_esp;
    state.bx = g_ebx; state.si = g_esi; state.di = g_edi;
    state.bp = g_ebp; state.seh = g_seh_ebp;
    memcpy(state.fp, g_fp_stack, sizeof state.fp);
    state.top = g_fp_top; state.cmp = g_fp_cmp;
    state.cc = g_fp_cc; state.cw = g_fp_control_word;
    return state;
}
static void restore(State state)
{
    g_eax = state.ax; g_ecx = state.cx; g_edx = state.dx; g_esp = state.sp;
    g_ebx = state.bx; g_esi = state.si; g_edi = state.di;
    g_ebp = state.bp; g_seh_ebp = state.seh;
    memcpy(g_fp_stack, state.fp, sizeof state.fp);
    g_fp_top = state.top; g_fp_cmp = state.cmp;
    g_fp_cc = state.cc; g_fp_control_word = state.cw;
}

static void seed(unsigned test)
{
    unsigned n;
    for (n = 0; n < sizeof memory; ++n)
        memory[n] = (unsigned char)next_random();
    g_eax = next_random(); g_ecx = 512; g_edx = 1024; g_esp = 4096;
    g_ebx = next_random(); g_esi = next_random(); g_edi = next_random();
    g_ebp = next_random(); g_seh_ebp = next_random();
    g_fp_top = test % 8;
    g_fp_cmp = (int)(test % 4) - 1;
    g_fp_cc = (uint16_t)next_random();
    /* Every precision-control encoding crossed with all RC encodings. */
    g_fp_control_word = (uint16_t)(0x3f | ((test / 8) % 16) * 0x100);
    for (n = 0; n < 8; ++n) g_fp_stack[n] = random_number();
    MEM32(4100) = test % 25; /* Includes empty and multi-plane lists. */
    for (n = 0; n < 15; ++n) MEMF(512 + n * 4) = (float)random_number();
    for (n = 0; n < 96; ++n) MEMF(1024 + n * 4) = (float)random_number();
    if (test % 5 == 0) {
        /* Exact tangency, then one float to either side of the boundary. */
        float distance = (test % 3 == 0) ? 1.0f : (test % 3 == 1) ?
            nextafterf(1.0f, 0.0f) : nextafterf(1.0f, 2.0f);
        for (n = 0; n < 15; ++n) MEMF(512 + n * 4) = 0.0f;
        MEMF(512) = MEMF(528) = MEMF(544) = 1.0f;
        MEMF(560) = MEMF(564) = MEMF(568) = 1.0f;
        for (n = 0; n < 24; ++n) {
            MEMF(1024 + n * 16) = (n % 3 == 0) ? 1.0f : 0.0f;
            MEMF(1028 + n * 16) = (n % 3 == 1) ? 1.0f : 0.0f;
            MEMF(1032 + n * 16) = (n % 3 == 2) ? 1.0f : 0.0f;
            MEMF(1036 + n * 16) = -distance;
        }
    }
    if (test % 17 == 0) {
        static const uint32_t bits[] = {
            0, 0x80000000, 0x7f800000, 0xff800000, 0x7fc12345,
            1, 0x80000001, 0x7f7fffff, 0xff7fffff, 0x00800000
        };
        static const uint64_t fp_bits[] = {
            0, UINT64_C(0x8000000000000000), UINT64_C(0x7ff0000000000000),
            UINT64_C(0xfff0000000000000), UINT64_C(0x7ff8123456789abc),
            1, UINT64_C(0x8000000000000001)
        };
        MEM32(512 + (next_random() % 15) * 4) = bits[(test / 17) % 10];
        MEM32(1024 + (next_random() % 96) * 4) = bits[(test / 170) % 10];
        memcpy(&g_fp_stack[(test / 17) % 8], &fp_bits[(test / 17) % 7], 8);
    }
}

static double benchmark(void (*function)(void), unsigned iterations)
{
    LARGE_INTEGER start, end, frequency;
    unsigned i;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);
    for (i = 0; i < iterations; ++i) {
        g_ecx = 512; g_edx = 1024; g_esp = 4096;
        function();
    }
    QueryPerformanceCounter(&end);
    return (end.QuadPart - start.QuadPart) * 1e9 /
        (double)frequency.QuadPart / iterations;
}

int main(int argc, char **argv)
{
    unsigned i;
    (void)argv;
    g_xbox_mem_offset = (ptrdiff_t)memory;
    for (i = 0; i < 200000; ++i) {
        State start, expected, actual;
        seed(i);
        start = snapshot();
        memcpy(initial, memory, sizeof memory);
        visibility_oracle();
        expected = snapshot();
        memcpy(expected_memory, memory, sizeof memory);
        restore(start);
        memcpy(memory, initial, sizeof memory);
        sub_001C793A();
        actual = snapshot();
        if (memcmp(&expected, &actual, sizeof expected) ||
            memcmp(expected_memory, memory, sizeof memory)) {
            printf("FAIL case %u: control=%04X top=%d planes=%u\n",
                   i, start.cw, start.top, *(uint32_t *)(initial + 4100));
            return 1;
        }
    }
    puts("PASS: 200000 exact visibility state/memory cases");
    if (argc > 1) {
        /* The strict half-space test accepts the just-beyond-tangent case. */
        seed(5);
        MEM32(4100) = 6;
        visibility_oracle();
        if (g_edx != 1024 + 6 * 16 || LO8(g_eax) != 1) {
            puts("FAIL: benchmark fixture did not visit all six planes");
            return 1;
        }
        for (i = 0; i < 6; ++i) {
            g_fp_control_word = (i < 3) ? 0x3f : 0x23f;
            double original = benchmark(visibility_oracle, 1000000);
            double optimized = benchmark(sub_001C793A, 1000000);
            printf("6 passing planes, PC=%u: original %.1f ns; optimized %.1f ns; %.2fx\n",
                   (g_fp_control_word >> 8) & 3, original, optimized, original / optimized);
        }
    }
    return 0;
}
