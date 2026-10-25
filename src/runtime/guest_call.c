#include "guest_call.h"
#include "guest_state.h"
#include "xbox_memory_layout.h"

extern RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi, g_ebp;

static uint32_t guest32(uint32_t va)
{
    return *(volatile uint32_t *)((uintptr_t)va + (uintptr_t)xbox_GetMemoryOffset());
}

static void guest32_store(uint32_t va, uint32_t value)
{
    *(volatile uint32_t *)((uintptr_t)va + (uintptr_t)xbox_GetMemoryOffset()) = value;
}

void xbox_guest_call_repeated(void (*fn)(void), int n, int nargs, uint32_t ret_pop)
{
    uint32_t esp0 = g_esp, saved[8], eax = g_eax, ecx = g_ecx, edx = g_edx;
    uint32_t ebx = g_ebx, esi = g_esi, edi = g_edi, ebp = g_ebp;
    int i, a;
    for (a = 0; a <= nargs && a < 8; a++)
        saved[a] = guest32(esp0 + 4u * a);
    for (i = 0; i < n; i++) {
        g_esp = esp0;
        for (a = 0; a <= nargs && a < 8; a++)
            guest32_store(esp0 + 4u * a, saved[a]);
        g_eax = eax; g_ecx = ecx; g_edx = edx;
        g_ebx = ebx; g_esi = esi; g_edi = edi; g_ebp = ebp;
        fn();
    }
    if (n == 0) {
        g_eax = eax; g_ebx = ebx; g_esi = esi; g_edi = edi; g_ebp = ebp;
    }
    g_esp = esp0 + ret_pop;
}
