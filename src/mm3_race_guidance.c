#include <math.h>
#include "recomp_types.h"

/* 201BCA publishes the native race's ordered checkpoint list once per player
 * HUD update. Keep copied positions and identities, never dereference a saved
 * guest object. The map and world hoop draw both consume this same ordering. */
enum { RACE_PLAYERS = 2, RACE_CHECKPOINTS = 128 };
typedef struct { uint32_t object; float x, z; } RaceCheckpoint;
static struct {
    RaceCheckpoint checkpoints[RACE_CHECKPOINTS];
    uint32_t current_object;
    unsigned count, current, valid, closed;
} s_race[RACE_PLAYERS];

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
                                  uint32_t position, uint32_t current)
{
    unsigned count;
    float x, z;
    if (player >= RACE_PLAYERS || !object || !position) return;
    count = s_race[player].count;
    if (count >= RACE_CHECKPOINTS) return;
    x = MEMF(position); z = MEMF(position + 8);
    if (!isfinite(x) || !isfinite(z)) return;
    s_race[player].checkpoints[count].object = object;
    s_race[player].checkpoints[count].x = x;
    s_race[player].checkpoints[count].z = z;
    s_race[player].current_object = current;
    if (object == current) s_race[player].current = count;
    s_race[player].count = count + 1;
}

void mm3_race_guidance_end(uint32_t player)
{
    unsigned i;
    if (player >= RACE_PLAYERS) return;
    for (i = 0; i < s_race[player].count; ++i)
        if (s_race[player].checkpoints[i].object == s_race[player].current_object) {
            s_race[player].current = i;
            s_race[player].valid = 1;
            return;
        }
}

static unsigned checkpoint_rank(unsigned player, unsigned index)
{
    unsigned current = s_race[player].current;
    if (index == current) return 0;
    if (current + 1 < s_race[player].count) {
        if (index == current + 1) return 1;
    } else if (s_race[player].closed && index == 0) return 1;
    return 2;
}

/* MM3's packed draw colors are RGBA with alpha in the low byte. */
uint32_t mm3_race_world_checkpoint_color(uint32_t object, uint32_t native_color)
{
    uint32_t context = MEM32(object + 0x54);
    unsigned player, i;
    if (!context) return native_color;
    /* Native 16F160 sets this field together with the current camera. */
    player = MEM32(context + 0x15218);
    if (player >= RACE_PLAYERS || !s_race[player].valid) return native_color;
    /* Retain the title's green hoop hue, make the first target opaque and
     * second clearly visible, while distant/completed hoops stay subdued. */
    for (i = 0; i < s_race[player].count; ++i)
        if (s_race[player].checkpoints[i].object == object + 0x18) {
            unsigned rank = checkpoint_rank(player, i);
            return (MEM32(0x3BA52C) & 0xFFFFFF00u) | (rank == 0 ? 255u : rank == 1 ? 150u : 28u);
        }
    return native_color;
}

void mm3_race_map_checkpoint_style(uint32_t table, uint32_t marker,
                                   uint32_t color, uint32_t size)
{
    unsigned player, i, best = 3;
    float x, z, scale;
    if (table < 0x407F78 || table >= 0x407F78 + 0x10 * RACE_PLAYERS ||
        ((table - 0x407F78) & 15) || !marker || !color ||
        (MEM32(marker + 12) & 0x1F) != 0) return;
    player = (table - 0x407F78) / 0x10;
    if (!s_race[player].valid) return;
    x = MEMF(marker); z = MEMF(marker + 8);
    for (i = 0; i < s_race[player].count; ++i) {
        const RaceCheckpoint *checkpoint = &s_race[player].checkpoints[i];
        if (fabsf(checkpoint->x - x) > .01f || fabsf(checkpoint->z - z) > .01f) continue;
        if (checkpoint_rank(player, i) < best) best = checkpoint_rank(player, i);
    }
    if (best == 3) return;
    /* Courses can revisit a crossing. Native current flag and best matching
     * order keep an earlier checkpoint at that position from hiding target. */
    if (MEM32(marker + 12) & 0x80) best = 0;
    /* Override the current marker's pulse floor so it stays readable;
     * next marker has a smaller amber icon, other icons are dim and small. */
    MEMF(color) = best == 1 ? 1.0f : .5f;
    MEMF(color + 4) = best == 1 ? .72f : 1.0f;
    MEMF(color + 8) = best == 1 ? .15f : .6f;
    MEMF(color + 12) = best == 0 ? 1.0f : best == 1 ? .75f : .14f;
    scale = best == 0 ? 1.4f : best == 1 ? 1.15f : .75f;
    if (size) MEMF(size) *= scale;
}

extern void sub_001C1585(void);

