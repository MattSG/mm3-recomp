/* Optional diagnostic: observe RootLevel child-pointer writes. */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "recomp_funcs.h"

extern void sub_000FA37E_original(void);
extern void sub_000858F3_original(void);
extern void sub_0011935B_original(void);
extern void sub_001253F4_original(void);
extern void sub_0008E420_original(void);
extern void sub_0008E3C0_original(void);

void sub_0008E3C0(void)
{
    uint32_t header = MEM32(g_esp + 4u), caller = MEM32(g_esp);
    uint32_t total = MEM32(0x003BDF78u), count = MEM32(0x003BDF7Cu);
    uint32_t outputs = MEM32(0x00392FF8u), sizes = MEM32(0x00392FFCu);
    int trace = getenv("MM3_BINK_ALLOC_TRACE") != NULL;
    sub_0008E3C0_original();
    if (trace) {
        int invalid = g_eax && g_eax < 0x00500000u;
        fprintf(stderr, "[BINK_LAYOUT] caller=%08X header=%u total=%u count=%u result=%08X outputs=%08X sizes=%08X\n",
                caller, header, total, count, g_eax, outputs, sizes);
        if (outputs >= 0x10000u && outputs < 0x08000000u - 256u &&
            sizes >= 0x10000u && sizes < 0x08000000u - 256u)
            for (uint32_t i = 0; i < count && i < 64; ++i) {
                uint32_t output = MEM32(outputs + i*4u);
                uint32_t value = output >= 0x10000u && output < 0x08000000u-4u ? MEM32(output) : 0;
                fprintf(stderr, "[BINK_LAYOUT_FIELD] index=%u output=%08X size=%u value=%08X\n",
                        i, output, MEM32(sizes + i*4u), value);
                if (value && value < 0x00500000u) invalid = 1;
            }
        if (invalid && getenv("RECOMP_BINK_ALLOC_BREAK")) DebugBreak();
    }
}

void sub_0008E420(void)
{
    uint32_t size = MEM32(g_esp + 4u), caller = MEM32(g_esp);
    uint32_t callback = MEM32(0x003BDF9Cu), before = g_esp;
    int trace = getenv("MM3_BINK_ALLOC_TRACE") != NULL;
    sub_0008E420_original();
    if (trace) {
        static RECOMP_TLS unsigned reports;
        int invalid = g_eax && g_eax < 0x00500000u;
        if (reports++ < 64 || invalid)
            fprintf(stderr, "[BINK_ALLOC] size=%u caller=%08X callback=%08X result=%08X esp=%08X->%08X\n",
                    size, caller, callback, g_eax, before, g_esp);
        if (invalid && getenv("RECOMP_BINK_ALLOC_BREAK"))
            DebugBreak();
    }
}
void mm3_frontend_before_call(uint32_t va);
void mm3_frontend_after_call(uint32_t va, uint32_t edi_before, uint32_t esp_before, uint32_t esi_before);
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
static RECOMP_TLS uint32_t menu_owner;
static RECOMP_TLS uint32_t menu_root;
static RECOMP_TLS int menu_loading;
static RECOMP_TLS uint32_t menu_virtual_target;
static RECOMP_TLS uint32_t menu_size_target;
static RECOMP_TLS struct { uint32_t va, calls; } unresolved_stubs[512];
static RECOMP_TLS unsigned unresolved_stub_count;

void mm3_frontend_unresolved_stub(uint32_t va)
{
    unsigned i;
    uint32_t count;
    if (!getenv("MM3_UNRESOLVED_STUB_TRACE")) return;
    for (i = 0; i < unresolved_stub_count; ++i)
        if (unresolved_stubs[i].va == va) break;
    if (i == unresolved_stub_count) {
        if (i == 512) return;
        unresolved_stubs[i].va = va;
        ++unresolved_stub_count;
    }
    count = ++unresolved_stubs[i].calls;
    if (count == 1 || (count >= 1024 && !(count & (count-1u))))
        fprintf(stderr, "[UNRESOLVED_STUB] va=%08X count=%u caller=%08X eax=%08X ecx=%08X esp=%08X thread=%u\n", va, count,
                g_esp >= 0x10000u && g_esp < 0x08000000u-4u ? MEM32(g_esp) : 0,
                g_eax, g_ecx, g_esp, GetCurrentThreadId());
}

void mm3_frontend_icall_site(uint32_t va, uint32_t site)
{
    if (site == 0x0011BA84u && getenv("MM3_HEAP_FRONTIER")) {
        menu_size_target = va;
        fprintf(stderr, "[MENU_SIZE_SOURCE] object=%08X vt=%08X target=%08X esp=%08X\n", g_ebx, MEM32(g_ebx), va, g_esp);
    }
    if (site == 0x001C3A79u && getenv("MM3_HEAP_FRONTIER")) {
        menu_virtual_target = va;
        fprintf(stderr, "[MENU_FACTORY] factory=%08X vt=%08X target=%08X esp=%08X frame=%08X\n", g_ebx, MEM32(g_ebx), va, g_esp, g_ebp);
    }
    if ((site == 0x001C36B1u || site == 0x001C403Bu) && menu_loading) {
        uint32_t object = g_esi >= 0x10000u && g_esi < 0x08000000u-4u ? MEM32(g_esi) : 0;
        menu_virtual_target = va;
        fprintf(stderr, "[MENU_BIND] site=%08X target=%08X object=%08X vt=%08X ctx=%08X esi=%08X esp=%08X\n",
            site, va, object, object >= 0x10000u && object < 0x08000000u-4u ? MEM32(object) : 0, MEM32(g_esp+4u), g_esi, g_esp);
    }
}

