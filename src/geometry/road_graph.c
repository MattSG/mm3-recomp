#include "road_graph.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

float graph_distance(GraphPoint a, GraphPoint b)
{
    float x = a.x - b.x, y = a.y - b.y, z = a.z - b.z;
    return sqrtf(x*x + y*y + z*z);
}

int graph_point_valid(GraphPoint p)
{
    return isfinite(p.x) && isfinite(p.y) && isfinite(p.z) &&
        fabsf(p.x) < 100000.0f && fabsf(p.y) < 100000.0f && fabsf(p.z) < 100000.0f;
}

void road_graph_free(RoadGraph *g)
{
    if (!g) return;
    free(g->nodes); free(g->edges); free(g->distance);
    free(g->previous); free(g->heap); free(g->heap_position);
    free(g);
}

RoadGraph *road_graph_new(unsigned nodes, unsigned max_edges)
{
    RoadGraph *g;
    unsigned i;
    if (!nodes || !max_edges) return NULL;
    g = (RoadGraph *)calloc(1, sizeof *g);
    if (!g) return NULL;
    g->nodes = (GraphNode *)calloc(nodes, sizeof(GraphNode));
    g->edges = (GraphEdge *)malloc(max_edges * sizeof(GraphEdge));
    g->distance = (float *)malloc(nodes * sizeof(float));
    g->previous = (uint32_t *)malloc(nodes * sizeof(uint32_t));
    g->heap = (uint32_t *)malloc(nodes * sizeof(uint32_t));
    g->heap_position = (uint32_t *)malloc(nodes * sizeof(uint32_t));
    if (!g->nodes || !g->edges || !g->distance || !g->previous || !g->heap || !g->heap_position) {
        road_graph_free(g);
        return NULL;
    }
    g->node_count = nodes;
    g->max_edges = max_edges;
    for (i = 0; i < nodes; ++i) g->nodes[i].first = ROAD_GRAPH_NO_NODE;
    return g;
}

int road_graph_connect(RoadGraph *g, unsigned a, unsigned b)
{
    float length;
    unsigned index;
    if (a >= g->node_count || b >= g->node_count || a == b) return 0;
    length = graph_distance(g->nodes[a].position, g->nodes[b].position);
    if (!isfinite(length)) return 0;
    /* Zero-length joins are real when two sections share an endpoint. */
    if (length < .001f) length = .001f;
    if (g->edge_count + 2 > g->max_edges) return 0;
    index = g->edge_count;
    g->edges[index].to = b; g->edges[index].length = length;
    g->edges[index].next = g->nodes[a].first;
    g->nodes[a].first = index;
    g->edges[index + 1].to = a; g->edges[index + 1].length = length;
    g->edges[index + 1].next = g->nodes[b].first;
    g->nodes[b].first = index + 1;
    g->edge_count += 2;
    return 1;
}

static void heap_update(RoadGraph *g, unsigned node)
{
    unsigned position = g->heap_position[node];
    if (position == ROAD_GRAPH_NO_NODE) position = g->heap_count++;
    while (position) {
        unsigned parent = (position - 1) / 2, other = g->heap[parent];
        if (g->distance[other] <= g->distance[node]) break;
        g->heap[position] = other; g->heap_position[other] = position;
        position = parent;
    }
    g->heap[position] = node; g->heap_position[node] = position;
}

static unsigned heap_pop(RoadGraph *g)
{
    unsigned result = g->heap[0], node = g->heap[--g->heap_count];
    unsigned position = 0;
    if (g->heap_count) {
        while (position * 2 + 1 < g->heap_count) {
            unsigned child = position * 2 + 1;
            if (child + 1 < g->heap_count && g->distance[g->heap[child + 1]] < g->distance[g->heap[child]])
                ++child;
            if (g->distance[node] <= g->distance[g->heap[child]]) break;
            g->heap[position] = g->heap[child];
            g->heap_position[g->heap[position]] = position;
            position = child;
        }
        g->heap[position] = node; g->heap_position[node] = position;
    }
    g->heap_position[result] = ROAD_GRAPH_NO_NODE;
    return result;
}