static void route_segment(uint32_t renderer, uint32_t clip,
                           float ax, float ay, float bx, float by, float alpha)
{
    float dx = bx - ax, dy = by - ay, length = hypotf(dx, dy), px, py;
    uint32_t saved_eax, saved_ecx, saved_edx, saved_esp, saved_ebx, saved_esi, saved_edi;
    uint32_t saved_ebp, saved_seh_ebp, quad;
    double saved_fp[8];
    int saved_top, saved_cmp, saved_df;
    uint16_t saved_cc, saved_control;
    unsigned i;
    const float thickness = 1.25f;
    float xy[4][2];
    if (!isfinite(length) || length < .01f) return;
    px = -dy * thickness / length; py = dx * thickness / length;
    xy[0][0] = ax + px; xy[0][1] = ay + py;
    xy[1][0] = bx + px; xy[1][1] = by + py;
    xy[2][0] = bx - px; xy[2][1] = by - py;
    xy[3][0] = ax - px; xy[3][1] = ay - py;
    saved_eax = g_eax; saved_ecx = g_ecx; saved_edx = g_edx; saved_esp = g_esp;
    saved_ebx = g_ebx; saved_esi = g_esi; saved_edi = g_edi;
    saved_ebp = g_ebp; saved_seh_ebp = g_seh_ebp;
    saved_top = g_fp_top; saved_cmp = g_fp_cmp; saved_df = g_df;
    saved_cc = g_fp_cc; saved_control = g_fp_control_word;
    memcpy(saved_fp, g_fp_stack, sizeof saved_fp);
    /* Same 0xA0 native quad structure as 2DC95; scratch belongs to this
     * synchronous submission and is below the live guest stack frame. */
    g_esp -= 0xA0; quad = g_esp;
    memset((void *)XBOX_PTR(quad), 0, 0xA0);
    MEM32(quad + 4) = 7; MEM32(quad + 8) = 8;
    for (i = 0; i < 4; ++i) {
        uint32_t vertex = quad + 0xC + i * 0x24;
        MEMF(vertex) = xy[i][0]; MEMF(vertex + 4) = xy[i][1];
        MEMF(vertex + 0x14) = 1.0f;
        MEMF(vertex + 0x18) = .72f;
        MEMF(vertex + 0x1C) = .15f;
        MEMF(vertex + 0x20) = alpha;
    }
    PUSH32(g_esp, quad); PUSH32(g_esp, 0);
    g_eax = clip; g_edx = renderer;
    PUSH32(g_esp, 0x0011CB45u);
    RECOMP_ABI_CALL(0x001C1585u, sub_001C1585);
    g_eax = saved_eax; g_ecx = saved_ecx; g_edx = saved_edx; g_esp = saved_esp;
    g_ebx = saved_ebx; g_esi = saved_esi; g_edi = saved_edi;
    g_ebp = saved_ebp; g_seh_ebp = saved_seh_ebp;
    g_fp_top = saved_top; g_fp_cmp = saved_cmp; g_df = saved_df;
    g_fp_cc = saved_cc; g_fp_control_word = saved_control;
    memcpy(g_fp_stack, saved_fp, sizeof saved_fp);
}

void mm3_race_map_route(uint32_t node, uint32_t table,
                         uint32_t renderer, uint32_t clip, uint32_t layout)
{
    unsigned player, i;
    float scale, width, angle, cosine, sine, radius, cx, cy;
    float positions[RACE_CHECKPOINTS][2];
    if (!node || !renderer || !layout || table < 0x407F78 ||
        table >= 0x407F78 + 0x10 * RACE_PLAYERS || ((table - 0x407F78) & 15)) return;
    player = (table - 0x407F78) / 0x10;
    if (!s_race[player].valid || s_race[player].count < 2) return;
    scale = MEMF(node + 0x50); width = MEMF(node + 0x58) - MEMF(node + 0x54);
    angle = MEMF(node + 0x4C); cx = MEMF(node + 0x94); cy = MEMF(node + 0x98);
    radius = MEMF(layout + 8) * .5f - MEMF(node + 0xB4) * 8.0f;
    if (!isfinite(scale) || scale <= 0 || !isfinite(width) || width <= 0 ||
        !isfinite(angle) || !isfinite(cx) || !isfinite(cy) || !isfinite(radius) || radius <= 0) return;
    cosine = cosf(angle); sine = sinf(angle);
    for (i = 0; i < s_race[player].count; ++i) {
        float x = -s_race[player].checkpoints[i].x * scale * 1024.0f / width;
        float y = s_race[player].checkpoints[i].z * scale * 1024.0f / width;
        /* C6ADC rotates clockwise in screen space around the native centre. */
        positions[i][0] = x * cosine + y * sine;
        positions[i][1] = y * cosine - x * sine;
    }
    for (i = 0; i + 1 < s_race[player].count + s_race[player].closed; ++i) {
        unsigned next = (i + 1) % s_race[player].count;
        float ax = positions[i][0], ay = positions[i][1];
        float bx = positions[next][0], by = positions[next][1];
        float dx = bx - ax, dy = by - ay;
        float a = dx * dx + dy * dy, b = 2 * (ax * dx + ay * dy);
        float c = ax * ax + ay * ay - radius * radius, discriminant, lo, hi;
        /* Clip actual segments to the round map, not their endpoints: a
         * crossing route remains visible even with both checkpoints off-map. */
        if (a < .0001f) continue;
        discriminant = b * b - 4 * a * c;
        if (discriminant < 0) continue;
        lo = (-b - sqrtf(discriminant)) / (2 * a);
        hi = (-b + sqrtf(discriminant)) / (2 * a);
        if (lo < 0) lo = 0; if (hi > 1) hi = 1;
        if (lo >= hi) continue;
        route_segment(renderer, clip, cx + ax + lo * dx, cy + ay + lo * dy,
                      cx + ax + hi * dx, cy + ay + hi * dy,
                      next == s_race[player].current ? .9f : .6f);
    }
}
