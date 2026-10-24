#include <math.h>
#include <float.h>
#include <stdlib.h>
#include <stdio.h>
#include "recomp_types.h"
#include "guest_state.h"
#include "kb_spline.h"
#include "road_graph.h"

/* 201BCA publishes the native race's complete ordered checkpoint list once
 * per player HUD update, before hiding passed/inactive checkpoint icons.
 * Copy positions and identities; never dereference a saved guest object. */
enum { RACE_PLAYERS = 2, RACE_CHECKPOINTS = 128 };
typedef GraphPoint RoadPoint;
typedef struct { uint32_t object; RoadPoint position; } RaceCheckpoint;
static struct {
    RaceCheckpoint checkpoints[RACE_CHECKPOINTS];
    RoadPoint origin;
    uint32_t current_object;
    unsigned count, current, valid, closed;
} s_race[RACE_PLAYERS];

enum { ROAD_NODES = 65536, ROAD_EDGES = 262144, COURSE_POINTS = 16384,
       ACTIVE_POINTS = 4096, NO_NODE = UINT32_MAX };
static RoadGraph *s_roads;
static unsigned s_road_generation;
static struct {
    RaceCheckpoint checkpoints[RACE_CHECKPOINTS];
    RoadPoint course[COURSE_POINTS], active[ACTIVE_POINTS], active_origin;
    unsigned starts[RACE_CHECKPOINTS], lengths[RACE_CHECKPOINTS];
    unsigned count, closed, generation, course_count, active_count;
    uint32_t active_target;
} s_paths[RACE_PLAYERS];

#define road_distance graph_distance
#define road_point_valid graph_point_valid

static void road_graph_clear(void)
{
    road_graph_free(s_roads);
    s_roads = NULL;
    s_road_generation++;
    memset(s_paths, 0, sizeof s_paths);
}

static int road_graph_allocate(unsigned nodes)
{
    if (!nodes || nodes > ROAD_NODES) return 0;
    s_roads = road_graph_new(nodes, ROAD_EDGES);
    return s_roads != NULL;
}

static int road_graph_edge(unsigned a, unsigned b)
{
    return road_graph_connect(s_roads, a, b);
}

/* The fork snaps both ends to the graph with the title's height weight and
 * snap distance, then runs the shortest path. */
static unsigned road_path(RoadPoint start, RoadPoint finish, RoadPoint *points, unsigned capacity)
{
    GraphSnap from, to;
    if (!s_roads || capacity < 2 ||
        !road_graph_snap(s_roads, start, 9.0f, 200.0f * 200.0f, &from) ||
        !road_graph_snap(s_roads, finish, 9.0f, 200.0f * 200.0f, &to))
        return 0;
    return road_graph_path(s_roads, &from, &to, points, capacity);
}

typedef struct {
    uint32_t object, controls;
    unsigned control_count, steps, first, last;
    float bias, tension, continuity;
} RoadCurve;

static RoadPoint road_control(uint32_t controls, unsigned index)
{
    RoadPoint point;
    uint32_t address = controls + index * 12;
    point.x = MEMF(address); point.y = MEMF(address + 4); point.z = MEMF(address + 8);
    return point;
}

static RoadPoint road_curve_point(const RoadCurve *curve, float t)
{
    /* Native sub_000C191C: Kochanek–Bartels Hermite, with two ghost controls.
     * sub_00154FC8 uses this exact spline at zero lateral/vertical offset for
     * the road centre, including hills and overpasses. The segment's Hermite
     * evaluation is the fork's kb_segment_point. */
    float s = (curve->control_count - 3) * t, u;
    unsigned segment = (unsigned)s;
    RoadPoint result;
    KbPoint p[4], q;
    unsigned i;
    if (segment >= curve->control_count - 3) { segment = curve->control_count - 4; u = 1; }
    else u = s - segment;
    for (i = 0; i < 4; ++i) {
        RoadPoint c = road_control(curve->controls, segment + i);
        p[i].x = c.x; p[i].y = c.y; p[i].z = c.z;
    }
    q = kb_segment_point(p, u, curve->bias, curve->tension, curve->continuity);
    result.x = q.x; result.y = q.y; result.z = q.z;
    return result;
}

