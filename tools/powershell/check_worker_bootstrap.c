/* Checks the exact project bootstrap bodies extracted by the runner. */
#include "recomp_types.h"
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>

static void check_indirect(uint32_t target);
void sub_000838C3(void);
void sub_001E7B8F_gen(void);
#undef RECOMP_ICALL_SAFE
#define RECOMP_ICALL_SAFE(target, saved_esp) check_indirect((uint32_t)(target))
#include "worker_bootstrap_body.inc"

ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp;
RECOMP_TLS uint32_t g_ebx, g_esi, g_edi, g_ebp, g_seh_ebp, g_fs_base;
RECOMP_TLS int g_df;
static jmp_buf terminated;
static uint32_t context, calls;
static unsigned char memory[4 * 1024 * 1024];

void sub_001E7B8F_gen(void)
{
    assert(MEM32(g_esp) == 0x83ADEu);
    assert(MEM32(g_esp + 4) == context);
    assert(g_ebx == 0x87654321u);
    calls++;
}

void sub_000838C3(void)
{
    assert(MEM32(g_esp) == (calls ? 0x83AE8u : 0x83AD8u));
    assert(MEM32(g_esp + 4) == (calls ? 0u : 1u));
    g_esp += 8;
}

static void check_indirect(uint32_t target)
{
    if (target == 0x2F75B0u) {
        assert(MEM32(g_esp) == 0x83ADEu);
        assert(MEM32(g_esp + 4) == context);
        g_esp += 8; /* Original worker's RET 4. */
        g_eax = 0x12345678;
        calls++;
    } else {
        assert(target == 0xFE000102u);
        assert(calls == 1 && MEM32(g_esp) == 0x83B03u);
        assert(MEM32(g_esp + 4) == 0x12345678u);
        assert(MEM32(g_seh_ebp - 4) == 0xFFFFFFFFu);
        longjmp(terminated, 1);
    }
}

int main(void)
{
    unsigned i;
    _set_error_mode(_OUT_TO_STDERR);
    g_xbox_mem_offset = (ptrdiff_t)memory;
    for (i = 0; i < 3; i++) {
        memset(memory, 0, sizeof memory);
        context = i * 0x1000;
        calls = 0;
        g_esp = 0x380000;
        g_ebp = g_seh_ebp = 0x380000;
        g_fs_base = 0x4000;
        g_df = 0;
        MEM32(g_fs_base + 0x28) = 0x5000;
        MEM32(0x362030) = 0xFE000102u;
        MEM32(g_esp) = 0;
        MEM32(g_esp + 4) = 0x2F75B0;
        MEM32(g_esp + 8) = context;
        if (!setjmp(terminated)) {
            sub_00083A6C();
            assert(!"bootstrap returned instead of terminating");
        }
    }
    g_esp = 0x380000;
    MEM32(g_esp) = 0x83ADEu;
    MEM32(g_esp + 4) = context;
    g_ebx = 0x87654321u;
    calls = 0;
    sub_001E7B8F();
    assert(calls == 1);
    puts("PASS: bootstrap preserves callback context/return slot and thread exit status");
    return 0;
}