int road_graph_snap(const RoadGraph *g, GraphPoint position, float height_weight, float max_score,
                    GraphSnap *snap)
{
    unsigned a, edge;
    float best = FLT_MAX;
    if (!graph_point_valid(position)) return 0;
    for (a = 0; a < g->node_count; ++a)
        for (edge = g->nodes[a].first; edge != ROAD_GRAPH_NO_NODE; edge = g->edges[edge].next) {
            unsigned b = g->edges[edge].to;
            GraphPoint p = g->nodes[a].position, q = g->nodes[b].position, projected;
            float x, y, z, length2, t, score;
            if (b <= a) continue;
            x = q.x - p.x; y = q.y - p.y; z = q.z - p.z;
            length2 = x*x + y*y + z*z;
            if (length2 < .0001f) continue;
            t = ((position.x - p.x)*x + (position.y - p.y)*y + (position.z - p.z)*z) / length2;
            t = t < 0 ? 0 : (t > 1 ? 1 : t);
            projected.x = p.x + t*x; projected.y = p.y + t*y; projected.z = p.z + t*z;
            x = position.x - projected.x; y = position.y - projected.y; z = position.z - projected.z;
            score = x*x + height_weight*y*y + z*z;
            if (score < best) {
                best = score; snap->position = projected; snap->a = a; snap->b = b;
                snap->t = t; snap->length = sqrtf(length2);
            }
        }
    return best < max_score;
}

unsigned road_graph_path(RoadGraph *g, const GraphSnap *from, const GraphSnap *to,
                         GraphPoint *points, unsigned capacity)
{
    unsigned i, last = ROAD_GRAPH_NO_NODE, count = 0;
    float best = FLT_MAX;
    if (capacity < 2 || !g->edge_count) return 0;
    if (from->a == to->a && from->b == to->b) {
        points[0] = from->position; points[1] = to->position;
        return graph_distance(points[0], points[1]) > .01f ? 2 : 0;
    }
    g->heap_count = 0;
    for (i = 0; i < g->node_count; ++i) {
        g->distance[i] = FLT_MAX; g->previous[i] = ROAD_GRAPH_NO_NODE;
        g->heap_position[i] = ROAD_GRAPH_NO_NODE;
    }
    g->distance[from->a] = from->t * from->length;
    g->distance[from->b] = (1 - from->t) * from->length;
    heap_update(g, from->a); heap_update(g, from->b);
    while (g->heap_count) {
        unsigned node = heap_pop(g), edge;
        float distance = g->distance[node], extra;
        if (distance >= best) break;
        if (node == to->a || node == to->b) {
            extra = node == to->a ? to->t * to->length : (1 - to->t) * to->length;
            if (distance + extra < best) { best = distance + extra; last = node; }
        }
        for (edge = g->nodes[node].first; edge != ROAD_GRAPH_NO_NODE; edge = g->edges[edge].next) {
            unsigned next = g->edges[edge].to;
            float candidate = distance + g->edges[edge].length;
            if (candidate < g->distance[next]) {
                g->distance[next] = candidate; g->previous[next] = node;
                heap_update(g, next);
            }
        }
    }
    if (last == ROAD_GRAPH_NO_NODE) return 0;
    for (i = last; i != ROAD_GRAPH_NO_NODE; i = g->previous[i]) {
        if (count + 2 >= capacity) return 0;
        points[count++] = g->nodes[i].position;
    }
    for (i = 0; i < count / 2; ++i) {
        GraphPoint swap = points[i]; points[i] = points[count - 1 - i]; points[count - 1 - i] = swap;
    }
    memmove(points + 1, points, count * sizeof *points);
    points[0] = from->position; points[count + 1] = to->position;
    for (i = 1, last = 1; i < count + 2; ++i)
        if (graph_distance(points[last - 1], points[i]) > .01f) points[last++] = points[i];
    return last >= 2 ? last : 0;
}