static unsigned road_object_index(uint32_t object, uint32_t base, unsigned count, unsigned stride)
{
    if (!base || object < base || (object - base) % stride || (object - base) / stride >= count) return NO_NODE;
    return (object - base) / stride;
}

void mm3_race_guidance_roads(uint32_t city, uint32_t loaded)
{
    uint32_t arrays[4], links;
    unsigned counts[4], road_count, junction_count, total, i, j;
    RoadCurve *curves;
    road_graph_clear();
    if (!loaded || !city) return;
    /* sub_0014BAE9 has finished resolving the city.ai arrays. Snapshot world
     * geometry and the native paired object links now, before teardown can
     * free them. Never infer connections from nearby streets or crossings. */
    for (i = 0; i < 4; ++i) {
        arrays[i] = MEM32(city + 0x40 + 4*i); counts[i] = MEM32(city + 0xA0 + 4*i);
        if (counts[i] > 4096 || (counts[i] && !arrays[i])) return;
    }
    /* +4C holds polygon areas such as plazas, with links into road interiors.
     * A road route uses +48 street junctions, rather than area-centre shortcuts. */
    /* +44 special splines have no car lane paths and form separate rail-like
     * loops. Cars and checkpoints snap only to the main +40 street network. */
    road_count = counts[0]; junction_count = counts[2];
    links = MEM32(city + 0x58);
    if (!road_count || !links || MEM32(city + 0xB0) > 32768) return;
    curves = (RoadCurve *)calloc(road_count, sizeof *curves);
    if (!curves) return;
    total = junction_count;
    for (i = 0; i < road_count; ++i) {
        uint32_t road = arrays[0] + i * 0xDC, spline = MEM32(road + 0x78), end;
        float length = MEMF(road + 0x74);
        RoadCurve *curve = curves + i;
        if (!spline || !isfinite(length) || length <= 0 || length > 50000) goto invalid;
        curve->object = road; curve->controls = MEM32(spline + 4); end = MEM32(spline + 8);
        if (!curve->controls || end < curve->controls || (end - curve->controls) % 12) goto invalid;
        curve->control_count = (end - curve->controls) / 12;
        if (curve->control_count < 4 || curve->control_count > 4096) goto invalid;
        curve->bias = MEMF(spline + 0x10); curve->tension = MEMF(spline + 0x14);
        curve->continuity = MEMF(spline + 0x18);
        if (!isfinite(curve->bias) || !isfinite(curve->tension) || !isfinite(curve->continuity)) goto invalid;
        curve->steps = (unsigned)ceilf(length / 6.0f);
        curve->first = total; total += curve->steps + 1; curve->last = total - 1;
        if (total > ROAD_NODES) goto invalid;
    }
    if (!road_graph_allocate(total)) goto invalid;
    for (i = 0; i < junction_count; ++i) {
        uint32_t object = arrays[2] + i * 0xAC;
        s_roads->nodes[i].position = road_control(object + 0x34, 0);
        if (!road_point_valid(s_roads->nodes[i].position)) goto invalid;
    }
    for (i = 0; i < road_count; ++i) {
        RoadCurve *curve = curves + i;
        for (j = 0; j <= curve->steps; ++j) {
            s_roads->nodes[curve->first + j].position = road_curve_point(curve, (float)j / curve->steps);
            if (!road_point_valid(s_roads->nodes[curve->first + j].position)) goto invalid;
            if (j && !road_graph_edge(curve->first + j - 1, curve->first + j)) goto invalid;
        }
    }
    for (i = 0; i < MEM32(city + 0xB0); ++i) {
        unsigned ids[2] = {NO_NODE, NO_NODE}, road_ids[2] = {NO_NODE, NO_NODE}, side;
        for (side = 0; side < 2; ++side) {
            uint32_t object = MEM32(links + i*8 + side*4);
            for (j = 0; j < 3; ++j) {
                if (j == 1) continue;
                unsigned index = road_object_index(object, arrays[j], counts[j], j < 2 ? 0xDC : 0xAC);
                if (index == NO_NODE) continue;
                if (j == 0) road_ids[side] = index;
                else ids[side] = index;
                break;
            }
        }
        /* sub_0014E660 resolves the two directional object IDs. Slot zero
         * attaches to the road's spline start; slot one attaches to its end.
         * This also handles loops and roads with only a single native link. */
        for (side = 0; side < 2; ++side) if (road_ids[side] != NO_NODE)
            ids[side] = side ? curves[road_ids[side]].last : curves[road_ids[side]].first;
        if (ids[0] != NO_NODE && ids[1] != NO_NODE && ids[0] != ids[1] && !road_graph_edge(ids[0], ids[1])) goto invalid;
    }
    fprintf(stderr, "[RACE_ROUTE] city=%08X roads=%u junctions=%u nodes=%u links=%u edges=%u\n",
            city, road_count, junction_count, s_roads->node_count, MEM32(city + 0xB0), s_roads->edge_count / 2);
    free(curves);
    return;
invalid:
    fprintf(stderr, "[RACE_ROUTE] rejected invalid city road geometry at %08X\n", city);
    free(curves); road_graph_clear();
}

