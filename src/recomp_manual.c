#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <xmmintrin.h>
#include <emmintrin.h>
#include <string.h>
#include "platform/xbox_winnt.h"

#include "recomp_types.h"
#include "mods/mods.h"

typedef void (*recomp_func_t)(void);

static int g_mm3_unimpl_trap;
static int g_mm3_frontend_icall_trace;
static int g_mm3_stream_trace;

/* Configure before guest workers start. cli/sti occur in tight polling loops;
 * querying the CRT environment on each no-op serializes those workers. */
void mm3_configure_diagnostics(void)
{
    const char *trap = getenv("RECOMP_UNIMPL_TRAP");
    g_mm3_unimpl_trap = trap && *trap && *trap != '0';
    g_mm3_frontend_icall_trace = getenv("MM3_FRONTEND_ICALL_TRACE") != NULL;
    g_mm3_stream_trace = getenv("MM3_STREAM_TRACE") != NULL;
    /* The building trace includes the runtime's async-wait trace unless that
     * is set itself. */
    {
        const char *building = getenv("MM3_BUILDING_TRACE");
        if (building && *building && *building != '0' && !getenv("RECOMP_ASYNC_WAIT_TRACE"))
            _putenv_s("RECOMP_ASYNC_WAIT_TRACE", "1");
    }
}

static void recomp_unimpl_known(volatile LONG *noted, const char *text, uint32_t va,
                                const char *why)
{
    if (InterlockedCompareExchange(noted, 1, 0) == 0)
        fprintf(stderr, "[UNIMPL] known no-op `%s` at 0x%08X: %s\n", text, va, why);
}

void recomp_unimpl(const char *text, uint32_t va)
{
    static volatile LONG noted[4];
    int stop = g_mm3_unimpl_trap;

    if (!stop) {
        /* These exact sites operate Xbox hardware which the native backends
         * already replace. Interrupt masking around the APU clock sample
         * cannot mask Windows interrupts; host port 0x80C0 is not Xbox PCI.
         * Keep unknown instructions visible and retain the diagnostic trap.
         * Each known site is logged once at its first hit, then stays a no-op. */
        if (va == 0x0035F882u && strcmp(text, "cli") == 0) {
            recomp_unimpl_known(&noted[0], text, va,
                                "APU clock sample interrupt masking, no-op on the host");
            return;
        }
        if (va == 0x0035F893u && strcmp(text, "sti") == 0) {
            recomp_unimpl_known(&noted[1], text, va,
                                "APU clock sample interrupt masking, no-op on the host");
            return;
        }
        if (va == 0x003475DCu && strcmp(text, "out dx, al") == 0 &&
            (g_edx & 0xFFFFu) == 0x80C0u) {
            recomp_unimpl_known(&noted[2], text, va,
                                "host port 0x80C0 is not Xbox PCI, no-op");
            return;
        }
        if (va == 0x00345E9Du && strcmp(text, "wbinvd") == 0) {
            /* Guest memory is coherent host RAM. Publish earlier writes to
             * the native GPU consumer without flushing host CPU caches. */
            _mm_mfence();
            recomp_unimpl_known(&noted[3], text, va,
                                "guest memory is coherent host RAM, mfence in place");
            return;
        }
    }
    fprintf(stderr,
            "[UNIMPL] untranslated instruction REACHED: `%s` at 0x%08X; stopping%s\n",
            text, va, stop ? " (RECOMP_UNIMPL_TRAP=1)" : "");
    fflush(stderr);
    abort();
}

extern void sub_000838C3(void);
extern void sub_00097AA4(void);
extern void sub_0009492B(void);
extern void sub_00170EEE(void);
extern void sub_00012000(void);
extern void sub_00012731(void);
extern void sub_000127A9(void);
extern void sub_00078D64(void);
void sub_00021103(void);
void sub_0007169D(void);
void sub_00012850(void);
void sub_00043E62(void);
void sub_00079CB2(void);
void sub_00052A60(void);
void sub_0007A4BD(void);
void sub_00094FC0(void);
void sub_00094FFB(void);
void sub_00094AD9(void);
void sub_00095D30(void);
void sub_00094977(void);
void sub_00094B10(void);
void sub_0009D34E(void);
void sub_00083A6C(void);
extern void sub_00083FBB_gen(void) /* alternate-SEH generated overlay */;
extern void sub_00097E46(void);
extern void sub_00097C9F(void);
extern void sub_00084020_gen(void) /* alternate-SEH generated overlay */;
extern void sub_0008427E_gen(void) /* alternate-SEH generated overlay */;
extern void sub_000842EA_gen(void) /* alternate-SEH generated overlay */;
extern void sub_000854CF_gen(void) /* alternate-SEH generated overlay */;
extern void sub_000858F3_gen(void) /* alternate-SEH generated overlay */;
extern void sub_000860AA_gen(void) /* alternate-SEH generated overlay */;
extern void sub_0008629E_gen(void) /* alternate-SEH generated overlay */;
extern void sub_00096738_gen(void) /* alternate-SEH generated overlay */;
extern void sub_00097AFC_gen(void) /* alternate-SEH generated overlay */;
extern void sub_0009E85A_gen(void) /* alternate-SEH generated overlay */;
extern void sub_00093C45_gen(void) /* generated body with ABI wrapper */;
extern void sub_0009418C_gen(void) /* generated body with ABI wrapper */;
extern void sub_000943ED_gen(void) /* generated body with ABI wrapper */;
extern void sub_0002539A_gen(void) /* generated body with ABI wrapper */;
extern void sub_001E7B8F_gen(void) /* generated body with worker-context overlay */;
extern void sub_00093B04_gen(void) /* recovered memmove tail body */;
extern void sub_00093AFC(void);
extern void sub_00093B58_gen(void);
extern void sub_00093B60_gen(void);
extern void sub_00093B70_gen(void);
extern void sub_00093B84_gen(void);

/* 0x252AF is a real helper that the generator reports as an unresolved
 * target because it begins inside the surrounding function cluster. */
void sub_000252AF(void)
{
    uint32_t caller_ebp = g_ebp;
    uint32_t caller_seh_ebp = g_seh_ebp;
    uint32_t caller_ebx = g_ebx;
    uint32_t caller_esi = g_esi;
    uint32_t caller_edi = g_edi;
    /* Native 0x252AF is PUSH [EAX]: the stream's FILE pointer, not the
     * containing polymorphic stream object. Passing EAX-4 makes fread
     * interpret a vtable and path fields as CRT buffering state. */
    PUSH32(g_esp, MEM32(g_eax));
    PUSH32(g_esp, 1);
    PUSH32(g_esp, MEM32(g_esp + 0x10));
    PUSH32(g_esp, MEM32(g_esp + 0x10));
    uint32_t args_esp = g_esp;
    PUSH32(g_esp, 0x000252C0u);
    RECOMP_ABI_CALL(0x0009492Bu, sub_0009492B);
    g_esp = args_esp;
    g_ebp = caller_ebp;
    g_seh_ebp = caller_seh_ebp;
    g_ebx = caller_ebx;
    g_esi = caller_esi;
    g_edi = caller_edi;
    g_esp += 0x10;
    /* ICALL leaves the guest return sentinel for the callee's ret 8. */
    g_esp += 12;
}

