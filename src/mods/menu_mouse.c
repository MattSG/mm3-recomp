#include <math.h>
#include <stdint.h>
#include <string.h>

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

#include "nv2a_d3d11.h"

#define RECTS 1024
#define ROWS 32
#define ROW_TEXS 16

/* For a client-area fraction: 0 off any item, 1 on one (*offset rows below
 * the focused item, negative above), -1 on one whose focus is not known. */
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
    for (i = 0; i < n; i++) {
        float w = r[i].x1 - r[i].x0, h = r[i].y1 - r[i].y0;
        if (w < 24 || h < 12 || h > 64 || gx < r[i].x0 || gx > r[i].x1 || gy < r[i].y0 || gy > r[i].y1)
            continue;
        if (!hit || w * h < (hit->x1 - hit->x0) * (hit->y1 - hit->y0)) hit = &r[i];
    }
    if (!hit)
        return 0;
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
