/* Kochanek–Bartels Hermite evaluation for one spline segment.
 *
 * p[0..3] are the segment's control points (p[1] and p[2] bracket the segment).
 * u runs 0..1 across it. bias, tension and continuity are the Kochanek–Bartels
 * shape parameters; zero for all three gives the Catmull-Rom curve. */
#ifndef KB_SPLINE_H
#define KB_SPLINE_H

typedef struct { float x, y, z; } KbPoint;

KbPoint kb_segment_point(const KbPoint p[4], float u, float bias, float tension, float continuity);

#endif
