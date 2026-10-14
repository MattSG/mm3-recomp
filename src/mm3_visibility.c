/* The plane-list/OBB visibility test was a measured guest CPU hotspot.
 * Its leaf body has a statically balanced x87 stack: keep those seven scratch
 * slots in native scalar locals, instead of rotating the TLS stack on every
 * arithmetic instruction. The operation order, precision-control rounding,
 * guest scratch writes, status word and register/stack effects remain exact.
 * Do not enable fast-math or replace the ordered sums with dot-product/FMA.
 * tests/mm3_visibility_regression.py compares the complete machine state with
 * the generated Xbox function, including all eight (also popped) x87 slots.
 */
#include "recomp_types.h"

void sub_001C793A(void)
{
    const unsigned top = (unsigned)g_fp_top;
    double f1 = g_fp_stack[(top + 1) & 7u];
    double f2 = g_fp_stack[(top + 2) & 7u];
    double f3 = g_fp_stack[(top + 3) & 7u];
    double f4 = g_fp_stack[(top + 4) & 7u];
    double f5 = g_fp_stack[(top + 5) & 7u];
    double f6, f7;
    uint32_t frame = g_ebp, count;

    PUSH32(g_esp, frame);
    frame = g_esp;
    g_ebp = g_seh_ebp = frame;
    g_esp -= 0x34;
    f7 = MEMF(g_ecx + 0x30);
    PUSH32(g_esp, g_esi);
    g_esi = 0;
    count = MEM32(frame + 8);

    /* Scale the three oriented axes by the box half-extents. Keep the float
     * stores/reloads: the guest rounds these before the per-plane products. */
    f6 = RECOMP_FP_PC(f7 * MEMF(g_ecx));
    MEMF(frame - 52) = (float)f6;
    f6 = RECOMP_FP_PC(f7 * MEMF(g_ecx + 4));
    MEMF(frame - 48) = (float)f6;
    f7 = RECOMP_FP_PC(f7 * MEMF(g_ecx + 8));
    MEMF(frame - 44) = (float)f7;
    f7 = MEMF(g_ecx + 0x34);
    f6 = RECOMP_FP_PC(f7 * MEMF(g_ecx + 0xC));
    MEMF(frame - 40) = (float)f6;
    f6 = RECOMP_FP_PC(f7 * MEMF(g_ecx + 0x10));
    MEMF(frame - 36) = (float)f6;
    f7 = RECOMP_FP_PC(f7 * MEMF(g_ecx + 0x14));
    MEMF(frame - 32) = (float)f7;
    f7 = MEMF(g_ecx + 0x38);
    f6 = RECOMP_FP_PC(f7 * MEMF(g_ecx + 0x18));
    MEMF(frame - 28) = (float)f6;
    f6 = RECOMP_FP_PC(f7 * MEMF(g_ecx + 0x1C));
    MEMF(frame - 24) = (float)f6;
    f7 = RECOMP_FP_PC(f7 * MEMF(g_ecx + 0x20));
    MEMF(frame - 20) = (float)f7;
    if (!count) goto accepted;

    do {
        f7 = MEMF(g_edx);
        MEMF(frame - 16) = (float)f7;
        f6 = MEMF(g_edx + 4);
        MEMF(frame - 12) = (float)f6;
        f5 = MEMF(g_edx + 8);
        MEMF(frame - 8) = (float)f5;

        /* Signed plane distance of the center, in the retail Z,Y,X order. */
        f4 = RECOMP_FP_PC(f5 * MEMF(g_ecx + 0x2C));
        f3 = RECOMP_FP_PC(f6 * MEMF(g_ecx + 0x28));
        f4 = RECOMP_FP_PC(f4 + f3);
        f3 = RECOMP_FP_PC(f7 * MEMF(g_ecx + 0x24));
        f4 = RECOMP_FP_PC(f4 + f3);
        f4 = RECOMP_FP_PC(f4 + MEMF(g_edx + 0xC));

        /* Projected radius: sum the absolute projections of each box axis.
         * f1..f7 retain the exact final contents of the guest scratch slots. */
        f3 = RECOMP_FP_PC(f5 * MEMF(frame - 20));
        f2 = RECOMP_FP_PC(f6 * MEMF(frame - 24));
        f3 = RECOMP_FP_PC(f3 + f2);
        f2 = RECOMP_FP_PC(f7 * MEMF(frame - 28));
        f3 = RECOMP_FP_PC(f3 + f2);
        f3 = fabs(f3);
        f7 = f3;
        f2 = RECOMP_FP_PC((double)MEMF(frame - 8) * MEMF(frame - 32));
        f1 = RECOMP_FP_PC((double)MEMF(frame - 12) * MEMF(frame - 36));
        f2 = RECOMP_FP_PC(f2 + f1);
        f1 = RECOMP_FP_PC((double)MEMF(frame - 16) * MEMF(frame - 40));
        f2 = RECOMP_FP_PC(f2 + f1);
        f2 = fabs(f2);
        MEMF(frame - 4) = (float)f2;
        f3 = RECOMP_FP_PC(f3 + f2);
        f2 = RECOMP_FP_PC((double)MEMF(frame - 8) * MEMF(frame - 44));
        f1 = RECOMP_FP_PC((double)MEMF(frame - 12) * MEMF(frame - 48));
        f2 = RECOMP_FP_PC(f2 + f1);
        f1 = RECOMP_FP_PC((double)MEMF(frame - 16) * MEMF(frame - 52));
        f2 = RECOMP_FP_PC(f2 + f1);
        f2 = fabs(f2);
        MEMF(frame - 4) = (float)f2;
        f3 = RECOMP_FP_PC(f3 + f2);
        f3 = -f3;
        g_fp_cmp = RECOMP_FCMP(f3, f4);
        g_fp_cc = RECOMP_FCMP_CC(g_fp_cmp);
        g_eax = (g_eax & 0xFFFF0000u) |
            (uint16_t)((((top + 6) & 7u) << 11) | g_fp_cc);
        if (!RECOMP_PARITY8(HI8(g_eax) & 0x41)) {
            SET_LO8(g_eax, 0);
            goto finished;
        }
        ++g_esi;
        g_edx += 0x10;
    } while (g_esi < MEM32(frame + 8));

accepted:
    SET_LO8(g_eax, 1);
finished:
    POP32(g_esp, g_esi);
    g_esp = frame;
    POP32(g_esp, frame);
    g_esp += 8; /* ret 4 */
    g_fp_stack[(top + 1) & 7u] = f1;
    g_fp_stack[(top + 2) & 7u] = f2;
    g_fp_stack[(top + 3) & 7u] = f3;
    g_fp_stack[(top + 4) & 7u] = f4;
    g_fp_stack[(top + 5) & 7u] = f5;
    g_fp_stack[(top + 6) & 7u] = f6;
    g_fp_stack[(top + 7) & 7u] = f7;
}
