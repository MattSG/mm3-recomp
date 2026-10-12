#include <math.h>
#include "recomp_types.h"

/* The title owns these map nodes and calls all three helpers on its update/
 * draw thread. Keep only identities and scalars here; no saved guest pointer
 * is followed after a scene change. 11CA25 resets a node's layout on entry. */
enum { MAP_ZOOM_SLOTS = 16 };
static struct {
    uint32_t node;
    float target, span, baseline;
} s_map_zoom[MAP_ZOOM_SLOTS];
static unsigned s_map_zoom_replacement;

static unsigned map_zoom_slot(uint32_t node)
{
    unsigned i, empty = MAP_ZOOM_SLOTS;
    for (i = 0; i < MAP_ZOOM_SLOTS; ++i) {
        if (s_map_zoom[i].node == node) return i;
        if (!s_map_zoom[i].node && empty == MAP_ZOOM_SLOTS) empty = i;
    }
    if (empty == MAP_ZOOM_SLOTS)
        empty = s_map_zoom_replacement++ % MAP_ZOOM_SLOTS;
    s_map_zoom[empty].node = node;
    s_map_zoom[empty].target = s_map_zoom[empty].span = 1.0f;
    s_map_zoom[empty].baseline = 0.0f;
    return empty;
}

void mm3_map_zoom_reset(uint32_t node)
{
    unsigned slot;
    if (!node) return;
    slot = map_zoom_slot(node);
    s_map_zoom[slot].target = s_map_zoom[slot].span = 1.0f;
    s_map_zoom[slot].baseline = 0.0f;
}

void mm3_map_zoom_update(uint32_t node, float dt)
{
    unsigned slot;
    float blend;
    if (!node || !isfinite(dt) || dt <= 0.0f) return;
    /* Use simulation time, so pausing freezes the zoom. A long load/resume
     * update cannot snap it to the new target in a single frame. */
    if (dt > 0.1f) dt = 0.1f;
    slot = map_zoom_slot(node);
    blend = 1.0f - expf(-dt / 0.6f);
    s_map_zoom[slot].span +=
        (s_map_zoom[slot].target - s_map_zoom[slot].span) * blend;
}

void mm3_map_zoom_apply(uint32_t node, uint32_t player_record)
{
    unsigned slot;
    float speed, t, scale;
    if (!node || !player_record) return;
    slot = map_zoom_slot(node);
    /* 218844 takes absolute forward velocity, multiplies by 3.6 (3860F8)
     * and stores integer km/h at +4. This same per-player HUD record supplies
     * varPl1Speed/varPl2Speed, independent of the displayed speed units. */
    speed = fabsf((float)SMEM32(player_record + 4));
    t = (speed - 20.0f) / 100.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    s_map_zoom[slot].target = 1.0f + 0.25f * t * t * (3.0f - 2.0f * t);
    scale = MEMF(node + 0x50);
    /* 11FE40 fits stock zoom to checkpoint proximity. That fit can zoom in
     * while accelerating and cancel the speed zoom. Preserve this layout's
     * initial framing (including native split-screen scaling), then hold it
     * as the speed curve's baseline. Roads and 11CAE8's marker positions all
     * read +50; their window and icon size calculations remain native. */
    if (isfinite(scale) && scale > 0.0f) {
        if (!s_map_zoom[slot].baseline) s_map_zoom[slot].baseline = scale;
        MEMF(node + 0x50) = s_map_zoom[slot].baseline / s_map_zoom[slot].span;
    }
}
