#include "crash_report.h"
#include "guest_state.h"
#include "xbox_memory_layout.h"
#include <stdint.h>
#include <stdio.h>

extern RECOMP_TLS uint32_t g_eax, g_ecx, g_ebx, g_esp, g_esi, g_edi, g_ebp, g_seh_ebp, g_fs_base;

/* Reads a guest word, or returns 0 when the address is not guest memory. */
static uint32_t crash_guest32(uint32_t va)
{
    uint32_t value = 0;
    if (!xbox_GuestRead(va, &value, sizeof value)) return 0;
    return value;
}

LONG xbox_CrashReport(EXCEPTION_POINTERS *info)
{
    const EXCEPTION_RECORD *record = info ? info->ExceptionRecord : NULL;
    const CONTEXT *context = info ? info->ContextRecord : NULL;
    ULONG_PTR fault = record && record->NumberParameters > 1
        ? record->ExceptionInformation[1] : 0;
    uintptr_t module_base = (uintptr_t)GetModuleHandleW(NULL);
    uint32_t mapped = (uint32_t)xbox_GetMappedSize();
    fprintf(stderr, "[CRASH] code=0x%08lX address=%p guest_esp=0x%08X eax=0x%08X\n",
            record ? record->ExceptionCode : 0,
            record ? record->ExceptionAddress : NULL,
            g_esp, g_eax);
    fprintf(stderr, "[CRASH] fault=%p rip=%p rsp=%p rax=%llX rdx=%llX\n",
            (void *)fault,
            context ? (void *)context->Rip : NULL,
            context ? (void *)context->Rsp : NULL,
            context ? context->Rax : 0,
            context ? context->Rdx : 0);
    {
        void *frames[16];
        USHORT count = CaptureStackBackTrace(0, 16, frames, NULL);
        USHORT i;
        for (i = 0; i < count; ++i)
            fprintf(stderr, "[CRASH_NATIVE] frame=%u rva=0x%llX\n",
                    (unsigned)i,
                    (unsigned long long)((uintptr_t)frames[i] - module_base));
        if (g_ebp >= 0x10000u && g_ebp < mapped - 8u)
            fprintf(stderr, "[CRASH_CALLER] frame=%08X parent=%08X return=%08X\n",
                    g_ebp, crash_guest32(g_ebp), crash_guest32(g_ebp + 4u));
    }
    fprintf(stderr, "[CRASH] module=%p rip_rva=0x%llX\n",
            (void *)module_base,
            context ? (unsigned long long)((uintptr_t)context->Rip - module_base) : 0);
    fprintf(stderr, "[CRASH] xbox_mem_offset=%lld memory_base=%p\n",
            (long long)xbox_GetMemoryOffset(), xbox_GetMemoryBase());
    fprintf(stderr, "[CRASH] guest fs=0x%08X ebp=0x%08X seh_ebp=0x%08X ecx=0x%08X ebx=0x%08X esi=0x%08X edi=0x%08X\n",
            g_fs_base, g_ebp, g_seh_ebp, g_ecx, g_ebx, g_esi, g_edi);
    fprintf(stderr, "[CRASH] stack=0x%08X +4=0x%08X +8=0x%08X\n",
            crash_guest32(g_esp), crash_guest32(g_esp + 4), crash_guest32(g_esp + 8));
    fflush(stderr);
    return EXCEPTION_CONTINUE_SEARCH;
}
