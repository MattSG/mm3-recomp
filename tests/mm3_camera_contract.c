/* Native caller/matrix integration contract for the production camera hooks.
 * The common chase solver is a capture boundary: its interpolation/collision
 * logic remains retail. Everything feeding it is the real generated code. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "recomp_types.h"

ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi;
RECOMP_TLS uint32_t g_ebp, g_seh_ebp;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top, g_fp_cmp, g_df;
RECOMP_TLS uint16_t g_fp_cc, g_fp_control_word;
static unsigned char memory[0x440000];
static const uint32_t stack = 0x80000, camera = 0x10000, params = 0x14000;
static uint32_t captured[14];
static unsigned solver_calls, hud_calls, edge_calls;
static int edge_value;
static float display_aspect;

float nv2a_d3d_display_aspect(void) { return display_aspect; }
void mm3_hud_set_edge(int on) { ++edge_calls; edge_value = on; }
void mm3_hud_set_start(float a, float b, float c, float d) {
    assert(a == 50 && b == 320 && c == 178 && d == 448); ++hud_calls;
}
void sub_0011C742_original(void) { g_esp += 8; }
void sub_0011CAAC_original(void) { g_esp += 12; }
void sub_0007CEC3_original(void) { g_esp += 8; }
void sub_001EEC53_original(void) {
    ++solver_calls;
    memcpy(captured, (const void *)XBOX_PTR(g_esp + 4), sizeof captured);
    g_esp += 60; /* Native RET 56. */
}
#include "mods.h"
int mm3_mod_on(int mod) {
    const char *setting = getenv(mod == MM3_MOD_WIDESCREEN ? "MM3_MOD_WIDESCREEN" : "MM3_MOD_FOV");
    return !setting || strcmp(setting, "0") != 0;
}
#include "production_widescreen.c"

#define eax g_eax
#define ecx g_ecx
#define edx g_edx
#define esp g_esp
#define ebx g_ebx
#define esi g_esi
#define edi g_edi
#include "native_camera.inc"

static void close_float(float got, float expected) {
    if (!isfinite(got) || fabsf(got - expected) > 0.00003f) {
        fprintf(stderr, "camera mismatch: got %.9g expected %.9g\n", got, expected);
        abort();
    }
}
static float number(uint32_t word) { float f; memcpy(&f, &word, 4); return f; }

static void reset_call(void) {
    g_esp = stack; g_ebp = 0x70000;
    g_ecx = camera; g_esi = camera; g_ebx = 0x13579; g_edi = 0x24680;
    g_fp_top = 0; g_fp_control_word = 0x37F; g_df = 0;
    memset(g_fp_stack, 0, sizeof g_fp_stack);
    MEM32(stack) = 0xDEADBEEF; MEMF(stack + 4) = 1.0f / 60.0f;
    MEM32(stack + 8) = 0x12345678;
}

static void chase_contract(void) {
    unsigned i, j, repeat;
    uint32_t retail[14];
    unsigned char before[0x40];
    /* Varied per-car eye/goal parameters, plus the native null-parameter
     * defaults. Values also distinguish every coordinate and near/far pair. */
    for (i = 0; i < 6; ++i) {
        for (j = 0; j < 0x40; j += 4) MEMF(params + j) = (float)j * 0.07f + (float)i;
        MEMF(params + 0x20) = 3.68f + (float)i * 0.17f;
        MEMF(params + 0x24) = 3.85f + (float)i * 0.23f;
        MEMF(params + 0x34) = 6.98f + (float)i * 0.29f;
        MEMF(params + 0x38) = 7.81f + (float)i * 0.31f;
        MEM32(camera + 0x80) = i ? params : 0;
        memcpy(before, (const void *)XBOX_PTR(params), sizeof before);
        reset_call(); retail_near(); memcpy(retail, captured, sizeof retail);
        assert(g_esp == stack + 8 && g_fp_top == 0);
        for (repeat = 0; repeat < 100; ++repeat) {
            unsigned calls = solver_calls;
            reset_call(); sub_001EF87E();
            assert(solver_calls == calls + 1 && g_esp == stack + 8 && g_fp_top == 0);
            assert(g_ebx == 0x13579 && g_edi == 0x24680 && MEM32(stack + 8) == 0x12345678);
            for (j = 0; j < 14; ++j) {
                if (j == 4 || j == 7) close_float(number(captured[j]), number(retail[j]) - 0.25f);
                else assert(captured[j] == retail[j]);
            }
            assert(!memcmp(before, (const void *)XBOX_PTR(params), sizeof before));
        }
        reset_call(); retail_far(); memcpy(retail, captured, sizeof retail);
        reset_call(); sub_001EF738();
        assert(g_esp == stack + 8 && g_fp_top == 0 && !memcmp(retail, captured, sizeof retail));
        assert(!memcmp(before, (const void *)XBOX_PTR(params), sizeof before));
    }
    /* Unknown caller, forward-facing and nonfinite distances are untouched. */
    for (i = 0; i < 4; ++i) {
        reset_call();
        MEM32(stack) = i ? 0x001EF922 : 0x11111111;
        for (j = 0; j < 14; ++j) MEMF(stack + 4 + j * 4) = (float)j + 1.0f;
        MEMF(stack + 0x14) = i == 2 ? NAN : -4.0f;
        MEMF(stack + 0x20) = i == 1 ? 5.0f : (i == 3 ? INFINITY : -5.0f);
        memcpy(retail, (const void *)XBOX_PTR(stack + 4), sizeof retail);
        sub_001EEC53();
        assert(g_esp == stack + 60 && !memcmp(retail, captured, sizeof retail));
    }
}

