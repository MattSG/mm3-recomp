#include <math.h>
#include <stdio.h>
#include "kb_spline.h"
#include "polyline.h"
#include "road_graph.h"
#include "vec3_rotate.h"
#include "thick_line.h"
#include "fixed_step.h"

/* Not assert: the calls inside the checks must run in release builds too. */
#define CHECK(x) do { if (!(x)) { printf("%s:%d: failed: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)
#define NEAR(a, b) (fabsf((float)(a) - (float)(b)) < 1e-4f)

static int test_polyline(void)
{
    /* Collinear samples collapse to their endpoints. */
    float line[] = {0, 0, 1, 0, 2, 0, 3, 0, 4, 0};
    unsigned char keep[5];
    PolylineRange ranges[5];
    polyline_simplify_2d(line, 5, .25f, 5 * 64, keep, ranges);
    CHECK(keep[0] && !keep[1] && !keep[2] && !keep[3] && keep[4]);

    /* A peak well outside the tolerance survives. */
    {
        float peak[] = {0, 0, 2, 3, 4, 0};
        polyline_simplify_2d(peak, 3, .25f, 3 * 64, keep, ranges);
        CHECK(keep[0] && keep[1] && keep[2]);
    }

    /* A zero work limit keeps every sample. */
    polyline_simplify_2d(line, 5, .25f, 0, keep, ranges);
    CHECK(keep[0] && keep[1] && keep[2] && keep[3] && keep[4]);
    return 0;
}

static int test_road_graph(void)
{
    RoadGraph *g = road_graph_new(3, 8);
    GraphSnap from, to;
    GraphPoint path[8];
    unsigned count;
    CHECK(g != NULL);
    g->nodes[0].position = (GraphPoint){0, 0, 0};
    g->nodes[1].position = (GraphPoint){10, 0, 0};
    g->nodes[2].position = (GraphPoint){10, 0, 10};
    CHECK(road_graph_connect(g, 0, 1));
    CHECK(road_graph_connect(g, 1, 2));
    CHECK(!road_graph_connect(g, 0, 0));
    CHECK(NEAR(graph_distance(g->nodes[0].position, g->nodes[2].position), sqrtf(200.0f)));

    /* Snap a point just above the first edge, then route to the far end. */
    CHECK(road_graph_snap(g, (GraphPoint){5, 2, 0}, 1.0f, 100.0f, &from));
    CHECK(NEAR(from.position.x, 5) && NEAR(from.position.z, 0));
    CHECK(road_graph_snap(g, (GraphPoint){10, 0, 8}, 1.0f, 100.0f, &to));
    count = road_graph_path(g, &from, &to, path, 8);
    CHECK(count >= 2);
    CHECK(NEAR(path[0].x, 5) && NEAR(path[count - 1].z, 8));

    CHECK(!graph_point_valid((GraphPoint){1e9f, 0, 0}));
    road_graph_free(g);
    return 0;
}

static int test_vec3_rotate(void)
{
    /* Axis (dz, 0, -dx) = (1, 0, 0): a quarter turn takes +y to +z. */
    const float in[3] = {0, 1, 0};
    float out[3];
    vec3_rotate_about_horizontal(out, in, 0, 1, 0, 1);
    CHECK(NEAR(out[0], 0) && NEAR(out[1], 0) && NEAR(out[2], 1));
    return 0;
}

static int test_kb_spline(void)
{
    const KbPoint p[4] = {{0, 0, 0}, {1, 0, 0}, {2, 0, 0}, {3, 0, 0}};
    KbPoint a = kb_segment_point(p, 0, 0, 0, 0), b = kb_segment_point(p, 1, 0, 0, 0);
    CHECK(NEAR(a.x, 1) && NEAR(b.x, 2));
    return 0;
}

static int test_thick_line(void)
{
    /* A horizontal segment of width 2 becomes a band of half-width 1. */
    float xy[8];
    CHECK(thick_line_quad(xy, 0, 0, 10, 0, 2));
    CHECK(NEAR(xy[0], 0) && NEAR(xy[1], -1) && NEAR(xy[2], 10) && NEAR(xy[3], -1));
    CHECK(NEAR(xy[4], 10) && NEAR(xy[5], 1) && NEAR(xy[6], 0) && NEAR(xy[7], 1));
    CHECK(!thick_line_quad(xy, 3, 3, 3, 3, 2));
    return 0;
}

int main(void)
{
    FixedStepClock clock = {0};
    float alpha = 0;
    CHECK(fixed_step_take(&clock, .25f, .5f, 2, &alpha) == 0 && NEAR(alpha, .5f));
    CHECK(fixed_step_take(&clock, .25f, .5f, 2, &alpha) == 1 && NEAR(alpha, 0));
    CHECK(recomp_clock_capped_advance(10, 3, 5) == 13);
    CHECK(recomp_clock_capped_advance(10, 9, 5) == 15);
    if (test_polyline() || test_road_graph() || test_vec3_rotate() || test_kb_spline() ||
        test_thick_line())
        return 1;
    printf("mm3_geometry: all checks passed\n");
    return 0;
}
