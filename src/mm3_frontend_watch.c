/* Optional diagnostic: observe RootLevel child-pointer writes. */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "recomp_funcs.h"

extern void sub_000FA37E_original(void);
extern void sub_000858F3_original(void);
extern intptr_t xbox_GetMemoryOffset(void);
#define MM3_ARM_WATCH 0xE0424D33u
static DWORD watch_thread;
static uint32_t watch_object, watch_value;
static volatile uint32_t *watch_field;
static unsigned watch_reports;
static PVOID watch_handler;
static int child_constructor;
static unsigned child_reports;
static int child_factory;
static unsigned factory_reports;

static void check_heap_frontier(uint32_t heap, uint32_t caller, const char *phase)
{
    static unsigned reports;
    if (reports >= 24 || heap < 0x10000u || heap >= 0x08000000u-0x580u) return;
    for (unsigned bin = 0; bin < 128; ++bin) {
        uint32_t head = heap + 0x180u + bin*8u;
        for (unsigned side = 0; side < 2; ++side) {
            uint32_t node = MEM32(head + side*4u);
            if (node == head || !node) continue;
            uint32_t next = 0, prev = 0;
            int invalid = node >= 0x08000000u-8u || (node & 7u);
            if (!invalid) {
                next = MEM32(node); prev = MEM32(node+4u);
                invalid = next >= 0x08000000u || prev >= 0x08000000u || (next & 7u) || (prev & 7u);
            }
            if (invalid && reports++ < 24)
                fprintf(stderr, "[HEAP_FRONTIER] phase=%s heap=%08X caller=%08X bin=%u side=%u node=%08X next=%08X prev=%08X thread=%u\n",
                    phase, heap, caller, bin, side, node, next, prev, GetCurrentThreadId());
        }
    }
}

void sub_000858F3(void)
{
    uint32_t heap = MEM32(g_esp+4u), caller = MEM32(g_esp);
    int trace = getenv("MM3_HEAP_FRONTIER") != NULL;
    if (trace) check_heap_frontier(heap, caller, "before");
    sub_000858F3_original();
    if (trace) check_heap_frontier(heap, caller, "after");
}