void mm3_race_guidance_frame_begin(void)
{
    /* 120091 updates every active player before any HUD/map drawing. Clear
     * the whole previous epoch, including players removed from split screen. */
    memset(s_race, 0, sizeof s_race);
}

void mm3_race_guidance_begin(uint32_t player)
{
    if (player < RACE_PLAYERS) {
        s_race[player].count = s_race[player].valid = 0;
        s_race[player].current_object = 0;
        /* Native event mode 2 loads /blitz/, while mode 3 loads the
         * /checkpoint/ circuit. Blitz finishes without wrapping to start. */
        s_race[player].closed = MEM32(0x407C1C) == 3;
    }
}

void mm3_race_guidance_checkpoint(uint32_t player, uint32_t object,
                                  uint32_t position, uint32_t origin,
                                  uint32_t current)
{
    unsigned count;
    RoadPoint point, player_origin;
    if (player >= RACE_PLAYERS || !object || !position || !origin) return;
    count = s_race[player].count;
    if (count >= RACE_CHECKPOINTS) return;
    /* 20BF9C exposes the checkpoint's world position at subobject +0x48.
     * Use the same player origin as the native marker projection, including
     * when the native +0x70 visibility flag suppresses the icon. */
    point.x = MEMF(position); point.y = MEMF(position + 4); point.z = MEMF(position + 8);
    player_origin.x = MEMF(origin); player_origin.y = MEMF(origin + 4); player_origin.z = MEMF(origin + 8);
    if (!road_point_valid(point) || !road_point_valid(player_origin)) return;
    s_race[player].checkpoints[count].object = object;
    s_race[player].checkpoints[count].position = point;
    s_race[player].origin = player_origin;
    s_race[player].current_object = current;
    if (object == current) s_race[player].current = count;
    s_race[player].count = count + 1;
}

void mm3_race_guidance_end(uint32_t player)
{
    unsigned i;
    if (player >= RACE_PLAYERS) return;
    /* A race may temporarily have no current target, including its finish
     * state. The course still exists and must continue drawing. */
    s_race[player].current = s_race[player].count;
    s_race[player].valid = s_race[player].count >= 2;
    for (i = 0; i < s_race[player].count; ++i)
        if (s_race[player].checkpoints[i].object == s_race[player].current_object) {
            s_race[player].current = i;
            return;
        }
}

