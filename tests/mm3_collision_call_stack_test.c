/* Original XBE: PUSH3 -> virtual +0x5C -> direct sub_001C7E60 -> RET12.
 * Execute the actual MM3 dispatch macros, retaining the argument block across
 * failed/resolved accessors. The following callee must consume it once only. */
#include <stdio.h>
#include <stdlib.h>
#define RECOMP_GENERATED_CODE
#include "mm3_recomp_types.h"

ptrdiff_t g_xbox_mem_offset;
uint32_t g_xbox_code_lo = 0x12000, g_xbox_code_hi = 0x260000;
RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi;
volatile uint32_t g_icall_trace[ICALL_TRACE_SIZE], g_icall_trace_idx;
volatile uint64_t g_icall_count;

static uint32_t ram[1024];
static uint32_t expected_args[3], expected_return, expected_result;
static unsigned not_code_logs, missing_logs, resolved_calls, lookup_calls;
static unsigned selected_lookup;

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #test); exit(1); \
} } while (0)

static void check_args(uint32_t sp)
{
    CHECK(MEM32(sp + 4) == expected_args[0]);
    CHECK(MEM32(sp + 8) == expected_args[1]);
    CHECK(MEM32(sp + 12) == expected_args[2]);
}

static void resolved_accessor(void)
{
    CHECK(MEM32(g_esp) == expected_return);
    check_args(g_esp);
    ++resolved_calls;
    g_eax = expected_result;
    g_esp += 4; /* Original cdecl accessor RET0; preserve PUSH3. */
}

recomp_func_t recomp_lookup_manual(uint32_t va)
{
    ++lookup_calls;
    return selected_lookup == 1 && va == 0x13000 ? resolved_accessor : NULL;
}
recomp_func_t recomp_lookup(uint32_t va)
{
    ++lookup_calls;
    return selected_lookup == 2 && va == 0x13000 ? resolved_accessor : NULL;
}
recomp_func_t recomp_lookup_kernel(uint32_t va)
{
    ++lookup_calls;
    return selected_lookup == 3 && va == 0xFE000040 ? resolved_accessor : NULL;
}
void recomp_icall_not_code_log(uint32_t va)
{
    CHECK(!RECOMP_ICALL_IS_CODE(va));
    ++not_code_logs;
}
void recomp_icall_fail_log(uint32_t va)
{
    CHECK(RECOMP_ICALL_IS_CODE(va));
    ++missing_logs;
}

static void consuming_callee(void)
{
    uint32_t entry_sp = g_esp;
    uint32_t frame = 0x12345678;
    CHECK(MEM32(entry_sp) == expected_return);
    check_args(entry_sp);
    /* Model the original standard frame and final LEAVE; RET12. */
    PUSH32(esp, frame);
    frame = g_esp;
    g_esp -= 0x10;
    g_esp = frame;
    POP32(esp, frame);
    CHECK(frame == 0x12345678);
    g_esp += 16;
    CHECK(g_esp == entry_sp + 16);
}

static void call_site(uint32_t site, unsigned mode, int caller_cleanup)
{
    uint32_t saved_sp = g_esp;
    uint32_t target = mode == 0 ? 0u : mode == 4 ? 0xFE000040u : 0x13000u;
    unsigned old_missing = missing_logs, old_noncode = not_code_logs;
    unsigned old_resolved = resolved_calls, old_lookup = lookup_calls;
    uint64_t old_calls = g_icall_count;
    expected_args[0] = 0x140u + (site & 0xFCu);
    expected_args[1] = 0x240u + (site & 0xFCu);
    expected_args[2] = 0x340u + (site & 0xFCu);
    expected_result = 0x500u + (site & 0xFCu);
    selected_lookup = mode < 2 ? 0 : mode - 1;
    PUSH32(esp, expected_args[2]);
    PUSH32(esp, expected_args[1]);
    PUSH32(esp, expected_args[0]);
    expected_return = site + 3;
    PUSH32(esp, expected_return);
    if (caller_cleanup) {
        RECOMP_ICALL_SAFE_AT_CC(target, saved_sp, site);
    } else {
        RECOMP_ICALL_SAFE_AT(target, saved_sp, site);
    }
    CHECK(g_icall_count == old_calls + 1);
    CHECK(g_icall_trace[(g_icall_trace_idx - 1) & (ICALL_TRACE_SIZE - 1)] == target);
    if (mode == 0) {
        CHECK(not_code_logs == old_noncode + 1 && missing_logs == old_missing);
        CHECK(resolved_calls == old_resolved && lookup_calls == old_lookup);
        CHECK(g_eax == 0);
    } else if (mode == 1) {
        CHECK(missing_logs == old_missing + 1 && not_code_logs == old_noncode);
        CHECK(resolved_calls == old_resolved && lookup_calls == old_lookup + 3);
        CHECK(g_eax == 0);
    } else {
        CHECK(not_code_logs == old_noncode && missing_logs == old_missing);
        CHECK(resolved_calls == old_resolved + 1);
        CHECK(lookup_calls == old_lookup + selected_lookup);
        CHECK(g_eax == expected_result);
    }
    if (!caller_cleanup && mode < 2) {
        /* Control: the old macro discarded all three arguments; RET12 would
         * pop saved registers. Demonstrate sensitivity without invalid reads. */
        CHECK(g_esp == saved_sp);
        CHECK(g_esp != saved_sp - 12);
        return;
    }
    CHECK(g_esp == saved_sp - 12);
    CHECK(MEM32(g_esp) == expected_args[0]);
    CHECK(MEM32(g_esp + 4) == expected_args[1]);
    CHECK(MEM32(g_esp + 8) == expected_args[2]);
    CHECK(g_ebx == 0x11111111 && g_esi == 0x22222222 && g_edi == 0x33333333);
    expected_return = site == 0x13FEB6 ? 0x13FEC3 : site == 0x13FEE5 ? 0x13FEF2 : 0x14E8C3;
    PUSH32(esp, expected_return);
    RECOMP_ABI_CALL(0x001C7E60u, consuming_callee);
    CHECK(g_esp == saved_sp);
}

int main(void)
{
    const uint32_t sites[] = {0x13FEB6, 0x13FEE5, 0x14E8B6};
    unsigned mode, site;
    g_xbox_mem_offset = (ptrdiff_t)ram;
    for (mode = 0; mode < 5; ++mode) {
        uint32_t frame = 0xCAFEBABE, restored;
        g_esp = 0xC00;
        g_ebx = 0x11111111; g_esi = 0x22222222; g_edi = 0x33333333;
        PUSH32(esp, frame);
        PUSH32(esp, ebx);
        PUSH32(esp, esi);
        PUSH32(esp, edi);
        for (site = 0; site < 3; ++site) {
            call_site(sites[site], mode, 1);
        }
        POP32(esp, restored); CHECK(restored == g_edi);
        POP32(esp, restored); CHECK(restored == g_esi);
        POP32(esp, restored); CHECK(restored == g_ebx);
        POP32(esp, restored); CHECK(restored == frame);
        CHECK(g_esp == 0xC00);
    }
    /* The retained block matches resolved dispatch under either macro. */
    for (site = 0; site < 3; ++site) {
        call_site(sites[site], 2, 0);
        call_site(sites[site], 0, 0);
        call_site(sites[site], 1, 0);
    }
    puts("PASS: actual MM3 call macros at three sites; noncode/unresolved/resolved manual/generated/kernel dispatch, PUSH3 reuse, RET12 once, sequential calls and saved-register sentinels");
    return 0;
}