/* 0x252DF is the one-byte RET tail at the end of the stream constructor.
 * The title also calls that tail through a vtable entry. */
void sub_000252DF(void)
{
    g_esp += 4u;
}

/* 0x2539A is an FPO helper whose nested calls may overwrite the emulated
 * callee-saved register set. Keep the generated body and repair the ABI at
 * this narrow boundary. */
void sub_0002539A(void)
{
    uint32_t esp = g_esp;
    uint32_t ebp = g_ebp;
    uint32_t seh_ebp = g_seh_ebp;
    uint32_t ebx = g_ebx;
    uint32_t esi = g_esi;
    uint32_t edi = g_edi;

    sub_0002539A_gen();

    g_ebp = ebp;
    g_seh_ebp = seh_ebp;
    g_ebx = ebx;
    g_esi = esi;
    g_edi = edi;
    g_esp = esp + 4;
}

/* The generator's normal title-wide SEH helper is 0x97AA4.  This one CRT
 * allocator call site uses the equivalent helper at 0x94FC0; keep the body
 * generated, but compile that one function with the correct helper selected. */
static void call_alternate_seh_preserving(recomp_func_t body)
{
    uint32_t ebx = g_ebx;
    uint32_t esi = g_esi;
    uint32_t edi = g_edi;
    uint32_t ebp = g_ebp;
    uint32_t seh_ebp = g_seh_ebp;

    if (!mm3_mod_on(MM3_MOD_TRANSLATION)) {     /* the generated body as it is */
        body();
        return;
    }
    body();

    g_ebx = ebx;
    g_esi = esi;
    g_edi = edi;
    g_ebp = ebp;
    g_seh_ebp = seh_ebp;
}

void sub_00083FBB(void)
{
    uint32_t esp = g_esp;
    uint32_t dest = MEM32(esp + 4);
    uint32_t src = MEM32(esp + 8);
    uint32_t count = MEM32(esp + 12);
    uint32_t i;

    /* The generated FPO body reads args via inherited EBP after its SEH prolog. */
    for (i = 0; i < count; ++i) {
        uint8_t ch = MEM8(src + i);
        if (!ch) {
            MEM8(dest + i) = 0;
            break;
        }
        MEM8(dest + i) = ch;
    }
    if (count && i == count)
        MEM8(dest + count - 1) = 0;
    g_eax = dest;
    g_esp = esp + 16;
}
void sub_00093DD3(void)
{
    static const char hex[] = "0123456789abcdef";
    uint32_t esp = g_esp;
    uint32_t frame, destination = MEM32(esp + 4u);
    uint32_t format = MEM32(esp + 8u);
    uint32_t i;

    if (MEM8(format) == '%' && MEM8(format + 1u) == '0' &&
        MEM8(format + 2u) == '8' && MEM8(format + 3u) == 'l' &&
        MEM8(format + 4u) == 'x' && MEM8(format + 5u) == 0) {
        uint32_t value = MEM32(esp + 12u);
        for (i = 0; i < 8u; ++i)
            MEM8(destination + i) = (uint8_t)hex[(value >> (28u - i * 4u)) & 0xFu];
        MEM8(destination + 8u) = 0;
        g_eax = 8u;
        g_esp = esp + 4u;
        return;
    }

    PUSH32(g_esp, g_ebp);
    frame = g_esp;
    g_ebp = frame;
    g_seh_ebp = frame;
    g_esp -= 0x20u;
    PUSH32(g_esp, g_esi);
    g_esi = MEM32(frame + 8u);
    PUSH32(g_esp, g_edi);
    PUSH32(g_esp, frame + 0x10u);
    PUSH32(g_esp, MEM32(frame + 0xCu));
    PUSH32(g_esp, frame - 0x20u);
    MEM32(frame - 0x1Cu) = 0x7FFFFFFFu;
    MEM32(frame - 0x14u) = 0x42u;
    MEM32(frame - 0x18u) = g_esi;
    MEM32(frame - 0x20u) = g_esi;
    g_ebp = frame;
    g_seh_ebp = frame;
    PUSH32(g_esp, 0x00093E02u);
    RECOMP_ABI_CALL(0x00097E46u, sub_00097E46);
    g_esp += 0xCu;
    g_edi = g_eax;
    if (g_esi) {
        MEM32(frame - 0x1Cu) -= 1u;
        if ((int32_t)MEM32(frame - 0x1Cu) >= 0)
            MEM8(MEM32(frame - 0x20u)) = 0;
        else {
            PUSH32(g_esp, frame - 0x20u);
            PUSH32(g_esp, 0);
            g_ebp = frame;
            g_seh_ebp = frame;
            PUSH32(g_esp, 0x00093E23u);
            RECOMP_ABI_CALL(0x00097C9Fu, sub_00097C9F);
            g_esp += 8u;
        }
    }
    g_eax = g_edi;
    POP32(g_esp, g_edi);
    POP32(g_esp, g_esi);
    g_esp = frame;
    POP32(g_esp, g_ebp);
    g_esp += 4u;
}
void sub_00084020(void) { call_alternate_seh_preserving(sub_00084020_gen); }
void sub_0008427E(void) { call_alternate_seh_preserving(sub_0008427E_gen); }
void sub_000842EA(void) { call_alternate_seh_preserving(sub_000842EA_gen); }
void sub_000854CF(void) { call_alternate_seh_preserving(sub_000854CF_gen); }
#ifdef MM3_HEAP_WATCH_WRAPPER
void sub_000858F3_original(void)
#else
void sub_000858F3(void)
#endif
{
    call_alternate_seh_preserving(sub_000858F3_gen);
}
#ifdef MM3_HEAP_WATCH_WRAPPER
void sub_000860AA_original(void) { call_alternate_seh_preserving(sub_000860AA_gen); }
#else
void sub_000860AA(void) { call_alternate_seh_preserving(sub_000860AA_gen); }
#endif
void sub_0008629E(void) { call_alternate_seh_preserving(sub_0008629E_gen); }
void sub_00096738(void) { sub_00096738_gen(); }
/* The generated alternate-SEH overlay already owns both frame transitions.
 * Preserve its register results; the caller observes EBX after this helper. */
void sub_00097AFC(void) { sub_00097AFC_gen(); }
void sub_0009E85A(void) { sub_0009E85A_gen(); }

/* The ABI overlay already contains the complete alternate-SEH epilogue. */
void sub_00093C45(void) { sub_00093C45_gen(); }

/* 0x9418C is an FPO cdecl helper; its generated SEH path overruns the frame. */
void sub_0009418C(void)
{
    uint32_t esp = g_esp;
    uint32_t ebp = g_ebp;
    uint32_t seh_ebp = g_seh_ebp;
    uint32_t ebx = g_ebx;
    uint32_t esi = g_esi;
    uint32_t edi = g_edi;
 uint32_t path = MEM32(g_esp + 4u);
 sub_0009418C_gen();
 if (g_mm3_stream_trace) {
  static unsigned samples;
  if (samples++ < 32) {
   fprintf(stderr, "[CRT_OPEN] path=%08X FILE=%08X", path, g_eax);
   if (path >= 0x10000u && path < 0x04000000u - 128u) {
    fprintf(stderr, " name=");
    for (unsigned i = 0; i < 128 && MEM8(path+i); ++i)
     fputc(MEM8(path+i), stderr);
   }
   fprintf(stderr, "\n");
  }
 }
    g_ebp = ebp;
    g_seh_ebp = seh_ebp;
    g_ebx = ebx;
    g_esi = esi;
    g_edi = edi;
    g_esp = esp + 4;
}

