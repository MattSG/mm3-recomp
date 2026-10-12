#include <math.h>
#include <stdlib.h>
#include "recomp_types.h"
#include "mods.h"

/* Widescreen (Hor+). MM3 has no 16:9 mode: every 3D camera builds its
 * projection through 0x25968E, D3DXMatrixPerspectiveFovLH(out, fovy, aspect,
 * zn, zf), with aspect read from the 4:3 constant at 0x385F20. Replacing that
 * argument with the display aspect keeps the vertical field of view and widens
 * the horizontal one, and the game culls against the same wider frustum, so
 * nothing pops in at the new screen edges. The renderer stretches display-
 * sized surfaces to that aspect; widescreen_hud.c places the 2D overlays.
 * Other aspects (square environment maps) are left alone.
 * The 0x385F20 constant itself is also read as a 4/3 scale by unrelated code
 * (0x220B1E), which is why the argument is patched rather than the data. */
extern void sub_0025968E_original(void);
extern float nv2a_d3d_display_aspect(void);
extern void mm3_hud_set_edge(int on);                                   /* widescreen_hud.c */
extern void mm3_hud_set_start(float x0, float y0, float x1, float y1);

/* The field-of-view mod: MM3_FOV percent (100-150) widens the race camera,
 * tan(fovy/2) scaled, in the matrix and in the copy the culling reads. */
static float fov_scale(void)
{
    static float k = -1.0f;
    if (k < 0.0f) {
        const char *v = getenv("MM3_FOV");
        k = v && mm3_mod_on(MM3_MOD_FOV) ? (float)atof(v) / 100.0f : 1.0f;
        k = k < 1.0f ? 1.0f : k > 1.5f ? 1.5f : k;
    }
    return k;
}

static void widen_race_fov(void)
{
    if (MEM32(g_esp) == 0x000F4AF5u && MEM32(g_ebp + 4) == 0x00210E1Eu && fov_scale() > 1.0f) {
        float fovy = 2.0f * atanf(tanf(MEMF(g_esp + 8) * 0.5f) * fov_scale());
        MEMF(g_esp + 8) = fovy;
        MEMF(g_ebp + 8) = fovy;
    }
}

void sub_0025968E(void)
{
    if (!mm3_mod_on(MM3_MOD_WIDESCREEN)) {
        widen_race_fov();
        sub_0025968E_original();
        return;
    }
    float aspect = MEMF(g_esp + 0xC), wide = nv2a_d3d_display_aspect();
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
     * It is also how widescreen_hud.c knows a race has started, so its HUD
     * can move out to the screen edges; the race camera builds its
     * projection once per race, car select's (0xE51EE) every frame. */
    if (MEM32(g_esp) == 0x000F4AF5u && MEM32(g_ebp + 4) == 0x00210E1Eu) {
        /* The race HUD starts with the minimap's base quad; 2D drawn ahead of
         * it is the name tags over cars, positioned through this camera. */
        mm3_hud_set_start(50.0f, 320.0f, 178.0f, 448.0f);
        mm3_hud_set_edge(1);
    }
    else if (MEM32(g_esp) == 0x000F4AF5u && MEM32(g_ebp + 4) == 0x000E51EEu)
        mm3_hud_set_edge(0);                   /* car select's turntable */
    if (MEM32(g_esp) == 0x000F4AF5u && MEM32(g_ebp + 4) == 0x00210E1Eu &&
        fabsf(aspect - wide) < 1e-4f && fabsf(wide - 4.0f / 3.0f) > 1e-4f) {
        float fovy = 2.0f * atanf(tanf(MEMF(g_esp + 8) * 0.5f) * wide * 0.75f);
        MEMF(g_esp + 8) = fovy;
        MEMF(g_ebp + 8) = fovy;
    }
    widen_race_fov();
    sub_0025968E_original();
    /* cdecl: the caller owns its argument area; give it back unchanged. */
    if (patch)
        MEMF(g_esp + 0x8) = aspect;
}
