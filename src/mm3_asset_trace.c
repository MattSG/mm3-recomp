#include <stdio.h>
#include <stdlib.h>
#include "recomp_types.h"

void sub_00170E5D_original(void);
void sub_000145DC_original(void);
void sub_001F02AD_original(void);
void mm3_stream_read_copy(void);

static int asset_trace_enabled(void)
{
    static int initialized, enabled;
    if (!initialized) {
        const char *value = getenv("MM3_ASSET_TRACE");
        enabled = value && *value && *value != '0';
        initialized = 1;
    }
    return enabled;
}

void mm3_asset_math_trace(uint32_t va, int after, uint32_t original_sp)
{
    static RECOMP_TLS unsigned reports;
    static RECOMP_TLS unsigned depth;
    if (!asset_trace_enabled()) return;
    if (va == 0x25371Cu && !after) ++depth;
    if (!depth && va != 0x1CCD2Au) return;
    if (va != 0x9B8C2u && va != 0x9B3DCu && va != 0x9BB15u &&
        va != 0x9BC52u && va != 0x9B94Eu && va != 0x1CCD2Au &&
        va != 0x25371Cu && va != 0x1CB15Fu) return;
    if (reports < 160) {
    ++reports;
    fprintf(stderr, "[ASSET_MATH_%s] va=%08X esp=%08X before=%08X ebp=%08X seh=%08X ebx=%08X esi=%08X edi=%08X\n",
            after ? "END" : "BEGIN", va, g_esp, original_sp, g_ebp, g_seh_ebp, g_ebx, g_esi, g_edi);
    }
    if (va == 0x25371Cu && after) --depth;
}

static void trace_copy(uint32_t va, void (*body)(void), unsigned cleanup)
{
    uint32_t sp = g_esp, bx = g_ebx, si = g_esi, di = g_edi;
    int trace = asset_trace_enabled() &&
        ((va == 0x145DCu && MEM32(sp) == 0x170E7Eu) ||
         (va == 0x1F02ADu && MEM32(sp) == 0x14617u));
    if (trace) {
        fprintf(stderr, "[ASSET_COPY_BEGIN] va=%08X caller=%08X eax=%08X ecx=%08X edx=%08X arg=%08X esi=%08X edi=%08X esp=%08X\n",
            va, MEM32(sp), g_eax, g_ecx, g_edx, MEM32(sp + 4u), si, di, sp);
        fflush(stderr);
    }
    body();
    if (trace && (g_esp != sp + cleanup || g_ebx != bx || g_esi != si || g_edi != di)) {
        fprintf(stderr, "[ASSET_COPY_ABI] va=%08X esp=%08X expected=%08X ebx=%08X/%08X esi=%08X/%08X edi=%08X/%08X\n",
            va, g_esp, sp + cleanup, g_ebx, bx, g_esi, si, g_edi, di);
        fflush(stderr);
    }
}

void sub_000145DC(void)
{
    static int initialized, original;
    if (!initialized) {
        const char *value = getenv("RECOMP_STREAM_COPY_ORIGINAL");
        original = value && *value && *value != '0';
        initialized = 1;
    }
    trace_copy(0x145DCu, original ? sub_000145DC_original : mm3_stream_read_copy, 12);
}
void sub_001F02AD(void) { trace_copy(0x1F02ADu, sub_001F02AD_original, 8); }

void mm3_asset_small_copy_begin(void)
{
    if (asset_trace_enabled() && MEM32(g_esp) == 0x14631u) {
        fprintf(stderr, "[ASSET_SMALL_BEGIN] caller=%08X dest=%08X src=%08X blocks=%08X esp=%08X ebp=%08X seh=%08X\n",
            MEM32(g_esp), g_ecx, g_edx, MEM32(g_esp + 4u), g_esp, g_ebp, g_seh_ebp);
        fflush(stderr);
    }
}

void mm3_asset_small_copy_end(uint32_t sp, uint32_t si, uint32_t di)
{
    if (asset_trace_enabled() && MEM32(sp) == 0x14631u) {
        fprintf(stderr, "[ASSET_SMALL_END] esp=%08X expected=%08X esi=%08X/%08X edi=%08X/%08X ebp=%08X seh=%08X\n",
            g_esp, sp + 8u, g_esi, si, g_edi, di, g_ebp, g_seh_ebp);
        fflush(stderr);
    }
}

/* Optional trace of the reader seam used by Cruise text-resource loading.
 * Preserve the original execution, including its allocation and read calls. */
void sub_00170E5D(void)
{
    uint32_t reader = g_eax;
    uint32_t caller = MEM32(g_esp);
    int trace = asset_trace_enabled() &&
        caller >= 0x0021ED64u && caller < 0x002202EEu;
    if (trace) {
        uint32_t stream = MEM32(reader);
        uint32_t table = MEM32(stream);
        fprintf(stderr, "[ASSET_READER_BEGIN] caller=%08X reader=%08X stream=%08X vt=%08X read=%08X size=%08X esi=%08X edi=%08X esp=%08X\n",
            caller, reader, stream, table, MEM32(table + 4u),
            MEM32(table + 8u), g_esi, g_edi, g_esp);
        fflush(stderr);
    }
    sub_00170E5D_original();
    if (trace) {
        fprintf(stderr, "[ASSET_READER_END] reader=%08X data=%08X end=%08X cursor=%08X esi=%08X edi=%08X esp=%08X\n",
            reader, MEM32(reader + 4u), MEM32(reader + 8u),
            MEM32(reader + 12u), g_esi, g_edi, g_esp);
        fflush(stderr);
    }
}
