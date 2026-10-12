#include <windows.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "nv2a_d3d11.h"

/* The widescreen mod's 2D placement (mods.c installs it as the renderer's
 * overlay hooks; widescreen.c says when a race HUD is up and where it
 * starts). Menus and loading screens span the display; the race HUD keeps
 * its proportions, each piece anchored to its own screen edge, and the name
 * tags over cars keep the position the widened camera projected. */

/* HUD edge placement is latched: the title turns it on when its in-game
 * HUD's screen begins
 * and off when it leaves. Frames that draw a full-screen background
 * image are menus or loading screens and never get it, which keeps menus
 * reached from a race out of the race HUD placement path. */
static volatile LONG s_edge_hud;
static uint32_t s_frame_bg_tex;     /* this frame's background image; reset at flip */
static int s_frame_tag_window;      /* still in the frame's leading name tags */
/* The race HUD draws its minimap (the HUD start rect) every frame it is up;
 * the pause and post-race menus hide it. A frame after one without it is a
 * menu: its 2D keeps 4:3 proportions, centred, instead of being split to
 * the screen edges -- edge placement pulled each button's end caps away
 * from its middle. ponytail: the zoomed map also hides the minimap and so
 * gets centred too; key on the title's pause state if that matters. */
static int s_hud_seen, s_hud_prev;

/* Name tags over cars (world-anchored 2D) are the first 2D a race frame
 * draws, ahead of the HUD; the title names the HUD's first element by its
 * 4:3 rectangle (nv2a_d3d_set_hud_start). Textures drawn from there on are
 * learned as HUD textures. In each race frame the leading draws keep their
 * projected position (hud_split's follow mode) until the HUD start or a
 * known HUD texture -- so frames without the minimap (zoomed map, pause)
 * still end the tags at their first HUD element. */
static float s_hud_start[4];
static uint32_t s_hud_tex[32];
static int s_hud_tex_n;

/* A full-screen quad textured from the race HUD's own atlas is a fade (the title
 * dims the screen this way while it resets the car), not a menu background:
 * taken for one, it put the HUD drawn after it back at 4:3 for the fade --
 * the speedometer's damage bar jumped left of the gauge. */
static int hud_texture(uint32_t tex)
{
    int i;
    for (i = 0; i < s_hud_tex_n; i++)
        if (s_hud_tex[i] == tex)
            return 1;
    return 0;
}

void mm3_hud_set_start(float x0, float y0, float x1, float y1)
{
    s_hud_start[0] = x0; s_hud_start[1] = y0; s_hud_start[2] = x1; s_hud_start[3] = y1;
}

static int world_tag_draw(float lo, float hi, float ylo, float yhi, uint32_t tex)
{
    int i;
    if (!s_hud_start[2])
        return 0;
    if (s_frame_tag_window &&
        fabsf(lo - s_hud_start[0]) < 2.0f && fabsf(ylo - s_hud_start[1]) < 2.0f &&
        fabsf(hi - s_hud_start[2]) < 2.0f && fabsf(yhi - s_hud_start[3]) < 2.0f)
        s_frame_tag_window = 0;
    for (i = 0; i < s_hud_tex_n && s_hud_tex[i] != tex; i++) {}
    if (s_frame_tag_window && i == s_hud_tex_n)
        return 1;
    s_frame_tag_window = 0;
    if (i == s_hud_tex_n && s_hud_tex_n < 32)
        s_hud_tex[s_hud_tex_n++] = tex;
    return 0;
}

void mm3_hud_set_edge(int on)
{
    if (!on)
        InterlockedExchange(&s_edge_hud, 0);
    else if (!InterlockedExchange(&s_edge_hud, 1))
        s_hud_tex_n = 0;                 /* a new level may load other textures */
}

static int hud_active(void)
{
    return !s_frame_bg_tex && InterlockedCompareExchange(&s_edge_hud, 0, 0) != 0;
}

/* The race HUD batches sprites from one texture atlas into a single draw
 * (elements can lie on opposite sides of the screen), so edge placement is decided per cluster: connected primitives form
 * a piece, pieces on the same line less than HUD_GAP apart join a cluster
 * (the glyphs of a centred message stay together), and each cluster moves by
 * its own half of the 4:3 layout. pos holds the screen positions by vertex
 * (index - sub); fills dx with each vertex's x shift in guest pixels and
 * returns 0 when nothing moves, leaving only the shader's 4:3 squeeze. */