static float vertical(float horizontal, float aspect) {
    return 2.0f * atanf(tanf(horizontal * (3.14159265358979323846f / 360.0f)) / aspect);
}
static void projection_contract(float override) {
    static const float aspects[] = {4.0f / 3.0f, 1.6f, 16.0f / 9.0f, 21.0f / 9.0f};
    static const uint32_t callers[] = {0x00210E1E, 0x000E51EE, 0x001EC9C2, 0x001ECA3A, 0x12345678};
    unsigned a, pass, k;
    int widescreen = mm3_mod_on(MM3_MOD_WIDESCREEN);
    const char *setting = mm3_mod_on(MM3_MOD_FOV) ? getenv("MM3_FOV") : NULL;
    float percentage = setting ? (float)atof(setting) / 100.0f : 1.0f;
    if (!isfinite(percentage) || percentage < 1.0f) percentage = 1.0f;
    if (percentage > 1.5f) percentage = 1.5f;
    for (a = 0; a < sizeof aspects / sizeof aspects[0]; ++a) {
        display_aspect = aspects[a];
        for (pass = 0; pass < 3; ++pass) {
            float input_aspect = pass == 0 ? 4.0f / 3.0f : (pass == 1 ? display_aspect : 1.0f);
            for (k = 0; k < sizeof callers / sizeof callers[0]; ++k) {
                float input_fovy = vertical(65.0f, input_aspect);
                float final_aspect = pass == 2 || !widescreen ? input_aspect : display_aspect;
                float expected_fovy = input_fovy;
                int race = k == 0;
                if (race && override) expected_fovy = vertical(override, final_aspect);
                else if (race && widescreen && pass == 1) expected_fovy = vertical(65.0f, 4.0f / 3.0f);
                if (race && !override) expected_fovy = 2.0f * atanf(tanf(expected_fovy * 0.5f) * percentage);
                reset_call(); hud_calls = edge_calls = 0;
                MEM32(stack) = callers[k];
                MEMF(stack + 4) = input_fovy; MEMF(stack + 8) = input_aspect;
                MEMF(stack + 12) = 0.2f; MEMF(stack + 16) = 1000.0f;
                MEM32(stack + 20) = 0x76543210;
                native_projection_setup();
                assert(g_esp == stack + 20 && g_fp_top == 0 && MEM32(stack + 20) == 0x76543210);
                assert(g_ebx == 0x13579 && g_edi == 0x24680);
                close_float(MEMF(camera + 0x1FA4), expected_fovy);
                close_float(MEMF(camera + 0x1FA8), final_aspect);
                close_float(MEMF(camera + 0x1EE4), 1.0f / tanf(expected_fovy * 0.5f) / final_aspect);
                close_float(MEMF(camera + 0x1EF8), 1.0f / tanf(expected_fovy * 0.5f));
                /* The visibility cone and the actual native matrix have the
                 * same screen-edge slope, not just the same requested FOV. */
                close_float(tanf(MEMF(camera + 0x1FA4) * 0.5f) * MEMF(camera + 0x1FA8),
                            1.0f / MEMF(camera + 0x1EE4));
                assert(hud_calls == (unsigned)(race && widescreen));
                assert(edge_calls == (unsigned)((race && widescreen) || k == 1));
                if (edge_calls) assert(edge_value == (race && widescreen));
            }
        }
        /* A direct non-camera projection must not interpret an arbitrary
         * caller frame, change FOV, or write after native RET 20 consumes args. */
        for (pass = 0; pass < 2; ++pass) {
            float input_aspect = pass ? 4.0f / 3.0f : 1.0f;
            float final_aspect = pass && widescreen ? display_aspect : input_aspect;
            reset_call(); g_ebp = 0xFFFFFFF0; hud_calls = edge_calls = 0;
            MEM32(stack) = 0x11111111; MEM32(stack + 4) = camera;
            MEMF(stack + 8) = 0.7f; MEMF(stack + 12) = input_aspect;
            MEMF(stack + 16) = 0.2f; MEMF(stack + 20) = 1000.0f;
            for (k = 24; k < 48; k += 4) MEM32(stack + k) = 0xA55AA55A;
            sub_0025968E();
            assert(g_esp == stack + 24 && g_fp_top == 0 && !hud_calls && !edge_calls);
            close_float(MEMF(camera), 1.0f / tanf(0.35f) / final_aspect);
            for (k = 24; k < 48; k += 4) assert(MEM32(stack + k) == 0xA55AA55A);
        }
    }
}

int main(int argc, char **argv) {
    float override;
    assert(argc == 2);
    override = strtof(argv[1], NULL);
    g_xbox_mem_offset = (ptrdiff_t)memory;
    native_constants();
    chase_contract(); projection_contract(override);
    printf("PASS camera: near-only +0.25m, native matrix/culling, FOV %.3g, four aspects, ABI guards\n", override);
    return 0;
}
