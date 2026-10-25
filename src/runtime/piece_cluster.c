#include "piece_cluster.h"
#include <stdlib.h>

static uint32_t *root_buf, *piece_buf, cap;
static float (*box_buf)[4];

#define FIND(x) do { while (root[x] != x) x = root[x] = root[root[x]]; } while (0)

int recomp_piece_clusters(uint32_t nv, const float (*pos)[4], const uint32_t *idx,
                          uint32_t sub, uint32_t ni, uint32_t per, float span, float gap,
                          const void *ctx, int (*skip)(const void *ctx, uint32_t vertex),
                          uint32_t *cluster, float (*box)[4])
{
    uint32_t *root = root_buf, *piece = piece_buf;
    float (*scratch)[4] = box_buf;
    uint32_t i, k, np = 0;

    if (nv > cap) {
        uint32_t *r = realloc(root_buf, (size_t)nv * sizeof *r);
        uint32_t *pc = r ? realloc(piece_buf, (size_t)nv * sizeof *pc) : NULL;
        float (*b)[4] = pc ? realloc(box_buf, (size_t)nv * sizeof *b) : NULL;
        if (r) root_buf = r;
        if (pc) piece_buf = pc;
        if (!b) return 0;
        box_buf = b;
        cap = nv;
        root = root_buf;
        piece = piece_buf;
        scratch = box_buf;
    }

    /* Pieces: vertices joined by primitives. */
    for (i = 0; i < nv; i++) root[i] = i;
    for (i = 0; i + per <= ni; i += per) {
        if (skip && skip(ctx, idx[i])) continue;
        for (k = 1; k < per; k++) {
            uint32_t a = idx[i] - sub, b = idx[i + k] - sub;
            if (a >= nv || b >= nv) return 0;
            FIND(a); FIND(b);
            if (a != b) root[b] = a;
        }
    }
    for (i = 0; i < nv; i++) {
        uint32_t r = i;
        FIND(r);
        if (r == i) {
            piece[i] = np;
            scratch[np][0] = scratch[np][2] = 1e30f;
            scratch[np][1] = scratch[np][3] = -1e30f;
            np++;
        }
    }
    for (i = 0; i < nv; i++) {
        uint32_t r = i, n;
        FIND(r);
        n = piece[r];
        piece[i] = n;
        if (skip && skip(ctx, sub + i)) continue;
        if (pos[i][0] < scratch[n][0]) scratch[n][0] = pos[i][0];
        if (pos[i][0] > scratch[n][1]) scratch[n][1] = pos[i][0];
        if (pos[i][1] < scratch[n][2]) scratch[n][2] = pos[i][1];
        if (pos[i][1] > scratch[n][3]) scratch[n][3] = pos[i][1];
    }

    /* Clusters: pieces on one line within gap. ponytail: O(pieces^2), fine for
     * HUD batches of a few hundred glyphs. */
    for (i = 0; i < np; i++) root[i] = i;
    for (i = 0; i < np; i++)
        for (k = i + 1; k < np; k++) {
            uint32_t a = i, b = k;
            /* A full-span quad overlaps every piece and stays its own. */
            if ((scratch[i][0] <= 0.5f && scratch[i][1] >= span - 0.5f) ||
                (scratch[k][0] <= 0.5f && scratch[k][1] >= span - 0.5f))
                continue;
            if (scratch[i][2] > scratch[k][3] || scratch[k][2] > scratch[i][3] ||
                scratch[i][0] > scratch[k][1] + gap || scratch[k][0] > scratch[i][1] + gap)
                continue;
            FIND(a); FIND(b);
            if (a == b) continue;
            root[b] = a;
            if (scratch[b][0] < scratch[a][0]) scratch[a][0] = scratch[b][0];
            if (scratch[b][1] > scratch[a][1]) scratch[a][1] = scratch[b][1];
            if (scratch[b][2] < scratch[a][2]) scratch[a][2] = scratch[b][2];
            if (scratch[b][3] > scratch[a][3]) scratch[a][3] = scratch[b][3];
        }

    for (i = 0; i < nv; i++) {
        uint32_t c = piece[i];
        FIND(c);
        cluster[i] = c;
    }
    for (i = 0; i < np; i++) {
        box[i][0] = scratch[i][0]; box[i][1] = scratch[i][1];
        box[i][2] = scratch[i][2]; box[i][3] = scratch[i][3];
    }
    return 1;
}