/* 0x943ED is the same FPO/SEH cdecl family as 0x9418C. */
void sub_000943ED(void)
{
    uint32_t esp = g_esp;
    uint32_t ebp = g_ebp;
    uint32_t seh_ebp = g_seh_ebp;
    uint32_t ebx = g_ebx;
    uint32_t esi = g_esi;
    uint32_t edi = g_edi;
    sub_000943ED_gen();
    g_ebp = ebp;
    g_seh_ebp = seh_ebp;
    g_ebx = ebx;
    g_esi = esi;
    g_edi = edi;
    g_esp = esp + 4;
}

void sub_00093B04(void)
{
    sub_00093B04_gen();
}

recomp_func_t recomp_lookup_manual(uint32_t xbox_va)
{
    if (xbox_va == 0x00093AFCu)
        return sub_00093AFC;
    if (xbox_va == 0x000252AFu)
        return sub_000252AF;
    if (xbox_va == 0x000252DFu)
        return sub_000252DF;
    if (xbox_va == 0x00021103u)
        return sub_00021103;
    if (xbox_va == 0x0007169Du)
        return sub_0007169D;
    if (xbox_va == 0x00012850u)
        return sub_00012850;
    if (xbox_va == 0x00043E62u)
        return sub_00043E62;
    if (xbox_va == 0x00079CB2u)
        return sub_00079CB2;
    if (xbox_va == 0x00052A60u)
        return sub_00052A60;
    if (xbox_va == 0x0007A4BDu)
        return sub_0007A4BD;
    if (xbox_va == 0x00094FC0u)
        return sub_00094FC0;
    if (xbox_va == 0x00094FFBu)
        return sub_00094FFB;
    if (xbox_va == 0x0009E85Au)
        return sub_0009E85A;
    if (xbox_va == 0x00094AD9u)
        return sub_00094AD9;
    if (xbox_va == 0x0009D34Eu)
        return sub_0009D34E;
    if (xbox_va == 0x00083A6Cu)
        return sub_00083A6C;
    if (xbox_va == 0x00093C45u)
        return sub_00093C45;
    if (xbox_va == 0x0009418Cu)
        return sub_0009418C;
    if (xbox_va == 0x000943EDu)
        return sub_000943ED;
    if (xbox_va == 0x00093B04u)
        return sub_00093B04;
    /* Reverse memmove remainder entries reuse the caller's saved frame. */
    if (xbox_va == 0x00093B58u)
        return sub_00093B58_gen;
    if (xbox_va == 0x00093B60u)
        return sub_00093B60_gen;
    if (xbox_va == 0x00093B70u)
        return sub_00093B70_gen;
    if (xbox_va == 0x00093B84u)
        return sub_00093B84_gen;
    return (recomp_func_t)0;
}

/* FPO/SEH frame transition: reload the frame established by __SEH_prolog
 * before restoring the saved registers.  The generated leaf body retains the
 * caller's pre-prolog EBP and otherwise returns with ESP 0x34 bytes early. */
void sub_00043E62(void)
{
    uint32_t ebp = g_ebp;

    g_eax = 0x253A20u;
    PUSH32(g_esp, 0x00043E6Cu);
    RECOMP_ABI_CALL(0x00097AA4u, sub_00097AA4);
    ebp = g_esp + 0x0Cu;

    g_esp -= 0x24;
    PUSH32(g_esp, g_ebx);
    PUSH32(g_esp, g_esi);
    PUSH32(g_esp, g_edi);
    MEM32(ebp - 16) = g_esp;
    PUSH32(g_esp, 0x18u);
    PUSH32(g_esp, 0x00043E7Cu);
    RECOMP_ABI_CALL(0x00170EEEu, sub_00170EEE);
    POP32(g_esp, g_ecx);

    MEM32(ebp - 20) = g_eax;
    g_edx = 0;
    MEM32(ebp - 4) = g_edx;
    MEM32(ebp - 24) = g_edx;
    MEM32(ebp - 28) = g_eax;
    if (g_eax != g_edx)
        MEM32(g_eax) = g_edx;
    MEM32(ebp - 32) = 1;
    MEM32(ebp - 36) = g_edx;
    g_ecx = g_eax + 4;
    MEM32(ebp - 40) = g_ecx;
    if (g_ecx != g_edx)
        MEM32(g_ecx) = g_edx;
    MEM32(ebp - 32) = 2;
    MEM32(ebp - 44) = g_edx;
    g_ecx = g_eax + 8;
    MEM32(ebp - 48) = g_ecx;
    if (g_ecx != g_edx)
        MEM32(g_ecx) = g_edx;
    MEM32(ebp - 4) |= 0xFFFFFFFFu;
    MEM8(g_eax + 0x14) = 1;
    MEM8(g_eax + 0x15) = LO8(g_edx);
    g_ecx = MEM32(ebp - 12);
    MEM32(XBOX_FS_BASE) = g_ecx;
    POP32(g_esp, g_edi);
    POP32(g_esp, g_esi);
    POP32(g_esp, g_ebx);
    g_esp = ebp;
    POP32(g_esp, ebp);
    g_esp += 8;
}

/* Same FPO/SEH frame transition as sub_00043E62, for the adjacent list
 * allocator path.  Reloading the post-prolog frame keeps its saved registers
 * and caller stack balanced. */
void sub_00079CB2(void)
{
    uint32_t ebp = g_seh_ebp;

    g_eax = 0x253C0Du;
    PUSH32(g_esp, 0x00079CBCu);
    RECOMP_ABI_CALL(0x00097AA4u, sub_00097AA4);
    ebp = g_esp + 0x0Cu;

    g_esp -= 0x24;
    PUSH32(g_esp, g_ebx);
    PUSH32(g_esp, g_esi);
    PUSH32(g_esp, g_edi);
    MEM32(ebp - 16) = g_esp;
    PUSH32(g_esp, 0x38u);
    PUSH32(g_esp, 0x00079CCCu);
    RECOMP_ABI_CALL(0x00170EEEu, sub_00170EEE);
    POP32(g_esp, g_ecx);

    MEM32(ebp - 20) = g_eax;
    g_edx = 0;
    MEM32(ebp - 4) = g_edx;
    MEM32(ebp - 24) = g_edx;
    MEM32(ebp - 28) = g_eax;
    if (g_eax != g_edx)
        MEM32(g_eax) = g_edx;
    MEM32(ebp - 32) = 1;
    MEM32(ebp - 36) = g_edx;
    g_ecx = g_eax + 4;
    MEM32(ebp - 40) = g_ecx;
    if (g_ecx != g_edx)
        MEM32(g_ecx) = g_edx;
    MEM32(ebp - 32) = 2;
    MEM32(ebp - 44) = g_edx;
    g_ecx = g_eax + 8;
    MEM32(ebp - 48) = g_ecx;
    if (g_ecx != g_edx)
        MEM32(g_ecx) = g_edx;
    MEM32(ebp - 4) |= 0xFFFFFFFFu;
    MEM8(g_eax + 0x34) = 1;
    MEM8(g_eax + 0x35) = LO8(g_edx);
    g_ecx = MEM32(ebp - 12);
    MEM32(XBOX_FS_BASE) = g_ecx;
    POP32(g_esp, g_edi);
    POP32(g_esp, g_esi);
    POP32(g_esp, g_ebx);
    g_esp = ebp;
    POP32(g_esp, ebp);
    g_esp += 8;
}

