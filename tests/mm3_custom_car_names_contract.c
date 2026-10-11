/* Offline ABI/ownership seam contract. Compile with the actual project
 * mm3_recomp_types.h copied as recomp_types.h on the include path. */
#include <assert.h>
#include <stdio.h>
#include "../src/mm3_custom_car_names.c"

ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi;
RECOMP_TLS uint32_t g_ebp, g_seh_ebp, g_fs_base;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top, g_fp_cmp, g_df;
RECOMP_TLS uint16_t g_fp_cc, g_fp_control_word;
RECOMP_TLS RecompXmm g_xmm0, g_xmm1, g_xmm2, g_xmm3;
RECOMP_TLS RecompXmm g_xmm4, g_xmm5, g_xmm6, g_xmm7;

static unsigned char memory[0x10000];
static unsigned original_calls, constructor_calls;
static const uint32_t stack = 0xD000, output = 0x1000, key = 0x2000;
static const uint32_t heap_text = 0x3000;

static void clobber_state(void)
{
    g_eax = g_ecx = g_edx = 0xBAD;
    g_ebx = g_esi = g_edi = 0xBAD;
    g_ebp = g_seh_ebp = g_fs_base = 0xBAD;
    g_fp_top = g_fp_cmp = g_df = 7;
    g_fp_control_word = g_fp_cc = 0xBAD;
    memset(g_fp_stack, 0xBB, sizeof g_fp_stack);
    memset(&g_xmm0, 0xBB, sizeof g_xmm0);
    memset(&g_xmm1, 0xBB, sizeof g_xmm1);
    memset(&g_xmm2, 0xBB, sizeof g_xmm2);
    memset(&g_xmm3, 0xBB, sizeof g_xmm3);
    memset(&g_xmm4, 0xBB, sizeof g_xmm4);
    memset(&g_xmm5, 0xBB, sizeof g_xmm5);
    memset(&g_xmm6, 0xBB, sizeof g_xmm6);
    memset(&g_xmm7, 0xBB, sizeof g_xmm7);
}

void sub_0011C194_original(void)
{
    assert(g_esp == stack && MEM32(stack + 4) == output);
    assert(MEM32(stack + 8) == key);
    ++original_calls;
    MEM32(output) = 0x12345678;
    clobber_state();
    g_eax = output;
    g_esp = stack + 12;
}

void sub_00016148(void)
{
    uint32_t source = MEM32(g_esp + 4);
    size_t i;
    assert(g_eax == output);
    assert(MEM32(g_esp) == 0x0011C1BEu);
    assert(source == stack - 64 && source > g_esp);
    ++constructor_calls;
    /* The real guest constructor copies text into guest-owned storage.
     * The seam fixture models that copy, preventing a stack-pointer result. */
    for (i = 0; MEM16(source + (uint32_t)i * 2u); ++i)
        MEM16(heap_text + (uint32_t)i * 2u) = MEM16(source + (uint32_t)i * 2u);
    MEM16(heap_text + (uint32_t)i * 2u) = 0;
    MEM32(output + 4) = heap_text;
    MEM32(output + 0x14) = (uint32_t)i;
    MEM32(output + 0x18) = 15;
    g_esp += 8;
    clobber_state();
}

static void initialize(const char *name)
{
    unsigned i;
    memset(memory, 0xA5, sizeof memory);
    g_xbox_mem_offset = (ptrdiff_t)memory;
    memcpy((void *)XBOX_PTR(key), name, strlen(name) + 1u);
    MEM32(stack) = 0x1044D3;
    MEM32(stack + 4) = output;
    MEM32(stack + 8) = key;
    g_esp = stack; g_eax = 0x101; g_ecx = 0x102; g_edx = 0x103;
    g_ebx = 0x104; g_esi = 0x105; g_edi = 0x106;
    g_ebp = 0x107; g_seh_ebp = 0x108; g_fs_base = 0x109;
    g_fp_top = 2; g_fp_cmp = 3; g_df = 0;
    g_fp_control_word = 0x37F; g_fp_cc = 0x4000;
    for (i = 0; i < 8; ++i) g_fp_stack[i] = 1.5 + i;
    memset(&g_xmm0, 0x11, sizeof g_xmm0); memset(&g_xmm1, 0x12, sizeof g_xmm1);
    memset(&g_xmm2, 0x13, sizeof g_xmm2); memset(&g_xmm3, 0x14, sizeof g_xmm3);
    memset(&g_xmm4, 0x15, sizeof g_xmm4); memset(&g_xmm5, 0x16, sizeof g_xmm5);
    memset(&g_xmm6, 0x17, sizeof g_xmm6); memset(&g_xmm7, 0x18, sizeof g_xmm7);
    original_calls = constructor_calls = 0;
}

