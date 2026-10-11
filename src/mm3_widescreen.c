#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "recomp_types.h"

/* Widescreen (Hor+). MM3 has no 16:9 mode: every 3D camera builds its
 * projection through 0x25968E, D3DXMatrixPerspectiveFovLH(out, fovy, aspect,
 * zn, zf), with aspect read from the 4:3 constant at 0x385F20. Replacing that
 * argument with the display aspect keeps the vertical field of view and widens
 * the horizontal one, and the game culls against the same wider frustum, so
 * nothing pops in at the new screen edges. The renderer stretches display-
 * sized surfaces to that aspect and keeps 2D overlays at their own proportions
 * (nv2a_d3d11.c). Other aspects (square environment maps) are left alone.
 * The 0x385F20 constant itself is also read as a 4/3 scale by unrelated code
 * (0x220B1E), which is why the argument is patched rather than the data. */
extern void sub_0025968E_original(void);
extern float nv2a_d3d_display_aspect(void);
extern void nv2a_d3d_set_edge_hud(int on);
extern void nv2a_d3d_set_hud_start(float x0, float y0, float x1, float y1);
extern void nv2a_d3d_set_centered_ui_query(int (*query)(void));

static float camera_fov_degrees(void)
{
    static int loaded;
    static float degrees;
    if (!loaded) {
        const char *env = getenv("MM3_FOV_DEGREES");
        if (env && *env) {
            char *end;
            float value = strtof(env, &end);
            if (end == env || *end || !isfinite(value) || value < 60.0f || value > 110.0f)
                fprintf(stderr, "[MM3_CAMERA] invalid MM3_FOV_DEGREES; expected 60..110, using stock FOV\n");
            else {
                degrees = value;
                fprintf(stderr, "[MM3_CAMERA] horizontal field of view %.3g degrees\n", degrees);
            }
        }
        loaded = 1;
    }
    return degrees;
}

extern void sub_001EEC53_original(void);
void sub_001EEC53(void)
{
    /* Native camera mode 5 (001EF926's dispatch table at 001EFAFE) calls
     * 001EF87E, which supplies the near chase's two rearward eye vectors.
     * Mode 7 (far chase) instead returns to 001EF7DC. Adjust only this
     * call's Z arguments, before the original speed interpolation, damping
     * and collision solver; no vehicle parameters or other views change. */
    if (MEM32(g_esp) == 0x001EF922u) {
        float start_z = MEMF(g_esp + 0x14), end_z = MEMF(g_esp + 0x20);
        if (isfinite(start_z) && isfinite(end_z) && start_z < 0.0f && end_z < 0.0f) {
            MEMF(g_esp + 0x14) = start_z - 0.75f;
            MEMF(g_esp + 0x20) = end_z - 0.75f;
        }
    }
    sub_001EEC53_original();
}

static int pause_ui_active(void)
{
    /* PAL pause singleton (001E25C1): open 001E2CD5 sets +2 and resume
     * 001E2BDA clears it. Its controls use several textures/draws, all in
     * the same 4:3 layout; they must not get individual race-HUD anchors.
     * The pushbuffer thread reads the current state without guest calls. */
    return *(const volatile uint8_t *)XBOX_PTR(0x00429DB2u) != 0;
}

void mm3_widescreen_init(void)
{
    /* Register after the XBE is mapped, before the renderer thread starts. */
    nv2a_d3d_set_centered_ui_query(pause_ui_active);
}

void sub_0025968E(void)
{
    float aspect = MEMF(g_esp + 0xC), wide = nv2a_d3d_display_aspect();
    int race = MEM32(g_esp) == 0x000F4AF5u && MEM32(g_ebp + 4) == 0x00210E1Eu;
    int patch = fabsf(aspect - 4.0f / 3.0f) < 1e-4f && fabsf(wide - aspect) > 1e-4f;
    if (patch)
        MEMF(g_esp + 0xC) = wide;
    /* Called from the camera's projection setup (0xF4AC3, returning to
     * 0xF4AF5), which afterwards stores its own fov/aspect arguments at
     * +0x1FA4/+0x1FA8 of the camera. The city's visibility culling reads them
     * back, so widen that argument too or blocks beyond the 4:3 frustum
     * vanish at the sides of the wider view. Its frame is published in g_ebp. */
    if (patch && MEM32(g_esp) == 0x000F4AF5u &&
        fabsf(MEMF(g_ebp + 0xC) - 4.0f / 3.0f) < 1e-4f)
        MEMF(g_ebp + 0xC) = wide;
    /* The race camera (0x210D6F, calling the setup with return 0x210E1E)
     * derives fovy from a horizontal field of view and that stored aspect:
     * fovy = 2 atan(tan(hfov / 2) / aspect). With the stored aspect widened
     * that narrows fovy (Vert-, a zoom). Undo it so the vertical view stays
     * the 4:3 one, for the matrix and for the copy stored for culling.
     * It is also how the renderer knows a race has started, so its HUD can
     * move out to the screen edges (nv2a_d3d11.c); the race camera builds its
     * projection once per race, car select's (0xE51EE) every frame. */
    if (race) {
        /* The race HUD starts with the minimap's base quad; 2D drawn ahead of
         * it is the name tags over cars, positioned through this camera. */
        nv2a_d3d_set_hud_start(50.0f, 320.0f, 178.0f, 448.0f);
        nv2a_d3d_set_edge_hud(1);
    }
    else if (MEM32(g_esp) == 0x000F4AF5u && MEM32(g_ebp + 4) == 0x000E51EEu)
        nv2a_d3d_set_edge_hud(0);              /* car select's turntable */
    if (race) {
        float degrees = camera_fov_degrees();
        if (degrees) {
            /* Absolute horizontal degrees at the actual render aspect.
             * The matrix and the camera's visibility frustum must agree.
             * This explicit renderSetFov call excludes menu turntables,
             * mirrors, cubemaps and temporary projection save/restores. */
            float fovy = 2.0f * atanf(tanf(degrees * (3.14159265358979323846f / 360.0f)) /
                                     MEMF(g_esp + 0xC));
            MEMF(g_esp + 8) = fovy;
            MEMF(g_ebp + 8) = fovy;
        } else if (fabsf(aspect - wide) < 1e-4f && fabsf(wide - 4.0f / 3.0f) > 1e-4f) {
            float fovy = 2.0f * atanf(tanf(MEMF(g_esp + 8) * 0.5f) * wide * 0.75f);
            MEMF(g_esp + 8) = fovy;
            MEMF(g_ebp + 8) = fovy;
        }
    }
    sub_0025968E_original();
    /* Native D3DX consumes all five arguments (ret 20). Nothing remains
     * to restore at the advanced stack pointer. The camera setup copies
     * its separate frame arguments into +1FA4/+1FA8 for visibility. */
}
