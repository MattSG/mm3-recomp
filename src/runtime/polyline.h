/* Simplification of a 2D polyline by Ramer-Douglas-Peucker: the samples that
 * keep the shape within a tolerance of the original line.
 *
 * points is interleaved x, y pairs. keep receives one flag per sample, and
 * ranges is scratch that must hold count entries. Once the work spent on
 * distance tests passes work_limit, every sample is kept, so a pathological
 * zigzag costs bounded CPU and still draws the original path. */
#ifndef POLYLINE_H
#define POLYLINE_H

typedef struct { unsigned first, last; } PolylineRange;

void polyline_simplify_2d(const float *points, unsigned count, float tolerance,
                          unsigned work_limit, unsigned char *keep, PolylineRange *ranges);

#endif
