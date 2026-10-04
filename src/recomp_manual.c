#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#include "recomp_types.h"

typedef void (*recomp_func_t)(void);

void recomp_unimpl(const char *text, uint32_t va)
{
    static int printed;
    const char *trap = getenv("RECOMP_UNIMPL_TRAP");
    int stop = trap && *trap && *trap != '0';

    if (printed < 50 || stop) {
        printed++;
        fprintf(stderr,
                "[UNIMPL] untranslated instruction REACHED: `%s` at 0x%08X"
                " (a no-op; set RECOMP_UNIMPL_TRAP=1 to stop here)\n",
                text, va);
        fflush(stderr);
    }
    if (stop) abort();
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
void sub_000858F3(void)
{
    static unsigned calls;
    if (calls++ < 8)
        fprintf(stderr, "[858F3_ENTRY] call=%u ret=%08X args=%08X,%08X,%08X esi=%08X edi=%08X\n",
                calls, MEM32(g_esp), MEM32(g_esp + 4), MEM32(g_esp + 8),
                MEM32(g_esp + 12), g_esi, g_edi);
    call_alternate_seh_preserving(sub_000858F3_gen);
}
void sub_000860AA(void) { call_alternate_seh_preserving(sub_000860AA_gen); }
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
 if (getenv("MM3_STREAM_TRACE")) {
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

/* Project-side fallbacks for the two string thunks used by title-data lookup.
 * The linked kernel archive currently exposes these as zero-return stubs. */
static void bridge_rtl_init_ansi_string(void)
{
    uint32_t dest = MEM32(g_esp + 4u);
    uint32_t src = MEM32(g_esp + 8u);
    uint32_t len = 0;

    if (dest) {
        if (src) {
            while (len < 0xFFFEu && MEM8(src + len) != 0)
                ++len;
            MEM16(dest) = (uint16_t)len;
            MEM16(dest + 2u) = (uint16_t)(len + 1u);
            MEM32(dest + 4u) = src;
        } else {
            MEM16(dest) = 0;
            MEM16(dest + 2u) = 0;
            MEM32(dest + 4u) = 0;
        }
    }
    g_eax = 0;
    g_esp += 12u;
}

static void bridge_rtl_equal_string(void)
{
    uint32_t s1 = MEM32(g_esp + 4u);
    uint32_t s2 = MEM32(g_esp + 8u);
    uint32_t nocase = MEM32(g_esp + 12u);
    uint32_t len1 = s1 ? MEM16(s1) : 0;
    uint32_t len2 = s2 ? MEM16(s2) : 0;
    uint32_t i;

    g_eax = (s1 && s2 && len1 == len2) ? 1u : 0u;
    for (i = 0; g_eax && i < len1; ++i) {
        uint8_t a = MEM8(MEM32(s1 + 4u) + i);
        uint8_t b = MEM8(MEM32(s2 + 4u) + i);
        if (nocase) {
            if (a >= 'a' && a <= 'z') a = (uint8_t)(a - 'a' + 'A');
            if (b >= 'a' && b <= 'z') b = (uint8_t)(b - 'a' + 'A');
        }
        if (a != b)
            g_eax = 0;
    }
    g_esp += 16u;
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
    if (xbox_va == 0xFE000068u)
        return bridge_rtl_init_ansi_string;
    if (xbox_va == 0xFE00017Cu)
        return bridge_rtl_equal_string;
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

    /* MM3's startup TLS slot holds the worker frame at EBP+0x10. */
    MEM32(MEM32(XBOX_FS_BASE + 0x28u) + 0x28u) = ebp + 0x10u;

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

void recomp_icall_fail_log(uint32_t xbox_va)
{
    uint32_t caller = 0;
    if (g_esp >= 0x00010000u && g_esp < 0x04000000u)
        caller = *(const uint32_t *)((uintptr_t)g_esp + g_xbox_mem_offset);
    fprintf(stderr, "[ICALL] Failed resolve VA 0x%08X caller=0x%08X\n",
            xbox_va, caller);
}

void recomp_icall_not_code_log(uint32_t xbox_va)
{
    uint32_t caller = 0;
    uint32_t slot = 0;
    for (uint32_t va = 0x00392834u; va < 0x00392848u; va += 4) {
        if (MEM32(va) == xbox_va) {
            slot = va;
            break;
        }
    }
    if (slot == 0) {
        for (uint32_t va = 0x00391F10u; va < 0x00392830u; va += 4) {
            if (MEM32(va) == xbox_va) {
                slot = va;
                break;
            }
        }
    }
    if (g_esp >= 0x00010000u && g_esp < 0x04000000u)
        caller = MEM32(g_esp);
    fprintf(stderr, "[ICALL] non-code target 0x%08X slot=0x%08X caller=0x%08X esp=0x%08X\n",
            xbox_va, slot, caller, g_esp);
    if (getenv("MM3_FRONTEND_ICALL_TRACE") &&
        (caller == 0x0011FB9Cu || caller == 0x000FB05Eu)) {
        static unsigned samples;
        if (samples++ < 8) {
            fprintf(stderr, "[FRONTEND_ICALL] eax=%08X ecx=%08X edx=%08X esi=%08X edi=%08X\n",
                    g_eax, g_ecx, g_edx, g_esi, g_edi);
            uint32_t pointers[] = {g_eax, g_ecx, g_esi, g_edi};
            for (unsigned i = 0; i < 4; ++i) {
                uint32_t p = pointers[i];
                if (p >= 0x10000u && p < 0x04000000u - 0x80u) {
                    fprintf(stderr, "[FRONTEND_ICALL] object %08X:", p);
                    for (unsigned j = 0; j < 32; ++j)
                        fprintf(stderr, " %08X", MEM32(p + 4u * j));
                    fprintf(stderr, "\n");
                }
            }
        }
    }
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