/* Same generated FPO/SEH defect in the third allocator instance. */
void sub_00052A60(void)
{
    uint32_t ebp = g_seh_ebp;

    g_eax = 0x253CADu;
    PUSH32(g_esp, 0x00052A6Au);
    RECOMP_ABI_CALL(0x00097AA4u, sub_00097AA4);
    ebp = g_esp + 0x0Cu;

    g_esp -= 0x24;
    PUSH32(g_esp, g_ebx);
    PUSH32(g_esp, g_esi);
    PUSH32(g_esp, g_edi);
    MEM32(ebp - 16) = g_esp;
    PUSH32(g_esp, 0x48u);
    PUSH32(g_esp, 0x00052A7Au);
    RECOMP_ABI_CALL(0x00170EEEu, sub_00170EEE);
    POP32(g_esp, g_ecx);

    MEM32(ebp - 20) = g_eax;
    g_edx = 0;
    MEM32(ebp - 4) = g_edx;
    MEM32(ebp - 24) = g_edx;
    MEM32(ebp - 28) = g_eax;
    if (g_eax != g_edx)
        MEM32(g_eax) = g_edx;
    MEM32(ebp - 32) = 1;
    MEM32(ebp - 36) = g_edx;
    g_ecx = g_eax + 4;
    MEM32(ebp - 40) = g_ecx;
    if (g_ecx != g_edx)
        MEM32(g_ecx) = g_edx;
    MEM32(ebp - 32) = 2;
    MEM32(ebp - 44) = g_edx;
    g_ecx = g_eax + 8;
    MEM32(ebp - 48) = g_ecx;
    if (g_ecx != g_edx)
        MEM32(g_ecx) = g_edx;
    MEM32(ebp - 4) |= 0xFFFFFFFFu;
    MEM8(g_eax + 0x44) = 1;
    MEM8(g_eax + 0x45) = LO8(g_edx);
    g_ecx = MEM32(ebp - 12);
    MEM32(XBOX_FS_BASE) = g_ecx;
    POP32(g_esp, g_edi);
    POP32(g_esp, g_esi);
    POP32(g_esp, g_ebx);
    g_esp = ebp;
    POP32(g_esp, ebp);
    g_esp += 8;
}

/* FPO/SEH correction for the five-argument table fill used by the startup
 * allocator.  Reload the post-prolog frame before register restoration. */
void sub_0007A4BD(void)
{
    uint32_t ebp = g_seh_ebp;

    g_eax = MEM32(ebp + 8);
    PUSH32(g_esp, 0x253772u);
    RECOMP_ABI_CALL(0x00097AA4u, sub_00097AA4);
    ebp = g_esp + 0x0Cu;

    PUSH32(g_esp, g_ecx);
    PUSH32(g_esp, g_ecx);
    PUSH32(g_esp, g_ebx);
    PUSH32(g_esp, g_esi);
    PUSH32(g_esp, g_edi);
    MEM32(ebp - 16) = g_esp;
    g_eax = MEM32(ebp + 8);
    MEM32(ebp - 4) = 0;

    while (MEM32(ebp + 0x0Cu) > 0) {
        MEM32(ebp - 20) = g_eax;
        if (g_eax != 0) {
            g_ecx = MEM32(ebp + 0x10u);
            g_ecx = MEM32(g_ecx);
            MEM32(g_eax) = g_ecx;
        }
        MEM32(ebp + 0x0Cu) -= 1;
        g_eax += 4;
        MEM32(ebp + 8) = g_eax;
    }

    MEM32(ebp - 4) |= 0xFFFFFFFFu;
    g_ecx = MEM32(ebp - 12);
    MEM32(XBOX_FS_BASE) = g_ecx;
    POP32(g_esp, g_edi);
    POP32(g_esp, g_esi);
    POP32(g_esp, g_ebx);
    g_esp = ebp;
    POP32(g_esp, ebp);
    g_esp += 24;
}

/* The XBE has a second SEH-prolog body at 0x00094FC0.  The generator's
 * per-title helper override handles 0x00097AA4; keep this direct bootstrap
 * helper's frame visible to its one manually owned caller as well. */
void sub_00094FC0(void)
{
    uint32_t ebp = g_ebp;

    PUSH32(g_esp, 0x00096A70u);
    g_eax = MEM32(XBOX_FS_BASE);
    PUSH32(g_esp, g_eax);
    g_eax = MEM32(g_esp + 0x10u);
    MEM32(g_esp + 0x10u) = ebp;
    ebp = g_esp + 0x10u;
    g_esp -= g_eax;
    PUSH32(g_esp, g_ebx);
    PUSH32(g_esp, g_esi);
    PUSH32(g_esp, g_edi);
    g_eax = MEM32(ebp - 8);
    MEM32(ebp - 24) = g_esp;
    PUSH32(g_esp, g_eax);
    g_eax = MEM32(ebp - 4);
    MEM32(ebp - 4) = 0xFFFFFFFFu;
    MEM32(ebp - 8) = g_eax;
    g_eax = ebp - 16;
    MEM32(XBOX_FS_BASE) = g_eax;
    g_seh_ebp = ebp;
    g_esp += 4;
}

/* Matching epilog for the project-owned 0x00094FC0 prolog. */
void sub_00094FFB(void)
{
    uint32_t ebp = g_seh_ebp;
    uint32_t ecx = MEM32(ebp - 16);

    MEM32(XBOX_FS_BASE) = ecx;
    POP32(g_esp, ecx);
    POP32(g_esp, g_edi);
    POP32(g_esp, g_esi);
    POP32(g_esp, g_ebx);
    g_esp = ebp;
    POP32(g_esp, ebp);
    PUSH32(g_esp, ecx);
    g_seh_ebp = ebp;
    g_esp += 4;
}

/* 0x94AD9 establishes a local SEH frame before calling the generated helper.
 * Keep this short bridge project-owned so the post-prolog frame is explicit. */
