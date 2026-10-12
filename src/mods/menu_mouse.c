#include <math.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* The menu mouse mod (mods.c): which menu item the pointer is on, and how
 * far that is from the focused one, read off the frame the renderer drew.
 *
 * MM3's menus are columns of same-height buttons sharing a left edge, right
 * edge or centre (the pause menu). Every row draws the same quads -- a
 * shadow, the blue pill, and a silver overlay pill -- and the vertex program
 * fades the overlay in on the focused row and out of the rest. So the focus
 * is the row where a texture every row draws comes out clearly more opaque;
 * nothing else about the rows differs (not texture, vertex colour, combiner
 * state or blending). The fade takes a few hundred ms, during which no row
 * stands out and the answer is "not known yet". */

#include <windows.h>
#include "nv2a_d3d11.h"

#define RECTS 1024
#define ROWS 32
#define ROW_TEXS 16

/* Car select's strip: same-size dimmed cards on one row, the chosen car a
 * larger opaque card in a gap among them, an arrow quad at either end. It
 * scrolls, so it is clicked, not hovered: 2 with *offset cars right of the
 * chosen one (negative left), or 0 when the pointer is not on such a strip. */
static int strip_pointer(const NvMenuRect *r, int n, const NvMenuRect *hit, int *offset)
{
    float hw = hit->x1 - hit->x0, hh = hit->y1 - hit->y0, cx = (hit->x0 + hit->x1) * 0.5f;
    float cards[ROWS], fx = 0;
    int ncards = 0, focus = 0, i, j, before = 0, after = 0;
    const NvMenuRect *card = NULL;
    /* The card the pointer is on, or the row's cards beside an arrow. */
    for (i = 0; i < n && !card; i++) {
        float w = r[i].x1 - r[i].x0, h = r[i].y1 - r[i].y0;
        if (w >= h * 3 && h >= 12 && (r[i].argb >> 24) < 0xC0 &&
            r[i].y0 < hit->y1 && r[i].y1 > hit->y0)
            card = &r[i];
    }
    if (!card)
        return 0;
    hw = card->x1 - card->x0; hh = card->y1 - card->y0;
    for (i = 0; i < n && ncards < ROWS; i++) {
        float c = (r[i].x0 + r[i].x1) * 0.5f;
        if (fabsf(r[i].x1 - r[i].x0 - hw) > 2 || fabsf(r[i].y1 - r[i].y0 - hh) > 2 ||
            fabsf(r[i].y0 - card->y0) > 2)
            continue;
        for (j = 0; j < ncards && fabsf(cards[j] - c) > 4; j++) {}
        if (j == ncards) cards[ncards++] = c;
    }
    /* The chosen car: bigger than a card, opaque, over the same row. */
    for (i = 0; i < n && !focus; i++) {
        float w = r[i].x1 - r[i].x0, h = r[i].y1 - r[i].y0;
        if (w > hw * 1.5f && h > hh * 1.5f && (r[i].argb >> 24) >= 0xF0 && w >= h * 3 &&
            r[i].y0 <= card->y0 && r[i].y1 >= card->y1) {
            fx = (r[i].x0 + r[i].x1) * 0.5f;
            focus = 1;
        }
    }
    if (ncards < 3 || !focus)
        return 0;
    for (i = 0; i < ncards; i++) {
        before += cards[i] < fx;
        after += cards[i] > fx;
    }
    if (!before && !after)   /* locked cars past the end draw differently */
        return 0;
    /* An arrow: square, at the row, not a card. */
    if (fabsf(hit->x1 - hit->x0 - (hit->y1 - hit->y0)) < 8 && (hit->x1 - hit->x0) >= 40 &&
        !(fabsf(hit->x1 - hit->x0 - hw) <= 2 && fabsf(hit->y1 - hit->y0 - hh) <= 2)) {
        *offset = cx < fx ? -1 : 1;
        return 2;
    }
    if (fabsf(hit->x1 - hit->x0 - hw) > 2 || fabsf(hit->y0 - card->y0) > 2)
        return 0;
    /* The cards between the chosen car and this one, this one included. */
    *offset = 0;
    for (i = 0; i < ncards; i++) {
        if (cx < fx && cards[i] < fx && cards[i] >= cx - 4) (*offset)--;
        if (cx > fx && cards[i] > fx && cards[i] <= cx + 4) (*offset)++;
    }
    return *offset ? 2 : 0;
}

/* For a client-area fraction: 0 off any item, 1 on one (*offset rows below
 * the focused item, negative above), -1 on one whose focus is not known,
 * 2 on a strip that moves by *offset items sideways when clicked. */