static void race_paths_update(unsigned player)
{
    unsigned i, changed;
    changed = s_paths[player].generation != s_road_generation ||
        s_paths[player].count != s_race[player].count || s_paths[player].closed != s_race[player].closed;
    for (i = 0; !changed && i < s_race[player].count; ++i)
        changed = s_paths[player].checkpoints[i].object != s_race[player].checkpoints[i].object ||
            road_distance(s_paths[player].checkpoints[i].position, s_race[player].checkpoints[i].position) > .01f;
    if (changed) {
        unsigned routed = 0;
        s_paths[player].generation = s_road_generation;
        s_paths[player].count = s_race[player].count;
        s_paths[player].closed = s_race[player].closed;
        memcpy(s_paths[player].checkpoints, s_race[player].checkpoints,
               s_race[player].count * sizeof(RaceCheckpoint));
        memset(s_paths[player].lengths, 0, sizeof s_paths[player].lengths);
        s_paths[player].course_count = 0;
        s_paths[player].active_target = 0;
        for (i = 0; i + 1 < s_race[player].count + s_race[player].closed; ++i) {
            unsigned next = (i + 1) % s_race[player].count, start = s_paths[player].course_count;
            unsigned count = road_path(s_race[player].checkpoints[i].position,
                s_race[player].checkpoints[next].position, s_paths[player].course + start, COURSE_POINTS - start);
            s_paths[player].starts[i] = start; s_paths[player].lengths[i] = count;
            s_paths[player].course_count += count;
            if (count) ++routed;
        }
        fprintf(stderr, "[RACE_ROUTE] player=%u checkpoints=%u routed_legs=%u/%u course_points=%u\n",
                player, s_race[player].count, routed,
                s_race[player].count - 1 + s_race[player].closed, s_paths[player].course_count);
    }
    if (s_race[player].current >= s_race[player].count) {
        s_paths[player].active_count = 0; s_paths[player].active_target = 0;
        return;
    }
    /* Course legs are cached in world space. Recalculate only the car's path
     * after moving 4m or receiving a new checkpoint, including missed turns. */
    if (s_paths[player].active_target != s_race[player].current_object ||
        road_distance(s_paths[player].active_origin, s_race[player].origin) >= 4.0f) {
        s_paths[player].active_origin = s_race[player].origin;
        s_paths[player].active_target = s_race[player].current_object;
        s_paths[player].active_count = road_path(s_race[player].origin,
            s_race[player].checkpoints[s_race[player].current].position,
            s_paths[player].active, ACTIVE_POINTS);
    }
}

extern void sub_001C1585(void);

/* Untextured native UI quads discard source UVs while packing their GPU
 * vertices. Carry this explicit route identity into unused packed UV0, then
 * the renderer consumes it and uploads the original zero UVs. This survives
 * queued command processing and batches shared with ordinary HUD quads. */
enum { ROUTE_TAG_U = 23117, ROUTE_TAG_V = -17493 };

uint32_t mm3_race_route_quad_tagged(uint32_t texture, uint32_t quad)
{
    unsigned i;
    if (texture || !quad) return 0;
    for (i = 0; i < 4; ++i) {
        uint32_t vertex = quad + 0xC + i * 0x24;
        if (MEMF(vertex + 0xC) != (float)ROUTE_TAG_U ||
            MEMF(vertex + 0x10) != (float)ROUTE_TAG_V) return 0;
    }
    return 1;
}

void mm3_race_route_tag_packed(uint32_t vertex)
{
    MEM16(vertex + 8) = (uint16_t)(int16_t)ROUTE_TAG_U;
    MEM16(vertex + 0xA) = (uint16_t)(int16_t)ROUTE_TAG_V;
}