void sub_00094AD9(void)
{
    uint32_t ebp;

    PUSH32(g_esp, 0x0Cu);
    PUSH32(g_esp, 0x00368458u);
    PUSH32(g_esp, 0x00094AE5u);
    RECOMP_ABI_CALL(0x00094FC0u, sub_00094FC0);
    ebp = g_seh_ebp;

    PUSH32(g_esp, MEM32(ebp + 8u));
    PUSH32(g_esp, 0x00094AEDu);
    g_ebp = ebp;
    g_seh_ebp = ebp;
    RECOMP_ABI_CALL(0x00095D30u, sub_00095D30);
    POP32(g_esp, g_ecx);

    MEM32(ebp - 4u) = 0;
    PUSH32(g_esp, MEM32(ebp + 8u));
    PUSH32(g_esp, 0x00094AFAu);
    g_ebp = ebp;
    g_seh_ebp = ebp;
    RECOMP_ABI_CALL(0x00094977u, sub_00094977);
    POP32(g_esp, g_ecx);

    MEM32(ebp - 0x1Cu) = g_eax;
    MEM32(ebp - 4u) = 0xFFFFFFFFu;
    PUSH32(g_esp, 0x00094B07u);
    g_ebp = ebp;
    g_seh_ebp = ebp;
    RECOMP_ABI_CALL(0x00094B10u, sub_00094B10);

    g_eax = MEM32(ebp - 0x1Cu);
    PUSH32(g_esp, 0x00094B0Fu);
    g_ebp = ebp;
    g_seh_ebp = ebp;
    RECOMP_ABI_CALL(0x00094FFBu, sub_00094FFB);
    g_esp += 4;
}

/* 0x9D34E calls the alternate SEH prolog.  Reload its post-prolog frame
 * before touching locals; the generated FPO body retains the stale caller
 * EBP across that helper call. */
void sub_0009D34E(void)
{
    uint32_t ebp;
    uint32_t saved_esp;
    uint32_t target_va;
    recomp_func_t target;

    saved_esp = g_esp;
    PUSH32(g_esp, 0x10u);
    PUSH32(g_esp, 0x368EC8u);
    PUSH32(g_esp, 0x0009D35Au);
    RECOMP_ABI_CALL(0x00094FC0u, sub_00094FC0);
    ebp = g_seh_ebp;

    if (MEM32(0x3C01D8u) == 0)
        MEM32(0x3C01D8u) = 0x0009D33Eu;

    MEM32(ebp - 4u) = 0;
    PUSH32(g_esp, MEM32(ebp + 0x0Cu));
    PUSH32(g_esp, MEM32(ebp + 8u));
    PUSH32(g_esp, 0x0009D37Du);
    target_va = MEM32(0x3C01D8u);
    target = recomp_lookup_manual(target_va);
    if (!target)
        target = recomp_lookup(target_va);
    if (target)
        RECOMP_ABI_CALL(target_va, target);
    else {
        recomp_icall_fail_log(target_va);
        g_esp = saved_esp;
        g_eax = 0;
    }

    MEM32(ebp - 28u) = g_eax;
    MEM32(ebp - 4u) = 0xFFFFFFFFu;
    g_seh_ebp = ebp;
    PUSH32(g_esp, 0x0009D3AEu);
    RECOMP_ABI_CALL(0x00094FFBu, sub_00094FFB);
    g_esp += 4;
}

/* Bootstrap thread routine: retain generated semantics while explicitly
 * reading back the alternate 0x00094FC0 frame. */
void sub_00083A6C(void)
{
    uint32_t ebp = g_seh_ebp;
    uint32_t eax;
    uint32_t edx;
    uint32_t ecx;
    uint32_t edi;
    uint32_t esi;
    uint32_t ebx;

    PUSH32(g_esp, 0x18u);
    PUSH32(g_esp, 0x362660u);
    PUSH32(g_esp, 0x00083A78u);
    RECOMP_ABI_CALL(0x00094FC0u, sub_00094FC0);
    ebp = g_seh_ebp;

    /* The kernel owns this worker's TLS block through fs:[0x28]+0x28.
     * Keep it there: redirecting the context into EBP+0x10 shares mutable
     * startup state and overwrites caller frames when workers overlap. */

    MEM32(ebp - 4) = MEM32(ebp - 4) & 0;
    eax = MEM32(XBOX_FS_BASE + 0x28u);
    MEM32(ebp - 28) = eax;
    edx = MEM32(eax + 0x28u) + 4;
    MEM32(ebp - 32) = edx;
    MEM32(edx - 4) = edx;
    ebx = MEM32(0x362690u);
    esi = MEM32(0x36268Cu);
    ebx -= esi;
    MEM32(ebp - 36) = ebx;
    ecx = ebx;
    edi = edx;
    eax = ecx;
    ecx >>= 2;
    if (!g_df) {
        uint32_t i;
        for (i = 0; i < ecx; ++i)
            MEM32(edi + i * 4) = MEM32(esi + i * 4);
        esi += ecx * 4;
        edi += ecx * 4;
    } else {
        uint32_t i;
        for (i = 0; i < ecx; ++i)
            MEM32(edi - i * 4) = MEM32(esi - i * 4);
        esi -= ecx * 4;
        edi -= ecx * 4;
    }
    ecx = 0;
    ecx = eax & 3u;
    if (!g_df) {
        uint32_t i;
        for (i = 0; i < ecx; ++i)
            MEM8(edi + i) = MEM8(esi + i);
        esi += ecx;
        edi += ecx;
    } else {
        uint32_t i;
        for (i = 0; i < ecx; ++i)
            MEM8(edi - i) = MEM8(esi - i);
        esi -= ecx;
        edi -= ecx;
    }
    ecx = MEM32(0x36269Cu);
    if (ecx != 0) {
        edi = ebx + edx;
        edx = ecx;
        ecx >>= 2;
        for (uint32_t i = 0; i < ecx; ++i)
            MEM32(edi + i * 4) = 0;
        edi += ecx * 4;
        ecx = edx & 3u;
        for (uint32_t i = 0; i < ecx; ++i)
            MEM8(edi + i) = 0;
    }

    /* The guest keeps EDI live across the startup callback. */
    g_edi = edi;

    PUSH32(g_esp, 1u);
    PUSH32(g_esp, 0x00083AD8u);
    RECOMP_ABI_CALL(0x000838C3u, sub_000838C3);

    {
        uint32_t saved_esp = g_esp;
        PUSH32(g_esp, MEM32(ebp + 0x0Cu));
        PUSH32(g_esp, 0x00083ADEu);
        RECOMP_ICALL_SAFE(MEM32(ebp + 8), saved_esp);
    }

    MEM32(ebp - 40) = g_eax;
    PUSH32(g_esp, 0);
    PUSH32(g_esp, 0x00083AE8u);
    RECOMP_ABI_CALL(0x000838C3u, sub_000838C3);
    /* Original normal tail at 0x83AF6 terminates this system thread. */
    MEM32(ebp - 4) = 0xFFFFFFFFu;
    {
        uint32_t saved_esp = g_esp;
        PUSH32(g_esp, MEM32(ebp - 40));
        PUSH32(g_esp, 0x00083B03u);
        RECOMP_ICALL_SAFE(MEM32(0x362030u), saved_esp);
    }
    /* The original executes INT3 if PsTerminateSystemThread returns. */
    fprintf(stderr, "[CRASH] thread termination returned at 0x00083B03\n");
    abort();
}

/* The bootstrap supplies the original return-address/argument layout. */
void sub_001E7B8F(void)
{
    sub_001E7B8F_gen();
}

/* FPO/SEH frame transition: __SEH_prolog changes the guest frame to
 * g_esp + 0x0C, but the generated body keeps the pre-prolog EBP.  Keep the
 * normal string-reallocation path here and publish the corrected frame before
 * cleanup so ESI/EDI restore from this function's own saved slots. */