int main(void)
{
    static const char *expected_names[] = {"MINI Rally", "Mustang GT", "Viper Race",
        "Panoz GTR-1", "McLaren F1", "Formula 1", "Bugatti Chiron", "Bugatti Bolide", "Ford GT"};
    static const char *stock[] = {"SID_CAR_SHORT_12", "SID_CAR_MF_8",
        "SID_CAR_MODEL_11", "SID_CAR_SHORT_90010", "SID_CAR_MODEL_9003_EXTRA",
        "SID_CAR_SHORT_900", "", "SID_OPTIONS"};
    size_t i, j;
    assert(sizeof car_names / sizeof car_names[0] == 27);
    for (i = 0; i < sizeof expected_names / sizeof expected_names[0]; ++i) {
        char expected_key[32];
        snprintf(expected_key, sizeof expected_key, "SID_CAR_SHORT_%u", 9001u + (unsigned)i);
        assert(strcmp(car_names[i * 3u + 2u].key, expected_key) == 0);
        assert(strcmp(car_names[i * 3u + 2u].value, expected_names[i]) == 0);
    }
    for (i = 0; i < sizeof car_names / sizeof car_names[0]; ++i) {
        assert(strlen(car_names[i].value) < 32); /* reserved UTF-16 scratch */
        initialize(car_names[i].key);
        sub_0011C194();
        assert(original_calls == 0 && constructor_calls == 1);
        assert(g_esp == stack + 12 && g_eax == output);
        assert(g_ecx == 0x102 && g_edx == 0x103 && g_ebx == 0x104);
        assert(g_esi == 0x105 && g_edi == 0x106 && g_ebp == 0x107);
        assert(g_seh_ebp == 0x108 && g_fs_base == 0x109);
        assert(g_fp_top == 2 && g_fp_cmp == 3 && g_df == 0);
        assert(g_fp_control_word == 0x37F && g_fp_cc == 0x4000);
        for (j = 0; j < 8; ++j) assert(g_fp_stack[j] == 1.5 + j);
        assert(g_xmm0.u[0] == 0x11111111 && g_xmm1.u[0] == 0x12121212);
        assert(g_xmm2.u[0] == 0x13131313 && g_xmm3.u[0] == 0x14141414);
        assert(g_xmm4.u[0] == 0x15151515 && g_xmm5.u[0] == 0x16161616);
        assert(g_xmm6.u[0] == 0x17171717 && g_xmm7.u[0] == 0x18181818);
        assert(MEM32(output + 4) == heap_text); /* no scratch pointer escapes */
        memset((void *)XBOX_PTR(stack - 64), 0xCC, 64);
        for (j = 0; j <= strlen(car_names[i].value); ++j)
            assert(MEM16(heap_text + (uint32_t)j * 2u) == (unsigned char)car_names[i].value[j]);
        assert(MEM32(stack) == 0x1044D3 && MEM32(stack + 4) == output);
        assert(MEM32(stack + 8) == key);
    }
    for (i = 0; i < sizeof stock / sizeof stock[0]; ++i) {
        initialize(stock[i]);
        sub_0011C194();
        assert(original_calls == 1 && constructor_calls == 0);
        assert(g_esp == stack + 12 && g_eax == output);
        assert(g_ecx == 0xBAD && g_ebx == 0xBAD && g_fp_top == 7);
        assert(MEM32(output) == 0x12345678);
    }
    puts("PASS: 27 custom car labels, RET 8, guest string ownership, full register state and exact stock passthrough");
    return 0;
}