static void route_segment(uint32_t renderer, uint32_t clip,
                           float ax, float ay, float bx, float by, float alpha)
{
    float dx = bx - ax, dy = by - ay, length = hypotf(dx, dy), px, py;
    uint32_t quad;
    xbox_guest_state saved;
    unsigned i;
    const float thickness = 1.25f;
    float xy[4][2];
    if (!isfinite(length) || length < .01f) return;
    px = -dy * thickness / length; py = dx * thickness / length;
    xy[0][0] = ax + px; xy[0][1] = ay + py;
    xy[1][0] = bx + px; xy[1][1] = by + py;
    xy[2][0] = bx - px; xy[2][1] = by - py;
    xy[3][0] = ax - px; xy[3][1] = ay - py;
    xbox_guest_state_save(&saved);
    /* Same 0xA0 native quad structure as 2DC95; scratch belongs to this
     * synchronous submission and is below the live guest stack frame. */
    g_esp -= 0xA0; quad = g_esp;
    memset((void *)XBOX_PTR(quad), 0, 0xA0);
    MEM32(quad + 4) = 7; MEM32(quad + 8) = 8;
    for (i = 0; i < 4; ++i) {
        uint32_t vertex = quad + 0xC + i * 0x24;
        MEMF(vertex) = xy[i][0]; MEMF(vertex + 4) = xy[i][1];
        MEMF(vertex + 0xC) = (float)ROUTE_TAG_U;
        MEMF(vertex + 0x10) = (float)ROUTE_TAG_V;
        MEMF(vertex + 0x14) = 1.0f;
        MEMF(vertex + 0x18) = .72f;
        MEMF(vertex + 0x1C) = .15f;
        MEMF(vertex + 0x20) = alpha;
    }
    PUSH32(g_esp, quad); PUSH32(g_esp, 0);
    g_eax = clip; g_edx = renderer;
    PUSH32(g_esp, 0x0011CB45u);
    RECOMP_ABI_CALL(0x001C1585u, sub_001C1585);
    xbox_guest_state_restore(&saved, saved.eax);
}

typedef struct { float x, y; } RoutePoint;
static struct {
    RoutePoint points[COURSE_POINTS];
    unsigned char keep[COURSE_POINTS];
    struct { unsigned first, last; } ranges[COURSE_POINTS];
} s_route_draw;

static void route_simplify(unsigned count)
{
    unsigned pending = 1, inspected = 0;
    memset(s_route_draw.keep, 0, count);
    s_route_draw.keep[0] = s_route_draw.keep[count - 1] = 1;
    s_route_draw.ranges[0].first = 0; s_route_draw.ranges[0].last = count - 1;
    while (pending) {
        unsigned first = s_route_draw.ranges[--pending].first;
        unsigned last = s_route_draw.ranges[pending].last, i, furthest = first;
        RoutePoint a = s_route_draw.points[first], b = s_route_draw.points[last];
        double dx = (double)b.x - a.x, dy = (double)b.y - a.y;
        double length2 = dx*dx + dy*dy, best = .25 * .25;
        for (i = first + 1; i < last; ++i) {
            double x = (double)s_route_draw.points[i].x - a.x;
            double y = (double)s_route_draw.points[i].y - a.y;
            double t = length2 ? (x*dx + y*dy) / length2 : 0, distance;
            /* RDP can be quadratic on a pathological zigzag. Bound the
             * extra CPU work, then safely draw all original samples. */
            if (++inspected > count * 64) {
                memset(s_route_draw.keep, 1, count); return;
            }
            t = t < 0 ? 0 : (t > 1 ? 1 : t);
            x -= t*dx; y -= t*dy; distance = x*x + y*y;
            if (distance > best) { best = distance; furthest = i; }
        }
        if (furthest == first) continue;
        s_route_draw.keep[furthest] = 1;
        s_route_draw.ranges[pending].first = first;
        s_route_draw.ranges[pending++].last = furthest;
        s_route_draw.ranges[pending].first = furthest;
        s_route_draw.ranges[pending++].last = last;
    }
}