void sub_00012850(void)
{
    uint32_t ebp = g_seh_ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t buffer;

    g_eax = 0x253D2Fu;
    PUSH32(g_esp, 0x0001285Au);
    RECOMP_ABI_CALL(0x00097AA4u, sub_00097AA4);
    ebp = g_esp + 0x0Cu;

    g_esp -= 0x10;
    PUSH32(g_esp, g_ebx);
    PUSH32(g_esp, g_esi);
    PUSH32(g_esp, g_edi);

    esi = g_ecx;
    edi = MEM32(ebp + 8) | 0xFu;
    if (edi > 0xFFFFFFFEu)
        edi = MEM32(ebp + 8);

    g_eax = edi + 1;
    PUSH32(g_esp, g_eax);
    PUSH32(g_esp, 0x00012883u);
    RECOMP_ABI_CALL(0x00170EEEu, sub_00170EEE);
    POP32(g_esp, g_ecx);
    buffer = g_eax;

    if (MEM32(ebp + 0x0C) != 0) {
        uint32_t source = (MEM32(esi + 0x18) >= 0x10u)
            ? MEM32(esi + 4)
            : esi + 4;

        PUSH32(g_esp, MEM32(ebp + 0x0C));
        PUSH32(g_esp, source);
        PUSH32(g_esp, buffer);
        PUSH32(g_esp, 0x000128D8u);
        RECOMP_ABI_CALL(0x00012000u, sub_00012000);
        g_esp += 0x0C;
    }

    PUSH32(g_esp, 1);
    g_ecx = esi;
    PUSH32(g_esp, 0x000128E4u);
    RECOMP_ABI_CALL(0x000127A9u, sub_000127A9);

    MEM32(esi + 4) = buffer;
    MEM32(esi + 0x18) = edi;
    PUSH32(g_esp, MEM32(ebp + 0x0C));
    g_ecx = esi;
    PUSH32(g_esp, 0x000128F7u);
    RECOMP_ABI_CALL(0x00012731u, sub_00012731);

    g_ecx = MEM32(ebp - 0x0C);
    MEM32(XBOX_FS_BASE) = g_ecx;
    POP32(g_esp, g_edi);
    POP32(g_esp, g_esi);
    POP32(g_esp, g_ebx);
    g_esp = ebp;
    POP32(g_esp, ebp);
    g_esp += 0x0C;
}

/* Past the 50-line cap the non-code and failed-resolve loggers only count.
 * The totals print at exit and every 1024 suppressed events, so a hot loop
 * stays bounded but its volume is still visible. */
static volatile LONG icall_suppressed[2];   /* 0: failed resolve, 1: non-code */
static volatile LONG icall_summary_armed;
static void mm3_icall_summary(void)
{
    fprintf(stderr, "[ICALL] suppressed after the 50-line cap: failed-resolve=%ld non-code=%ld\n",
            (long)icall_suppressed[0], (long)icall_suppressed[1]);
    fflush(stderr);
}

static void mm3_icall_count_suppressed(int kind)
{
    LONG count = InterlockedIncrement(&icall_suppressed[kind]);
    if (InterlockedCompareExchange(&icall_summary_armed, 1, 0) == 0)
        atexit(mm3_icall_summary);
    if (count % 1024 == 0)
        mm3_icall_summary();
}

void recomp_icall_fail_log(uint32_t xbox_va)
{
    static volatile LONG printed;
    uint32_t caller = 0;
    /* A broken guest callback can recur in a polling loop. Keep the first
     * failures useful without turning a fault into unbounded synchronous I/O.
     * Dispatch and fallback behavior are unchanged. Explicit callback tracing
     * retains all failures for diagnosis, matching the non-code logger. */
    if (!g_mm3_frontend_icall_trace &&
        (printed >= 50 || InterlockedIncrement(&printed) > 50)) {
        mm3_icall_count_suppressed(0);
        return;
    }
    if (g_esp >= 0x00010000u && g_esp < 0x04000000u)
        caller = *(const uint32_t *)((uintptr_t)g_esp + g_xbox_mem_offset);
    fprintf(stderr, "[ICALL] Failed resolve VA 0x%08X caller=0x%08X\n",
            xbox_va, caller);
}

/* Diagnostics must not dereference a corrupt guest object while reporting it.
 * The code image is stable, but heap/stack pointers may refer to freed pages. */