void sub_0011935B(void)
{
    uint32_t di = g_edi, sp = g_esp, si = g_esi;
    mm3_frontend_before_call(0x0011935Bu);
    sub_0011935B_original();
    mm3_frontend_after_call(0x0011935Bu, di, sp, si);
}

void sub_001253F4(void)
{
    fprintf(stderr, "[FRONTEND_INIT] object=%08X caller=%08X\n", g_ecx, MEM32(g_esp));
    sub_001253F4_original();
    fprintf(stderr, "[FRONTEND_INIT] returned eax=%08X\n", g_eax);
}

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
    uint32_t size = MEM32(g_esp+12u);
    int trace = getenv("MM3_HEAP_FRONTIER") != NULL;
    if (trace && (heap < 0x10000u || (heap & 0xFFFu)))
        fprintf(stderr, "[HEAP_INVALID_ARGUMENT] heap=%08X caller=%08X esp=%08X ebp=%08X seh=%08X args=%08X,%08X,%08X,%08X\n",
            heap, caller, g_esp, g_ebp, g_seh_ebp, MEM32(g_esp+4u), MEM32(g_esp+8u), MEM32(g_esp+12u), MEM32(g_esp+16u));
    if (trace) check_heap_frontier(heap, caller, "before");
    sub_000858F3_original();
    if (g_eax && g_eax < 0x00500000u &&
        (trace || getenv("MM3_HEAP_RETURN_TRACE"))) {
        static LONG return_breaks;
        fprintf(stderr, "[HEAP_RETURN_INVALID] heap=%08X caller=%08X size=%u result=%08X esp=%08X\n",
                heap, caller, size, g_eax, g_esp);
        if (getenv("RECOMP_HEAP_RETURN_BREAK") &&
            InterlockedCompareExchange(&return_breaks, 1, 0) == 0) DebugBreak();
    }
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
    static unsigned string_calls[2];
    static unsigned erase_copies;
    if (va == 0x00093860u && MEM32(g_esp) == 0x00012829u && MEM32(g_esp+12u) && getenv("MM3_HEAP_FRONTIER") && erase_copies++ < 10)
        fprintf(stderr, "[MENU_ERASE_COPY] esp=%08X frame=%08X dst=%08X src=%08X count=%08X\n", g_esp, g_ebp, MEM32(g_esp+4u), MEM32(g_esp+8u), MEM32(g_esp+12u));
    if ((va == 0x000127D6u || va == 0x00018B86u) && getenv("MM3_HEAP_FRONTIER") && string_calls[va == 0x00018B86u]++ < 20)
        fprintf(stderr, "[MENU_STRING_BEGIN] va=%08X caller=%08X esp=%08X ebp=%08X ecx=%08X args=%08X,%08X,%08X\n", va, MEM32(g_esp), g_esp, g_ebp, g_ecx, MEM32(g_esp+4u), MEM32(g_esp+8u), MEM32(g_esp+12u));
    if (menu_loading && va == 0x001C4044u) {
        fprintf(stderr, "[MENU_REF] source=%08X input=%08X old=%08X input_vt=%08X\n",
            g_ecx, g_eax, MEM32(g_ecx), g_eax >= 0x10000u && g_eax < 0x08000000u-4u ? MEM32(g_eax) : 0);
    }
    if (getenv("MM3_FRONTEND_WRITE_WATCH") && (va == 0x0011935Bu || va == 0x001C5E11u)) {
        uint32_t name = MEM32(g_esp + (va == 0x0011935Bu ? 8u : 4u));
        if (va == 0x0011935Bu) { menu_owner = g_ecx; menu_loading = 1; }
        else menu_root = g_ecx;
        fprintf(stderr, "[MENU_LOAD_BEGIN] va=%08X owner=%08X eax=%08X name=%08X %.96s\n",
                va, g_ecx, g_eax, name,
                name >= 0x10000u && name < 0x08000000u-96u ? (const char *)(xbox_GetMemoryOffset()+name) : "invalid");
    }
    if (va == 0x001C27E0u && MEM32(g_esp+8u) == 0x003A1124u && getenv("MM3_FRONTEND_WRITE_WATCH")) {
        static unsigned lookups;
        if (lookups++ < 8 && g_ecx >= 0x10000u && g_ecx < 0x08000000u-16u) {
            uint32_t inner = MEM32(g_ecx+8u);
            uint32_t inner_vt = inner >= 0x10000u && inner < 0x08000000u-4u ? MEM32(inner) : 0;
            fprintf(stderr, "[FRONTEND_LOOKUP_INPUT] manager=%08X parent=%08X inner=%08X prefix=%08X inner_vt=%08X inner40=%08X\n",
                g_ecx, MEM32(g_ecx+4u), inner, MEM32(g_ecx+12u), inner_vt,
                inner_vt >= 0x10000u && inner_vt < 0x08000000u-0x44u ? MEM32(inner_vt+0x40u) : 0);
        }
    }
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
    if (menu_size_target && menu_size_target == va) {
        fprintf(stderr, "[MENU_SIZE_RETURN] target=%08X size=%08X esp=%08X->%08X\n", va, g_eax, esp_before, g_esp);
        menu_size_target = 0;
    }
    static unsigned string_returns[2];
    static unsigned erase_returns;
    if (va == 0x00093860u && g_esp != esp_before+4u && getenv("MM3_HEAP_FRONTIER") && erase_returns++ < 10)
        fprintf(stderr, "[MENU_ERASE_COPY_END] esp=%08X->%08X eax=%08X esi=%08X->%08X edi=%08X->%08X\n", esp_before, g_esp, g_eax, esi_before, g_esi, edi_before, g_edi);
    if ((va == 0x000127D6u || va == 0x00018B86u) && getenv("MM3_HEAP_FRONTIER") && string_returns[va == 0x00018B86u]++ < 20)
        fprintf(stderr, "[MENU_STRING_END] va=%08X esp=%08X->%08X eax=%08X esi=%08X->%08X edi=%08X->%08X\n", va, esp_before, g_esp, g_eax, esi_before, g_esi, edi_before, g_edi);
    if (menu_virtual_target && menu_virtual_target == va) {
        if (getenv("MM3_HEAP_FRONTIER"))
            fprintf(stderr, "[MENU_FACTORY_RETURN] target=%08X object=%08X vt=%08X\n", va, g_eax, g_eax >= 0x10000u && g_eax < 0x08000000u-4u ? MEM32(g_eax) : 0);
        fprintf(stderr, "[MENU_BIND_END] target=%08X esp=%08X->%08X eax=%08X\n", va, esp_before, g_esp, g_eax);
        menu_virtual_target = 0;
    }
    if (menu_loading && (g_esi != esi_before || g_edi != edi_before || va == 0x001C369Eu || va == 0x001C5D15u || va == 0x001C4044u || va == 0x001C4851u)) {
        static unsigned menu_calls;
        if (menu_calls++ < 80)
            fprintf(stderr, "[MENU_CALL] va=%08X esi=%08X->%08X edi=%08X->%08X esp=%08X->%08X eax=%08X\n",
                va, esi_before, g_esi, edi_before, g_edi, esp_before, g_esp, g_eax);
    }
    if (getenv("MM3_FRONTEND_WRITE_WATCH") && (va == 0x0011935Bu || va == 0x001C5E11u)) {
        uint32_t root = va == 0x0011935Bu ? MEM32(menu_owner+0xCu) : menu_root;
        if (root >= 0x10000u && root < 0x08000000u-0x50u)
            fprintf(stderr, "[MENU_LOAD_END] va=%08X root=%08X vt=%08X node=%08X scope=%08X r48=%08X r4c=%08X eax=%08X\n",
                    va, root, MEM32(root), MEM32(root+8u), MEM32(root+0x30u), MEM32(root+0x48u), MEM32(root+0x4Cu), g_eax);
        if (va == 0x0011935Bu) menu_loading = 0;
    }
    if (va == 0x0006A965u && MEM32(esp_before) == 0x0011FB5Cu && getenv("MM3_FRONTEND_WRITE_WATCH")) {
        static unsigned reports;
        if (reports < 16 && g_eax < 0x08000000u-4u) {
            uint32_t vt = MEM32(g_eax);
            ++reports;
                fprintf(stderr, "[FRONTEND_SERVICE] input=%08X object=%08X vt=%08X target74=%08X esi=%08X edi=%08X\n",
                    esi_before, g_eax, vt, vt < 0x08000000u-0x78u ? MEM32(vt+0x74u) : 0, g_esi, g_edi);
            if (g_edi >= 0x10000u && g_edi < 0x08000000u-0x10u) {
                uint32_t root = MEM32(g_edi+0xCu);
                if (root >= 0x10000u && root < 0x08000000u-0x34u) {
                    uint32_t manager = MEM32(root+0x30u);
                    if (manager >= 0x10000u && manager < 0x08000000u-4u) {
                        uint32_t manager_vt = MEM32(manager);
                        if (manager_vt >= 0x10000u && manager_vt < 0x08000000u-0x44u)
                            fprintf(stderr, "[FRONTEND_LOOKUP] root=%08X manager=%08X vt=%08X target40=%08X\n",
                                root, manager, manager_vt, MEM32(manager_vt+0x40u));
                    }
                }
            }
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
