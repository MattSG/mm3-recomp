/* A graph of 3D points joined by edges, with snapping and shortest paths.
 *
 * Nodes carry a position and a list of edges; an edge's length is the distance
 * between its ends (never less than 0.001, so shared endpoints still join).
 * A point snaps to the nearest edge, by its distance to the edge with height
 * counted at height_weight, and a path runs between two snapped points along
 * the graph (Dijkstra with an indexed heap). Nothing here knows a title's data. */
#ifndef ROAD_GRAPH_H
#define ROAD_GRAPH_H

#include <stdint.h>

typedef struct { float x, y, z; } GraphPoint;

typedef struct { GraphPoint position; uint32_t first; } GraphNode;
typedef struct { uint32_t to, next; float length; } GraphEdge;

typedef struct {
    GraphPoint position;   /* the projection on the edge */
    uint32_t a, b;         /* the edge's end nodes */
    float t, length;       /* the projection's place along the edge, and its length */
} GraphSnap;

typedef struct RoadGraph {
    GraphNode *nodes;
    GraphEdge *edges;
    float *distance;
    uint32_t *previous, *heap, *heap_position;
    unsigned node_count, edge_count, heap_count, max_edges;
} RoadGraph;

#define ROAD_GRAPH_NO_NODE UINT32_MAX

float graph_distance(GraphPoint a, GraphPoint b);
int graph_point_valid(GraphPoint p);

/* Allocates a graph of nodes nodes, with room for max_edges directed edges.
 * Returns NULL when the request is empty or allocation fails. */
RoadGraph *road_graph_new(unsigned nodes, unsigned max_edges);
void road_graph_free(RoadGraph *graph);

/* Joins a and b both ways. Returns 0 for out-of-range or equal nodes, a
 * non-finite length, or no room. */
int road_graph_connect(RoadGraph *graph, unsigned a, unsigned b);

/* Snaps p to the nearest edge. The score is dx^2 + height_weight*dy^2 + dz^2;
 * returns 0 when p is invalid or the best score is not below max_score. */
int road_graph_snap(const RoadGraph *graph, GraphPoint p, float height_weight, float max_score,
                    GraphSnap *snap);

/* The points of the shortest path from one snap to the other, including both
 * snapped ends with duplicated endpoints removed. Returns 0 when there is no
 * route or the path does not fit in capacity. */
unsigned road_graph_path(RoadGraph *graph, const GraphSnap *from, const GraphSnap *to,
                         GraphPoint *points, unsigned capacity);

#endif
