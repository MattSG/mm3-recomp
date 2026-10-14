#include "recomp_types.h"

/* This CRT helper tests EFLAGS.ID, then CPUID.1:EDX.MMX. Both are supported
 * by the Xbox processor. Recover that result without the untranslated flag
 * instructions or querying the host processor.
 *
 * The generated ABI publishes each standard frame through g_ebp/g_seh_ebp,
 * even after returning. Keep that bridge and the original saved stack words:
 * later Bink functions inherit it. Fresh probes also expose CPUID's ECX/EDX.
 * Bink's flag must follow its original argument; skipping its optimized
 * initialization leaves its later conversion callbacks uninitialized. */
void sub_00093770(void)
{
    uint32_t entry_sp = g_esp;
    uint32_t frame = entry_sp - 4u;
    uint32_t available = MEM32(0x00393004u);

    MEM32(frame) = g_ebp;
    MEM32(frame - 4u) = g_ecx;
    MEM32(frame - 8u) = g_ebx;
    g_ebp = frame;
    g_seh_ebp = frame;
    if ((int32_t)available < 0) {
        uint32_t cpuid_a, cpuid_b;
        MEM32(frame - 12u) = g_ebx;
        recomp_cpuid(1u, g_ecx, &cpuid_a, &cpuid_b, &g_ecx, &g_edx);
        available = (g_edx & 0x00800000u) != 0u;
        MEM32(frame - 4u) = available;
        MEM32(0x00393004u) = available;
    }
    g_eax = available;
    MEM32(0x0046E580u) = available != 0u &&
                           (MEM32(entry_sp + 4u) & 0x00040000u) == 0u;
    g_esp = entry_sp + 4u; /* RET; caller owns the Bink flag argument. */
}