static int mm3_icall_read(uint32_t va, void *out, size_t size)
{
    uintptr_t address, end, cursor;
    SIZE_T copied = 0;
    if (!size || size > 128 || va < 0x10000u ||
        va >= 0x08000000u || size > 0x08000000u - va) return 0;
    address = XBOX_PTR(va);
    end = address + size;
    if (end < address) return 0;
    for (cursor = address; cursor < end;) {
        MEMORY_BASIC_INFORMATION info;
        uintptr_t next;
        if (!VirtualQuery((void *)cursor, &info, sizeof info) ||
            info.State != MEM_COMMIT || (info.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
            return 0;
        next = (uintptr_t)info.BaseAddress + info.RegionSize;
        if (next <= cursor) return 0;
        cursor = next;
    }
    /* A concurrent guest free can invalidate a page after VirtualQuery.
     * ReadProcessMemory reports that race instead of faulting in diagnostics. */
    return ReadProcessMemory(GetCurrentProcess(), (const void *)address, out,
                             size, &copied) && copied == size;
}

/* Decode the original near indirect CALL ending at its return address. A
 * target-value scan cannot identify a null vtable slot: every zero in the
 * unrelated startup callback table matches it. The common FF /2 addressing
 * forms below include register, absolute, displacement and SIB operands.
 * ESP at operand evaluation precedes the pushed return address by four bytes. */
static unsigned mm3_icall_decode(uint32_t caller, uint32_t target,
                                 uint32_t *slot, unsigned *base)
{
    uint32_t regs[8] = {g_eax, g_ecx, g_edx, g_ebx, g_esp + 4u,
                        g_ebp, g_esi, g_edi};
    unsigned length;
    *slot = 0; *base = 9;
    if (caller < g_xbox_code_lo + 2u || caller > g_xbox_code_hi) return 0;
    for (length = 7; length >= 2; --length) {
        unsigned char bytes[7];
        unsigned mod, rm, used = 2, reg;
        uint32_t address = 0, observed;
        int32_t displacement = 0;
        if (caller - g_xbox_code_lo < length ||
            !mm3_icall_read(caller - length, bytes, length) || bytes[0] != 0xFF ||
            ((bytes[1] >> 3) & 7u) != 2u) continue;
        mod = bytes[1] >> 6; rm = bytes[1] & 7u;
        if (mod == 3) {
            if (length == 2 && regs[rm] == target) { *base = rm; return 2; }
            continue;
        }
        reg = rm;
        if (rm == 4) {
            unsigned sib, index;
            if (used == length) continue;
            sib = bytes[used++]; reg = sib & 7u; index = (sib >> 3) & 7u;
            if (index != 4) address = regs[index] << (sib >> 6);
        }
        if (mod == 0 && reg == 5) {
            reg = 8; /* Absolute addressing, including a SIB without a base. */
        } else {
            address += regs[reg];
        }
        if (mod == 1) {
            if (used + 1 != length) continue;
            displacement = (int8_t)bytes[used++];
        } else if (mod == 2 || reg == 8) {
            if (used + 4 != length) continue;
            memcpy(&displacement, bytes + used, 4); used += 4;
        }
        if (used != length) continue;
        address += (uint32_t)displacement;
        if (mm3_icall_read(address, &observed, 4) && observed != target) continue;
        *slot = address; *base = reg;
        return 1;
    }
    return 0;
}

void recomp_icall_not_code_log(uint32_t xbox_va)
{
    static volatile LONG printed;
    static volatile LONG sequence;
    uint32_t caller = 0;
    uint32_t slot = 0;
    uint32_t slot_value = 0, object_words[8] = {0};
    uint32_t stack_words[16] = {0}, frame_words[16] = {0};
    uint32_t frame = g_ebp >= 0x10020u ? g_ebp - 0x20u : 0;
    unsigned base = 9, kind, slot_read, object_read, stack_read, frame_read, sample;
    unsigned long long tick;
    static const char *const names[] = {
        "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi", "absolute", "unknown"};
    /* Broken callbacks can recur every frame. Keep a bounded diagnostic,
     * with no decoding, pointer reads or synchronous I/O after the limit. */
    if (!g_mm3_frontend_icall_trace &&
        (printed >= 50 || InterlockedIncrement(&printed) > 50)) {
        mm3_icall_count_suppressed(1);
        return;
    }
    sample = (unsigned)InterlockedIncrement(&sequence);
    tick = (unsigned long long)GetTickCount64();
    mm3_icall_read(g_esp, &caller, 4);
    kind = mm3_icall_decode(caller, xbox_va, &slot, &base);
    slot_read = kind == 1 && mm3_icall_read(slot, &slot_value, 4);
    object_read = mm3_icall_read(g_ecx, object_words, sizeof object_words);
    stack_read = mm3_icall_read(g_esp, stack_words, sizeof stack_words);
    frame_read = mm3_icall_read(frame, frame_words, sizeof frame_words);
    fprintf(stderr, "[ICALL] non-code target 0x%08X slot=0x%08X caller=0x%08X esp=0x%08X"
            " kind=%s base=%s slot_read=%u slot_value=%08X"
            " eax=%08X ecx=%08X edx=%08X esi=%08X edi=%08X object_read=%u"
            " object_words=%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X"
            " sample=%u tick_ms=%llu\n",
            xbox_va, slot, caller, g_esp,
            kind == 1 ? "memory" : kind == 2 ? "register" : "unknown", names[base],
            slot_read, slot_value, g_eax, g_ecx, g_edx, g_esi, g_edi, object_read,
            object_words[0], object_words[1], object_words[2], object_words[3],
            object_words[4], object_words[5], object_words[6], object_words[7], sample, tick);
    fprintf(stderr, "[ICALL_STATE] sample=%u tick_ms=%llu ebx=%08X ebp=%08X seh_ebp=%08X df=%d"
            " stack_base=%08X stack_read=%u stack_words=", sample, tick,
            g_ebx, g_ebp, g_seh_ebp, g_df, g_esp, stack_read);
    for (unsigned i = 0; i < 16; ++i)
        fprintf(stderr, "%s%08X", i ? "," : "", stack_words[i]);
    fprintf(stderr, " frame_base=%08X frame_read=%u frame_words=", frame, frame_read);
    for (unsigned i = 0; i < 16; ++i)
        fprintf(stderr, "%s%08X", i ? "," : "", frame_words[i]);
    fprintf(stderr, "\n");
    if (caller == 0x0035D048u) {
        uint32_t request = stack_read ? stack_words[1] : g_eax;
        uint32_t words[16] = {0};
        unsigned readable = mm3_icall_read(request, words, sizeof words);
        /* This XPP call uses ECX as a callback, not a this pointer. Capture
         * the actual USB request passed as argument one, separately from
         * the callback target's bytes, to diagnose late or reused requests. */
        fprintf(stderr, "[ICALL_REQUEST] sample=%u tick_ms=%llu request=%08X source=%s"
                " request_read=%u size=%u kind=%u status=%08X callback=%08X context=%08X"
                " request_words=", sample, tick, request, stack_read ? "stack_arg" : "eax",
                readable, words[0] & 0xFFu, (words[0] >> 8) & 0xFFu,
                words[1], words[2], words[3]);
        for (unsigned i = 0; i < 16; ++i)
            fprintf(stderr, "%s%08X", i ? "," : "", words[i]);
        fprintf(stderr, "\n");
    }
    if (g_mm3_frontend_icall_trace &&
        (caller == 0x0011FB9Cu || caller == 0x000FB05Eu)) {
        static unsigned samples;
        if (samples++ < 8) {
            fprintf(stderr, "[FRONTEND_ICALL] eax=%08X ecx=%08X edx=%08X esi=%08X edi=%08X\n",
                    g_eax, g_ecx, g_edx, g_esi, g_edi);
            uint32_t pointers[] = {g_eax, g_ecx, g_esi, g_edi};
            for (unsigned i = 0; i < 4; ++i) {
                uint32_t p = pointers[i];
                uint32_t words[32];
                if (mm3_icall_read(p, words, sizeof words)) {
                    fprintf(stderr, "[FRONTEND_ICALL] object %08X:", p);
                    for (unsigned j = 0; j < 32; ++j)
                        fprintf(stderr, " %08X", words[j]);
                    fprintf(stderr, "\n");
                }
            }
        }
    }
    fflush(stderr);
}

/* Project override for the one startup allocator entry whose generated FPO
 * translation crosses __SEH_prolog.  The prolog changes the guest frame to
 * g_esp + 0x0C, but the generated caller keeps its stale local EBP.  That
 * writes the allocator result into the caller's frame and leaves ESI wrong.
 * Keep this override small and local; generated sources remain untouched. */
void sub_00021103(void)
{
    uint32_t ebp = g_seh_ebp;

    g_eax = 0x253A02u;
    PUSH32(g_esp, 0x0002110Du);
    RECOMP_ABI_CALL(0x00097AA4u, sub_00097AA4);
    ebp = g_esp + 0x0Cu;

    g_esp -= 0x24;
    PUSH32(g_esp, g_ebx);
    PUSH32(g_esp, g_esi);
    PUSH32(g_esp, g_edi);
    MEM32(ebp - 16) = g_esp;
    PUSH32(g_esp, 0x30u);
    PUSH32(g_esp, 0x0002111Du);
    RECOMP_ABI_CALL(0x00170EEEu, sub_00170EEE);
    POP32(g_esp, g_ecx);

    MEM32(ebp - 20) = g_eax;
    g_edx = 0;
    MEM32(ebp - 4) = g_edx;
    MEM32(ebp - 24) = g_edx;
    MEM32(ebp - 28) = g_eax;
    if (g_eax != g_edx)
        MEM32(g_eax) = g_edx;
    MEM32(ebp - 32) = 1;
    MEM32(ebp - 36) = g_edx;
    g_ecx = g_eax + 4;
    MEM32(ebp - 40) = g_ecx;
    if (g_ecx != g_edx)
        MEM32(g_ecx) = g_edx;
    MEM32(ebp - 32) = 2;
    MEM32(ebp - 44) = g_edx;
    g_ecx = g_eax + 8;
    MEM32(ebp - 48) = g_ecx;
    if (g_ecx != g_edx)
        MEM32(g_ecx) = g_edx;
    MEM32(ebp - 4) |= 0xFFFFFFFFu;
    MEM8(g_eax + 0x2C) = 1;
    MEM8(g_eax + 0x2D) = LO8(g_edx);
    g_ecx = MEM32(ebp - 12);
    MEM32(XBOX_FS_BASE) = g_ecx;
    POP32(g_esp, g_edi);
    POP32(g_esp, g_esi);
    POP32(g_esp, g_ebx);
    g_esp = ebp;
    POP32(g_esp, ebp);
    g_esp += 8;
}

/* Same FPO/SEH frame transition as sub_00021103, reached by the queue-node
 * producer.  Without the read-back, its cleanup restores the wrong guest
 * frame and corrupts the node link registers used by sub_0005A1FB. */
void sub_0007169D(void)
{
    uint32_t ebp = g_seh_ebp;

    g_eax = 0x25406Bu;
    PUSH32(g_esp, 0x000716A7u);
    RECOMP_ABI_CALL(0x00097AA4u, sub_00097AA4);
    ebp = g_esp + 0x0Cu;

    g_esp -= 0x0Cu;
    PUSH32(g_esp, g_ebx);
    PUSH32(g_esp, g_esi);
    PUSH32(g_esp, g_edi);
    MEM32(ebp - 16) = g_esp;
    PUSH32(g_esp, 0x30u);
    PUSH32(g_esp, 0x000716B7u);
    RECOMP_ABI_CALL(0x00170EEEu, sub_00170EEE);
    POP32(g_esp, g_ecx);

    g_esi = g_eax;
    MEM32(ebp - 20) = g_esi;
    MEM32(ebp - 4) = 0;
    MEM32(ebp - 24) = g_esi;
    if (g_esi != 0) {
        PUSH32(g_esp, MEM32(ebp + 0x1C));
        PUSH32(g_esp, MEM32(ebp + 0x14));
        PUSH32(g_esp, MEM32(ebp + 0x10));
        g_ebx = MEM32(ebp + 0x18);
        g_eax = MEM32(ebp + 0x0C);
        PUSH32(g_esp, 0x000716DCu);
        RECOMP_ABI_CALL(0x00078D64u, sub_00078D64);
    }

    MEM32(ebp - 4) |= 0xFFFFFFFFu;
    g_eax = g_esi;
    g_ecx = MEM32(ebp - 12);
    MEM32(XBOX_FS_BASE) = g_ecx;
    POP32(g_esp, g_edi);
    POP32(g_esp, g_esi);
    POP32(g_esp, g_ebx);
    g_esp = ebp;
    POP32(g_esp, ebp);
    g_esp += 28;
}

/* The WMVDEC section contains an unreachable cc_boundary false positive:
 * sub_002E0685 is leave; iretd followed by embedded data, with no callers.
 * Keep it out of generated code; it is not a game entry point. */
void sub_002E0685(void)
{
}

/* Same WMVDEC data-region false positive: this zero-caller range contains
 * embedded bytes decoded as iret instructions, not callable game code. */
void sub_002E2E2E(void)
{
}

/* Zero-caller library data false positives containing byte patterns decoded
 * as iret/iretd. They are excluded through --exclude-manual, not generated
 * or executed as game code. */
void sub_00284041(void) {}
void sub_0031569D(void) {}
void sub_00316465(void) {}
void sub_00317F5B(void) {}
void sub_00319630(void) {}
void sub_00358FAC(void) {}

/* XDK write-back-cache flush (D3D's push-buffer kick): set NV_PFB_WBC_FLUSH
 * and spin until the ack thread clears it, which it does between push-buffer
 * segments -- so each kick-off waits for the executor to finish the segment
 * in progress.
 *
 * 7fa6fcc made this a bare store fence so the title could run ahead of the
 * executor without limit. That broke a result MM3 reads mid-frame: a frame
 * now and then came out lit as if in a tunnel -- dark, headlights on, night
 * speedometer (5-22 per 1200-frame drive, cruise and missions alike). Truthful
 * fences, segment-end write-back of its 40x30 brightness targets and
 * serialising only BlockOnFence each fixed cruise but not the missions.
 * Bounding the run-ahead fixes both: the spin runs only once the executor is
 * more than MM3_KICK_LAG bytes of push buffer behind (or across a wrap).
 * 0 dark frames over 4 mission drives at 256 KB, 64 KB and 1 MB alike.
 * Race uncapped: spin every kick 296 fps, 64 KB 327, 256 KB 352, 1 MB 356,
 * unbounded 373.
 * ponytail: the racing read is still unidentified; bounded, not removed.
 * Registers are left as the guest routine leaves them. */
#define MM3_KICK_LAG 0x40000u   /* 256 KB */

/* The ack thread clears the bit between segments. A dead thread would
 * spin forever with no sign in the log, so report the wait every two
 * seconds. It keeps waiting: leaving early would let the title reuse
 * push-buffer space the executor has not read yet. */
static void mm3_kick_wait_ack(uint32_t flag_va, uint32_t cur, uint32_t get)
{
    LARGE_INTEGER freq, start, now;
    LONGLONG last;
    unsigned reports = 0;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    last = start.QuadPart;
    while (MEM32(flag_va) & 0x10000u) {
        QueryPerformanceCounter(&now);
        if (now.QuadPart - last >= 2 * freq.QuadPart) {
            last = now.QuadPart;
            fprintf(stderr, "[KICK_WAIT] ack bit 0x10000 at 0x%08X still set after %.1f s (report %u): cur=0x%08X get=0x%08X lag=0x%08X\n",
                    flag_va, (double)(now.QuadPart - start.QuadPart) / freq.QuadPart,
                    ++reports, cur, get, cur - get);
        }
    }
}

void sub_00344AB0(void)
{
    {   /* D3D's kick runs on the game's render thread: the one spike
         * reports sample (RECOMP_SPIKE_CAPTURE). */
        extern void recomp_sampler_watch_current_thread(void);
        static int watched;
        if (!watched) { watched = 1; recomp_sampler_watch_current_thread(); }
    }
    g_eax = MEM32(MEM32(0x351F48u) + 0x17C8u);
    g_ecx = MEM32(g_eax + 0x100410u) | 0x10000u;
    _mm_sfence();
    {   /* the executor's lag behind the push pointer about to be kicked. Both
         * sides are physical offsets (VA & 0x0FFFFFFF): the title writes PUT
         * that way, and the channel's GET is read back in the same space. The
         * title's own wait at 0x001F3948 masks both with 0xFFFFFFF, so bit 31
         * is not significant on either side; mask GET too, or it compares
         * differently from PUT whenever the GPU's readback sets bit 31. */
        uint32_t dev = MEM32(0x351F48u), regs = MEM32(dev + 0x17C0u);
        uint32_t cur = MEM32(dev) & 0x0FFFFFFFu, get = MEM32(regs + 0x44u) & 0x0FFFFFFFu;
        if (cur < get || cur - get > MM3_KICK_LAG) {
            MEM32(g_eax + 0x100410u) = g_ecx;
            mm3_kick_wait_ack(g_eax + 0x100410u, cur, get);
        }
    }
    g_esp += 4;
}
