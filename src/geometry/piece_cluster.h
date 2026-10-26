/* Groups a batch of 2D primitives into pieces, then clusters the pieces.
 *
 * Vertices that share a primitive form a piece (union-find over the index list),
 * and each piece's bounding box is taken over its vertices. Pieces that lie on
 * one line, less than gap apart, join into a cluster. A piece spanning the whole
 * width (span) is a full-screen quad and stays its own cluster.
 *
 * Primitives whose first vertex the skip callback names are left out of the
 * grouping, and so are the skipped vertices' positions. Index values are
 * absolute; vertex i of the position array is absolute vertex sub + i. */
#ifndef PIECE_CLUSTER_H
#define PIECE_CLUSTER_H

#include <stdint.h>

/* cluster[i] receives the cluster index of vertex i, and box[c] the bounds of
 * cluster c (x0, x1, y0, y1). Returns 0 when an index is out of range or the
 * scratch storage cannot be allocated. */
int recomp_piece_clusters(uint32_t nv, const float (*pos)[4], const uint32_t *idx,
                          uint32_t sub, uint32_t ni, uint32_t per, float span, float gap,
                          const void *ctx, int (*skip)(const void *ctx, uint32_t vertex),
                          uint32_t *cluster, float (*box)[4]);

#endif