#define HUD_GAP 24.0f
static int hud_split(const float (*pos)[4], uint32_t nv, const uint32_t *idx, uint32_t sub,
                     uint32_t ni, uint32_t per, float gw, float narrow, int follow, float *dx)
{
    static uint32_t *root, *piece, cap;
    static float (*box)[4];             /* per piece: x0 x1 y0 y1 */
    uint32_t i, k, np = 0;
    float delta = (1.0f - narrow) * gw / (2.0f * narrow), half = gw * 0.5f;
    int moved = 0;
    if (nv > cap) {
        uint32_t *r = realloc(root, (size_t)nv * sizeof *r);
        uint32_t *pc = r ? realloc(piece, (size_t)nv * sizeof *pc) : NULL;
        float (*b)[4] = pc ? realloc(box, (size_t)nv * sizeof *b) : NULL;
        if (r) root = r;
        if (pc) piece = pc;
        if (!b) return 0;
        box = b;
        cap = nv;
    }
#define FIND(x) do { while (root[x] != x) x = root[x] = root[root[x]]; } while (0)
    /* pieces: vertices joined by primitives */
    for (i = 0; i < nv; i++) root[i] = i;
    for (i = 0; i + per <= ni; i += per)
        for (k = 1; k < per; k++) {
            uint32_t a = idx[i] - sub, b = idx[i + k] - sub;
            if (a >= nv || b >= nv) return 0;
            FIND(a); FIND(b);
            if (a != b) root[b] = a;
        }
    for (i = 0; i < nv; i++) {
        uint32_t r = i;
        FIND(r);
        if (r == i) {
            piece[i] = np;
            box[np][0] = box[np][2] = 1e30f;
            box[np][1] = box[np][3] = -1e30f;
            np++;
        }
    }
    for (i = 0; i < nv; i++) {
        uint32_t r = i, n;
        FIND(r);
        n = piece[r];
        piece[i] = n;
        if (pos[i][0] < box[n][0]) box[n][0] = pos[i][0];
        if (pos[i][0] > box[n][1]) box[n][1] = pos[i][0];
        if (pos[i][1] < box[n][2]) box[n][2] = pos[i][1];
        if (pos[i][1] > box[n][3]) box[n][3] = pos[i][1];
    }
    /* clusters: pieces on one line within HUD_GAP (root[] reused per piece);
     * ponytail: O(pieces^2), fine for HUD batches of a few hundred glyphs */
    for (i = 0; i < np; i++) root[i] = i;
    for (i = 0; i < np; i++)
        for (k = i + 1; k < np; k++) {
            uint32_t a = i, b = k;
            /* a full-screen quad overlaps every piece: it stays its own */
            if ((box[i][0] <= 0.5f && box[i][1] >= gw - 0.5f) ||
                (box[k][0] <= 0.5f && box[k][1] >= gw - 0.5f))
                continue;
            if (box[i][2] > box[k][3] || box[k][2] > box[i][3] ||
                box[i][0] > box[k][1] + HUD_GAP || box[k][0] > box[i][1] + HUD_GAP)
                continue;
            FIND(a); FIND(b);
            if (a == b) continue;
            root[b] = a;
            if (box[b][0] < box[a][0]) box[a][0] = box[b][0];
            if (box[b][1] > box[a][1]) box[a][1] = box[b][1];
            if (box[b][2] < box[a][2]) box[a][2] = box[b][2];
            if (box[b][3] > box[a][3]) box[a][3] = box[b][3];
        }
    for (i = 0; i < nv; i++) {
        uint32_t c = piece[i];
        FIND(c);
        dx[i] = 0.0f;
        if (follow) {
            /* keep the cluster's centre where the title projected it on
             * the full width; the shader squeezes it to 4:3 about there */
            dx[i] = ((box[c][0] + box[c][1]) * 0.5f - half) * (1.0f / narrow - 1.0f);
            moved = 1;
        } else if (box[c][0] <= 0.5f && box[c][1] >= gw - 0.5f) {
            /* a full-screen quad in the batch (the fade the title draws over
             * a car reset, in the same call as the needle and damage bar):
             * undo the 4:3 squeeze so it still covers the whole width */
            dx[i] = (pos[i][0] - half) * (1.0f / narrow - 1.0f);
            moved = 1;
        } else if (box[c][1] <= half) { dx[i] = -delta; moved = 1; }
        else if (box[c][0] >= half) { dx[i] = delta; moved = 1; }
    }
#undef FIND
    return moved;
}

static void frame(void)
{
    s_frame_bg_tex = 0;
    s_frame_tag_window = 1;
    s_hud_prev = s_hud_seen;
    s_hud_seen = 0;
}

static int place(const NvOverlayDraw *d, float *dx, float *squeeze, float *shift)
{
    float xlo = d->x0, xhi = d->x1, ylo = d->y0, yhi = d->y1, narrow = d->narrow, gw = d->gw;
    int wide_x = xlo <= 0.5f && xhi >= gw - 0.5f;
    int tall = ylo <= 0.5f && yhi >= d->gh - 0.5f;
    int use_dx = 0;
    /* Remember the background only to exclude menus/loading screens
     * reached during a race from HUD placement. Do not zoom its art. */
    if (tall && wide_x && d->textured && !d->reads_wide && !s_frame_bg_tex && !hud_texture(d->tex))
        s_frame_bg_tex = d->tex;
    if (s_hud_start[2] && fabsf(xlo - s_hud_start[0]) < 2.0f && fabsf(ylo - s_hud_start[1]) < 2.0f &&
        fabsf(xhi - s_hud_start[2]) < 2.0f && fabsf(yhi - s_hud_start[3]) < 2.0f)
        s_hud_seen = 1;
    if (hud_active() && !(wide_x && (!d->textured || d->reads_wide))) {
        *squeeze = narrow;
        if (!s_hud_prev && !s_hud_seen) {
            /* a menu over the race: centred 4:3, kept whole */
        } else if (world_tag_draw(xlo, xhi, ylo, yhi, d->tex)) {
            /* a name tag over a car: the title projected it
             * through the widened camera, keep that position */
            use_dx = dx && hud_split(d->pos, d->count, d->idx, d->lo, d->ni, d->per, gw, narrow, 1, dx);
        } else if (hud_active()) {
            if (xhi <= gw * 0.5f) *shift = narrow - 1.0f;
            else if (xlo >= gw * 0.5f) *shift = 1.0f - narrow;
            else use_dx = dx && hud_split(d->pos, d->count, d->idx, d->lo, d->ni, d->per, gw, narrow, 0, dx);
        }
    }
    return use_dx;
}

/* The race camera's projection is built once per race: an aspect change
 * waits until it is gone rather than stretch the race view. */
static int hold_aspect(void)
{
    return InterlockedCompareExchange(&s_edge_hud, 0, 0) != 0;
}

const NvOverlayHooks mm3_hud_hooks = { frame, place, hold_aspect };