static LONG CALLBACK frontend_write_exception(EXCEPTION_POINTERS *p)
{
    CONTEXT *c = p->ContextRecord;
    if (GetCurrentThreadId() != watch_thread) return EXCEPTION_CONTINUE_SEARCH;
    if (p->ExceptionRecord->ExceptionCode == MM3_ARM_WATCH) {
        /* Do not take slot zero from an attached debugger. */
        if (c->Dr7 & 3u) return EXCEPTION_CONTINUE_EXECUTION;
        c->ContextFlags |= CONTEXT_DEBUG_REGISTERS;
        c->Dr0 = (DWORD64)(uintptr_t)watch_field;
        c->Dr7 = (c->Dr7 & ~0xF0003ull) | 0xD0001ull; /* write, four bytes */
        c->Dr6 &= ~1ull;
        fprintf(stderr, "[FRONTEND_WATCH] object=%08X child=%08X armed\n", watch_object, watch_value);
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (p->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP || !(c->Dr6 & 1u))
        return EXCEPTION_CONTINUE_SEARCH;
    uint32_t value = *watch_field;
    if (watch_reports++ < 64 || value == 0x2000u) {
        uintptr_t base = (uintptr_t)GetModuleHandleW(NULL);
        fprintf(stderr, "[FRONTEND_WRITE] object=%08X child=%08X->%08X hostRVA=%llX eax=%08X ecx=%08X edx=%08X esi=%08X edi=%08X ebp=%08X esp=%08X\n",
                watch_object, watch_value, value, (unsigned long long)(c->Rip-base),
                g_eax, g_ecx, g_edx, g_esi, g_edi, g_ebp, g_esp);
    }
    watch_value = value;
    c->Dr6 &= ~1ull;
    if (value == 0x2000u) c->Dr7 &= ~3ull;
    return EXCEPTION_CONTINUE_EXECUTION;
}

static void arm_frontend_watch(uint32_t object)
{
    static unsigned entries;
    if (entries++ < 16)
        fprintf(stderr, "[FRONTEND_WATCH_ENTRY] object=%08X enabled=%d thread=%u\n",
                object, getenv("MM3_FRONTEND_WRITE_WATCH") != NULL, GetCurrentThreadId());
    if (!getenv("MM3_FRONTEND_WRITE_WATCH") || watch_thread ||
        object < 0x10000u || object >= 0x08000000u-12u) return;
    if (!watch_handler) watch_handler = AddVectoredExceptionHandler(1, frontend_write_exception);
    if (!watch_handler) return;
    watch_object = object;
    watch_field = (volatile uint32_t *)(xbox_GetMemoryOffset() + object + 8u);
    watch_value = *watch_field;
    watch_thread = GetCurrentThreadId();
    RaiseException(MM3_ARM_WATCH, 0, 0, NULL);
}

void sub_000FA37E(void)
{
    /* Arm before the base constructor's property initialization runs. */
    arm_frontend_watch(g_edi);
    sub_000FA37E_original();
}

void mm3_frontend_watch_enter(void)
{
    arm_frontend_watch(g_edi);
}

void mm3_frontend_before_call(uint32_t va)
{
    if (va == 0x000F859Du && getenv("MM3_FRONTEND_WRITE_WATCH")) child_factory = 1;
    if (va == 0x000FFD9Eu && getenv("MM3_FRONTEND_WRITE_WATCH")) {
        static unsigned roots;
        if (roots++ < 4 && g_ecx >= 0x10000u && g_ecx < 0x08000000u-16u) {
            uint32_t vt = MEM32(g_ecx);
            if (vt >= 0x10000u && vt < 0x08000000u-24u)
                fprintf(stderr, "[FRONTEND_FACTORY] object=%08X vt=%08X target=%08X child=%08X\n",
                        g_ecx, vt, MEM32(vt+0x10u), MEM32(g_ecx+8u));
        }
    }
    if (va == 0x000FA37Eu) mm3_frontend_watch_enter();
    if (va == 0x000F7EA8u && getenv("MM3_FRONTEND_WRITE_WATCH")) {
        child_constructor = 1;
        fprintf(stderr, "[FRONTEND_CHILD_CTOR] input=%08X esp=%08X\n", g_edi, g_esp);
    }
}

void mm3_frontend_after_call(uint32_t va, uint32_t edi_before, uint32_t esp_before, uint32_t esi_before)
{
    if (va == 0x0006A965u && MEM32(esp_before) == 0x0011FB5Cu && getenv("MM3_FRONTEND_WRITE_WATCH")) {
        static unsigned reports;
        if (reports < 16 && g_eax < 0x08000000u-4u) {
            uint32_t vt = MEM32(g_eax);
            ++reports;
                fprintf(stderr, "[FRONTEND_SERVICE] input=%08X object=%08X vt=%08X target74=%08X esi=%08X edi=%08X\n",
                    esi_before, g_eax, vt, vt < 0x08000000u-0x78u ? MEM32(vt+0x74u) : 0, g_esi, g_edi);
        }
    }
    if (child_factory && g_esi != esi_before && factory_reports++ < 64)
        fprintf(stderr, "[FRONTEND_FACTORY_CALL] va=%08X esi=%08X->%08X edi=%08X->%08X esp=%08X->%08X eax=%08X ebp=%08X\n",
                va, esi_before, g_esi, edi_before, g_edi, esp_before, g_esp, g_eax, g_ebp);
    if (child_constructor && g_edi != edi_before && child_reports++ < 64)
        fprintf(stderr, "[FRONTEND_CHILD_CALL] va=%08X edi=%08X->%08X esp=%08X->%08X eax=%08X ebp=%08X\n",
                va, edi_before, g_edi, esp_before, g_esp, g_eax, g_ebp);
    if (va == 0x000F7EA8u && child_constructor) {
        fprintf(stderr, "[FRONTEND_CHILD_CTOR] result=%08X edi=%08X\n", g_eax, g_edi);
        child_constructor = 0;
    }
    if (va == 0x000F859Du) child_factory = 0;
}