static void route_polyline(uint32_t renderer, uint32_t clip, const RoadPoint *points,
                           unsigned count, RoadPoint origin, float factor,
                           float cosine, float sine, float radius, float cx, float cy, float alpha)
{
    unsigned i, previous = 0;
    if (count < 2 || count > COURSE_POINTS) return;
    for (i = 0; i < count; ++i) {
        float x = -(points[i].x - origin.x) * factor, y = (points[i].z - origin.z) * factor;
        s_route_draw.points[i].x = x*cosine + y*sine;
        s_route_draw.points[i].y = y*cosine - x*sine;
        if (!isfinite(s_route_draw.points[i].x) || !isfinite(s_route_draw.points[i].y)) return;
    }
    /* The graph retains its 6m samples for accurate routing. Submit only
     * screen-space bends: every removed point is within 0.25 pixel of the
     * retained segment, including backtracking and coincident endpoints.
     * Iterative RDP uses bounded host scratch instead of recursive frames. */
    route_simplify(count);
    for (i = 1; i < count; ++i) {
        float ax, ay, bx, by;
        double dx, dy, a, b, c, discriminant, lo, hi;
        if (!s_route_draw.keep[i]) continue;
        ax = s_route_draw.points[previous].x; ay = s_route_draw.points[previous].y;
        bx = s_route_draw.points[i].x; by = s_route_draw.points[i].y;
        previous = i;
        dx = (double)bx - ax; dy = (double)by - ay;
        a = dx*dx + dy*dy; b = 2*(ax*dx + ay*dy);
        c = (double)ax*ax + (double)ay*ay - (double)radius*radius;
        /* Clip each retained segment to the native circle, including crossings
         * whose endpoints both lie outside the map. */
        if (a < .0001f) continue;
        discriminant = b*b - 4*a*c;
        if (discriminant < 0) continue;
        lo = (-b - sqrt(discriminant)) / (2*a);
        hi = (-b + sqrt(discriminant)) / (2*a);
        lo = lo < 0 ? 0 : lo; hi = hi > 1 ? 1 : hi;
        if (lo >= hi) continue;
        route_segment(renderer, clip, (float)((double)cx + ax + lo*dx), (float)((double)cy + ay + lo*dy),
                      (float)((double)cx + ax + hi*dx), (float)((double)cy + ay + hi*dy), alpha);
    }
}

void mm3_race_map_route(uint32_t node, uint32_t table,
                         uint32_t renderer, uint32_t clip, uint32_t layout)
{
    unsigned player, i;
    float scale, width, angle, cosine, sine, radius, cx, cy, factor;
    if (!node || !renderer || !layout || table < 0x407F78 ||
        table >= 0x407F78 + 0x10 * RACE_PLAYERS || ((table - 0x407F78) & 15)) return;
    player = (table - 0x407F78) / 0x10;
    if (!s_race[player].valid || s_race[player].count < 2 || !s_roads || !s_roads->edge_count) return;
    scale = MEMF(node + 0x50); width = MEMF(node + 0x58) - MEMF(node + 0x54);
    angle = MEMF(node + 0x4C); cx = MEMF(node + 0x94); cy = MEMF(node + 0x98);
    radius = MEMF(layout + 8) * .5f - MEMF(node + 0xB4) * 8.0f;
    if (!isfinite(scale) || scale <= 0 || !isfinite(width) || width <= 0 ||
        !isfinite(angle) || !isfinite(cx) || !isfinite(cy) || !isfinite(radius) || radius <= 0) return;
    factor = scale * 1024.0f / width; cosine = cosf(angle); sine = sinf(angle);
    if (!isfinite(factor)) return;
    race_paths_update(player);
    for (i = 0; i + 1 < s_race[player].count + s_race[player].closed; ++i) {
        unsigned next = (i + 1) % s_race[player].count;
        /* The brighter leg comes from the car's present road position. A
         * wrong turn therefore gets fresh guidance to the current checkpoint. */
        if (next == s_race[player].current && s_paths[player].active_count) continue;
        route_polyline(renderer, clip, s_paths[player].course + s_paths[player].starts[i],
                       s_paths[player].lengths[i], s_race[player].origin, factor,
                       cosine, sine, radius, cx, cy, .8f);
    }
    route_polyline(renderer, clip, s_paths[player].active, s_paths[player].active_count,
                   s_race[player].origin, factor, cosine, sine, radius, cx, cy, .98f);
}
