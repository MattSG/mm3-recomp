#include "polyline.h"
#include <string.h>

void polyline_simplify_2d(const float *points, unsigned count, float tolerance,
                          unsigned work_limit, unsigned char *keep, PolylineRange *ranges)
{
    unsigned pending = 1, inspected = 0;
    if (!count) return;
    memset(keep, 0, count);
    keep[0] = keep[count - 1] = 1;
    ranges[0].first = 0; ranges[0].last = count - 1;
    while (pending) {
        unsigned first = ranges[--pending].first;
        unsigned last = ranges[pending].last, i, furthest = first;
        double ax = points[2 * first], ay = points[2 * first + 1];
        double dx = (double)points[2 * last] - ax, dy = (double)points[2 * last + 1] - ay;
        double length2 = dx*dx + dy*dy, best = (double)tolerance * tolerance;
        for (i = first + 1; i < last; ++i) {
            double x = (double)points[2 * i] - ax;
            double y = (double)points[2 * i + 1] - ay;
            double t = length2 ? (x*dx + y*dy) / length2 : 0, distance;
            /* RDP can be quadratic on a pathological zigzag. Bound the
             * extra CPU work, then safely draw all original samples. */
            if (++inspected > work_limit) {
                memset(keep, 1, count); return;
            }
            t = t < 0 ? 0 : (t > 1 ? 1 : t);
            x -= t*dx; y -= t*dy; distance = x*x + y*y;
            if (distance > best) { best = distance; furthest = i; }
        }
        if (furthest == first) continue;
        keep[furthest] = 1;
        ranges[pending].first = first;
        ranges[pending++].last = furthest;
        ranges[pending].first = furthest;
        ranges[pending++].last = last;
    }
}