int mm3_menu_pointer(float cx, float cy, int *offset)
{
    static NvMenuRect r[RECTS];                  /* input thread only */
    NvMenuRect *hit = NULL;
    float present[4], gx, gy, ys[ROWS], hh, left, right;
    uint32_t texs[ROWS][ROW_TEXS], alpha[ROWS][ROW_TEXS];
    int nt[ROWS], n, i, j, k, rows = 0, hovered, focused = -1, votes = 0, disagree = 0;

    n = nv2a_d3d_menu_rects(r, RECTS, present);
    if (present[2] <= 0 || present[3] <= 0)
        return 0;
    gx = (cx - present[0]) / present[2] * 640.0f;
    gy = (cy - present[1]) / present[3] * 480.0f;
    {   /* MM3_MENU_DUMP: the quads of the frame, every 5 s, for a new layout */
        static unsigned long long dumped;
        unsigned long long now = GetTickCount64();
        if (now - dumped > 5000 && getenv("MM3_MENU_DUMP")) {
            dumped = now;
            fprintf(stderr, "[MENU] pointer %.0f,%.0f, %d quads\n", gx, gy, n);
            for (i = 0; i < n; i++)
                fprintf(stderr, "[MENU] %4.0f %4.0f %4.0f %4.0f tex %08X argb %08X\n",
                        r[i].x0, r[i].y0, r[i].x1, r[i].y1, r[i].tex, r[i].argb);
            fflush(stderr);
        }
    }
    for (i = 0; i < n; i++) {
        float w = r[i].x1 - r[i].x0, h = r[i].y1 - r[i].y0;
        if (w < 24 || h < 12 || h > 64 || gx < r[i].x0 || gx > r[i].x1 || gy < r[i].y0 || gy > r[i].y1)
            continue;
        if (!hit || w * h < (hit->x1 - hit->x0) * (hit->y1 - hit->y0)) hit = &r[i];
    }
    if (!hit)
        return 0;
    i = strip_pointer(r, n, hit, offset);
    if (getenv("MM3_MENU_DUMP")) {
        static float lx, ly;
        if (fabsf(lx - gx) + fabsf(ly - gy) > 2) {
            lx = gx; ly = gy;
            fprintf(stderr, "[MENU] at %.0f,%.0f hit %.0f %.0f %.0f %.0f strip %d offset %d\n", gx, gy,
                    hit->x0, hit->y0, hit->x1, hit->y1, i, i ? *offset : 0);
            fflush(stderr);
        }
    }
    if (i)
        return i;
    /* The column's rows, by centre. */
    hh = (hit->y1 - hit->y0) * 0.5f;
    left = hit->x0; right = hit->x1;
    for (i = 0; i < n && rows < ROWS; i++) {
        float y = (r[i].y0 + r[i].y1) * 0.5f;
        int aligned = fabsf(r[i].x0 - hit->x0) <= 8 || fabsf(r[i].x1 - hit->x1) <= 8 ||
                      fabsf(r[i].x0 + r[i].x1 - hit->x0 - hit->x1) <= 16;
        if (!aligned || fabsf(r[i].y1 - r[i].y0 - hh * 2) > 4)
            continue;
        if (r[i].x0 < left) left = r[i].x0;
        if (r[i].x1 > right) right = r[i].x1;
        for (j = 0; j < rows && fabsf(ys[j] - y) > hh; j++) {}
        if (j == rows) ys[rows++] = y;
    }
    for (i = 1; i < rows; i++)                    /* top to bottom */
        for (j = i; j > 0 && ys[j] < ys[j - 1]; j--) { float t = ys[j]; ys[j] = ys[j - 1]; ys[j - 1] = t; }
    /* Only the evenly spaced run the pointer is in: something else that
     * happens to share the column's edge further away is not an item. */
    for (i = 0; i < rows && fabsf(ys[i] - (hit->y0 + hit->y1) * 0.5f) > hh; i++) {}
    if (i == rows)
        return 0;
    for (j = i; j > 0 && ys[j] - ys[j - 1] <= hh * 4; j--) {}
    for (k = i; k + 1 < rows && ys[k + 1] - ys[k] <= hh * 4; k++) {}
    rows = k - j + 1;
    hovered = i - j;
    memmove(ys, ys + j, (size_t)rows * sizeof *ys);
    if (rows < 2)
        return 0;
    /* Each row's textures, with the most opaque quad of each. */
    for (i = 0; i < rows; i++) nt[i] = 0;
    for (i = 0; i < n; i++) {
        float y = (r[i].y0 + r[i].y1) * 0.5f;
        if (r[i].x1 < left || r[i].x0 > right || r[i].y1 - r[i].y0 > hh * 4)
            continue;
        for (j = 0; j < rows && fabsf(ys[j] - y) > hh; j++) {}
        if (j == rows) continue;
        for (k = 0; k < nt[j] && texs[j][k] != r[i].tex; k++) {}
        if (k == nt[j]) {
            if (k == ROW_TEXS) continue;
            texs[j][k] = r[i].tex; alpha[j][k] = 0; nt[j]++;
        }
        if (r[i].argb >> 24 > alpha[j][k]) alpha[j][k] = r[i].argb >> 24;
    }
    /* A texture every row draws, much more opaque in one of them. */
    for (k = 0; k < nt[0]; k++) {
        uint32_t t = texs[0][k], best = 0, second = 0;
        int at = -1;
        for (i = 0; i < rows; i++) {
            int m;
            for (m = 0; m < nt[i] && texs[i][m] != t; m++) {}
            if (m == nt[i]) { at = -2; break; }
            if (alpha[i][m] > best) { second = best; best = alpha[i][m]; at = i; }
            else if (alpha[i][m] > second) second = alpha[i][m];
        }
        if (at < 0 || best < second + 96)
            continue;
        if (votes && at != focused) disagree = 1;
        focused = at;
        votes++;
    }
    if (!votes || disagree)
        return -1;
    *offset = hovered - focused;
    return 1;
}
