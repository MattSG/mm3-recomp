#define _CRT_SECURE_NO_WARNINGS
#ifdef _WIN32
#include <windows.h>
#include "guest_state.h"
#include "xbox_memory_layout.h"
#endif
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "recomp_types.h"
#include "mm3_speedometer.h"

/* The shared header exposes MMX only to generated translation units. Its
 * eight-byte representation is also needed here for complete preservation. */
#ifndef RECOMP_MMX_DEFINED
#define RECOMP_MMX_DEFINED
typedef union RecompMmx {
    int8_t b[8]; uint8_t ub[8]; int16_t w[4]; uint16_t uw[4];
    int32_t d[2]; uint32_t ud[2]; uint64_t q;
} RecompMmx;
#endif
extern RECOMP_TLS RecompMmx g_mm0, g_mm1, g_mm2, g_mm3;
extern RECOMP_TLS RecompMmx g_mm4, g_mm5, g_mm6, g_mm7;

extern void sub_001C1585(void);

/* The retail dial is a texture with fixed numbers. The installer publishes
 * these per-car ranges alongside the archives; camera maxima still control
 * the interior model's needle independently. No host pointer enters a guest
 * draw call, and an absent/invalid setting retains the complete retail dial. */
static struct { char alias[32]; float kmh, mph; } ranges[256];
static unsigned range_count;
static int initialized;

static int mapped(uint32_t va, size_t size)
{
    return size <= UINT32_MAX && xbox_GuestMapped(va, (uint32_t)size);
}

static unsigned ascii_lower(unsigned c)
{
    return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
}

static int same_alias(const char *a, const char *b)
{
    for (; *a && *b; ++a, ++b)
        if (ascii_lower((unsigned char)*a) != ascii_lower((unsigned char)*b))
            return 0;
    return *a == *b;
}

static int original_alias(const char *alias)
{
    static const char *const names[] = {
        "AudiTT", "Avalanche", "Avant", "ChevySSR", "Corvette", "Crossfire",
        "Koenigsegg", "Lotus", "MFastBack", "NewBeetle", "Saab93",
        "SerpentMk2", "Viper"
    };
    unsigned i;
    for (i = 0; i < sizeof names / sizeof names[0]; ++i)
        if (same_alias(names[i], alias)) return 1;
    return 0;
}

static int valid_alias(const char *alias)
{
    const unsigned char *p = (const unsigned char *)alias;
    if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z'))) return 0;
    for (; *p; ++p)
        if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
              (*p >= '0' && *p <= '9') || *p == '_')) return 0;
    return 1;
}

static void load_ranges(void)
{
    FILE *file;
    char line[192], alias[32], extra;
    float kmh, mph;
    if (initialized) return;
    initialized = 1;
    file = fopen("game_files/Data/mm3_speedometers.ini", "rb");
    if (!file) return;
    while (fgets(line, sizeof line, file)) {
        unsigned i;
        char *p = line;
        while (*p == ' ' || *p == '\t') ++p;
        if (*p == '#' || *p == '\r' || *p == '\n' || !*p) continue;
        if (sscanf(p, "%31s %f %f %c", alias, &kmh, &mph, &extra) != 3 ||
            !valid_alias(alias) || !isfinite(kmh) || !isfinite(mph) ||
            kmh < 100.0f || kmh > 1200.0f || mph < 60.0f || mph > 750.0f)
            continue;
        for (i = 0; i < range_count; ++i)
            if (same_alias(ranges[i].alias, alias)) break;
        if (i == range_count && range_count == sizeof ranges / sizeof ranges[0])
            continue;
        if (i == range_count) ++range_count;
        strcpy(ranges[i].alias, alias);
        ranges[i].kmh = kmh;
        ranges[i].mph = mph;
    }
    fclose(file);
}

static int player_alias(char alias[32])
{
    uint32_t manager, race, player, text, length, capacity;
    if (!mapped(0x003C5CDCu, 4)) return 0;
    manager = MEM32(0x003C5CDCu);
    if (!mapped(manager, 0x58)) return 0;
    race = MEM32(manager + 0x54);
    if (!mapped(race, 0xDC)) return 0;
    player = MEM32(race + 0xD8);
    if (!mapped(player, 0x1F8)) return 0;
    text = player + 0x1DC;
    length = MEM32(text + 0x14);
    capacity = MEM32(text + 0x18);
    if (!length || length >= 32 || capacity < length) return 0;
    text = capacity < 16 ? text + 4 : MEM32(text + 4);
    if (!mapped(text, (size_t)length + 1) || MEM8(text + length)) return 0;
    memcpy(alias, (const void *)XBOX_PTR(text), length);
    alias[length] = 0;
    return valid_alias(alias);
}

typedef struct {
    uint32_t scratch, renderer, effects, frame;
    float cx, cy;
    const xbox_guest_state *state;
} Dial;

static void quad(const Dial *dial, const float xy[8], float r, float g, float b)
{
    uint32_t rectangle = dial->scratch + 0x200, data = dial->scratch + 0x240;
    static const float uv[8] = {0, 0, 1, 0, 1, 1, 0, 1};
    unsigned i;
    /* Each native draw starts with the same complete guest state. The effect
     * chain can use more than its visible integer argument registers. */
    xbox_guest_state_restore(dial->state, dial->state->eax);
    memset((void *)XBOX_PTR(data), 0, 0xA0);
    MEMF(rectangle) = dial->cx - 66.0f;
    MEMF(rectangle + 4) = dial->cy - 66.0f;
    MEMF(rectangle + 8) = MEMF(rectangle + 12) = 132.0f;
    MEMF(rectangle + 16) = 0.0f;
    MEM32(data) = rectangle; MEM32(data + 4) = 7; MEM32(data + 8) = 8;
    for (i = 0; i < 4; ++i) {
        uint32_t vertex = data + 0xC + i * 0x24;
        MEMF(vertex) = xy[i * 2]; MEMF(vertex + 4) = xy[i * 2 + 1];
        MEMF(vertex + 0xC) = uv[i * 2]; MEMF(vertex + 0x10) = uv[i * 2 + 1];
        MEMF(vertex + 0x14) = r; MEMF(vertex + 0x18) = g;
        MEMF(vertex + 0x1C) = b; MEMF(vertex + 0x20) = 1.0f;
    }
    g_esp = dial->scratch; g_ebp = dial->frame; g_seh_ebp = dial->frame;
    g_eax = dial->effects; g_edx = dial->renderer;
    PUSH32(g_esp, data); PUSH32(g_esp, 0); PUSH32(g_esp, 0x0023FAD2u);
    RECOMP_ABI_CALL(0x001C1585u, sub_001C1585);
    xbox_guest_state_restore(dial->state, dial->state->eax);
}

static void box(const Dial *dial, float x, float y, float w, float h,
                float r, float g, float b)
{
    const float xy[8] = {x, y, x + w, y, x + w, y + h, x, y + h};
    quad(dial, xy, r, g, b);
}

static void line(const Dial *dial, float x1, float y1, float x2, float y2,
                 float width, float r, float g, float b)
{
    float dx = x2 - x1, dy = y2 - y1, length = sqrtf(dx * dx + dy * dy);
    float nx, ny, xy[8];
    if (length <= 0.0f) return;
    nx = dy * width * 0.5f / length; ny = -dx * width * 0.5f / length;
    xy[0] = x1 + nx; xy[1] = y1 + ny; xy[2] = x2 + nx; xy[3] = y2 + ny;
    xy[4] = x2 - nx; xy[5] = y2 - ny; xy[6] = x1 - nx; xy[7] = y1 - ny;
    quad(dial, xy, r, g, b);
}

static void disk(const Dial *dial)
{
    static RECOMP_TLS struct { float c, s; } circle[41];
    static RECOMP_TLS int ready;
    unsigned i;
    /* The dial background does not change with the car, units or camera.
     * Retain the original double angles and float rounding, but compute each
     * boundary once per guest thread instead of twice every frame. */
    if (!ready) {
        for (i = 0; i <= 40; ++i) {
            double a = i * 6.283185307179586 / 40.0;
            circle[i].c = (float)cos(a);
            circle[i].s = (float)sin(a);
        }
        ready = 1;
    }
    for (i = 0; i < 40; ++i) {
        const float xy[8] = {dial->cx, dial->cy,
            dial->cx + 56.0f * circle[i].c, dial->cy + 56.0f * circle[i].s,
            dial->cx + 56.0f * circle[i + 1].c, dial->cy + 56.0f * circle[i + 1].s,
            dial->cx, dial->cy};
        quad(dial, xy, 0.035f, 0.045f, 0.055f);
    }
}

static const float (*tick_directions(double start, double end))[2]
{
    static RECOMP_TLS struct {
        double start, end;
        float directions[17][2];
        int ready;
    } cache;
    unsigned i;
    /* Camera type/units may select another arc. Its angles remain fixed
     * while driving; positions and labels still use the current frame. */
    if (!cache.ready || cache.start != start || cache.end != end) {
        for (i = 0; i <= 16; ++i) {
            double a = (start + (end - start) * i / 16.0) * 0.017453292519943295;
            cache.directions[i][0] = (float)cos(a);
            cache.directions[i][1] = (float)sin(a);
        }
        cache.start = start; cache.end = end; cache.ready = 1;
    }
    return cache.directions;
}

static void digit(const Dial *dial, unsigned value, float x, float y)
{
    static const unsigned char masks[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66,
        0x6D, 0x7D, 0x07, 0x7F, 0x6F};
    static const float segments[7][4] = {{0, 0, 3, .8f}, {2.2f, .5f, .8f, 3},
        {2.2f, 3.5f, .8f, 3}, {0, 6.2f, 3, .8f}, {0, 3.5f, .8f, 3},
        {0, .5f, .8f, 3}, {0, 3.1f, 3, .8f}};
    unsigned i;
    for (i = 0; i < 7; ++i)
        if (masks[value] & (1u << i))
            box(dial, x + segments[i][0], y + segments[i][1],
                segments[i][2], segments[i][3], .93f, .95f, .98f);
}

static void number(const Dial *dial, unsigned value, float x, float y)
{
    char text[8];
    size_t i, length;
    snprintf(text, sizeof text, "%u", value);
    length = strlen(text);
    x -= ((float)length * 4.0f - 1.0f) * .5f;
    y -= 3.5f;
    for (i = 0; i < length; ++i) digit(dial, (unsigned)(text[i] - '0'), x + (float)i * 4.0f, y);
}

static void units(const Dial *dial, int metric)
{
    /* The analog gauge supplies its units beside the needle. */
    float x = dial->cx - (metric ? 10.0f : 7.0f), y = dial->cy + 15.0f;
    float r = .62f, g = .69f, b = .77f;
    unsigned i;
    const char *text = metric ? "KM/H" : "MPH";
    for (i = 0; text[i]; ++i, x += 5.0f) {
        char c = text[i];
        if (c != '/') line(dial, x, y, x, y + 5, .7f, r, g, b);
        if (c == 'K') {
            line(dial, x, y + 2.5f, x + 3, y, .7f, r, g, b);
            line(dial, x, y + 2.5f, x + 3, y + 5, .7f, r, g, b);
        } else if (c == 'M') {
            line(dial, x, y, x + 1.5f, y + 2, .7f, r, g, b);
            line(dial, x + 1.5f, y + 2, x + 3, y, .7f, r, g, b);
            line(dial, x + 3, y, x + 3, y + 5, .7f, r, g, b);
        } else if (c == 'H') {
            line(dial, x + 3, y, x + 3, y + 5, .7f, r, g, b);
            line(dial, x, y + 2.5f, x + 3, y + 2.5f, .7f, r, g, b);
        } else if (c == 'P') {
            line(dial, x, y, x + 3, y, .7f, r, g, b);
            line(dial, x + 3, y, x + 3, y + 2.5f, .7f, r, g, b);
            line(dial, x, y + 2.5f, x + 3, y + 2.5f, .7f, r, g, b);
        } else line(dial, x, y + 5, x + 3, y, .7f, r, g, b);
    }
}

int mm3_speedometer_draw(uint32_t node, uint32_t frame, double speed_kmh)
{
    static char last_alias[32];
    static float last_maximum;
    static int last_metric;
    char alias[32];
    uint32_t type, index;
    unsigned i;
    int metric;
    float maximum;
    double start, end, value, angle;
    const float (*directions)[2];
    float needle_c, needle_s;
    xbox_guest_state state;
    Dial dial;
    if (!mapped(node, 0x34) || !mapped(frame - 352u, 0x180) ||
        MEM32(frame - 344u) == 1u) return 0;
    type = MEM32(frame - 36u);
    if (type != 1u && type != 4u) return 0;
    /* Added cars retain their donor's complete native gauge: the retail
     * artwork includes its font, tick marks and units, and the original
     * needle is calibrated to those labels. Also ignore stale extra-car
     * overrides left by an older installer. */
    if (!player_alias(alias) || !original_alias(alias)) return 0;
    load_ranges();
    if (!range_count) return 0;
    for (i = 0; i < range_count; ++i)
        if (same_alias(ranges[i].alias, alias)) break;
    if (i == range_count) return 0;
    metric = MEM8(frame + 0x13u) != 0;
    maximum = metric ? ranges[i].kmh : ranges[i].mph;
    index = (type * 2u + (metric ? 0u : 1u)) * 4u;
    if (!mapped(0x0039E264u + index, 4) || !mapped(0x0039E28Cu + index, 4) ||
        g_esp < 0x10900u || !mapped(g_esp - 0x900u, 0x900) ||
        !mapped(g_esi, 4)) return 0;
    start = MEMF(0x0039E264u + index);
    end = 540.0 - MEMF(0x0039E28Cu + index);
    dial.cx = MEMF(frame - 76u) + 81.0f;
    dial.cy = MEMF(frame - 72u) + 81.0f;
    if (!isfinite(start) || !isfinite(end) || end <= start || end - start > 360.0 ||
        !isfinite(dial.cx) || !isfinite(dial.cy)) return 0;
    xbox_guest_state_save(&state);
    dial.scratch = g_esp - 0x500u; dial.renderer = g_esi;
    dial.effects = MEM32(frame + 0x18u); dial.frame = frame; dial.state = &state;
    disk(&dial);
    directions = tick_directions(start, end);
    for (i = 0; i <= 16; ++i) {
        float c = directions[i][0], s = directions[i][1], inner = i % 2 ? 52.0f : 50.0f;
        line(&dial, dial.cx + inner * c, dial.cy + inner * s,
            dial.cx + 55.0f * c, dial.cy + 55.0f * s, i % 2 ? .8f : 1.2f, .73f, .8f, .88f);
        if (!(i % 2))
            number(&dial, (unsigned)(maximum * i / 16.0f + .5f),
                   dial.cx + 42.0f * c, dial.cy + 42.0f * s);
    }
    units(&dial, metric);
    value = metric ? speed_kmh : speed_kmh * 0.6213712096214294;
    if (!isfinite(value) || value < 0.0) value = 0.0;
    if (value > maximum) value = maximum;
    angle = (start + (end - start) * value / maximum) * 0.017453292519943295;
    needle_c = (float)cos(angle); needle_s = (float)sin(angle);
    line(&dial, dial.cx - 5.0f * needle_c, dial.cy - 5.0f * needle_s,
         dial.cx + 64.0f * needle_c, dial.cy + 64.0f * needle_s,
         1.6f, 1.0f, .26f, .10f);
    box(&dial, dial.cx - 2.0f, dial.cy - 2.0f, 4.0f, 4.0f, .87f, .91f, .96f);
    xbox_guest_state_restore(&state, state.eax);
    if (!same_alias(last_alias, alias) || maximum != last_maximum || metric != last_metric) {
        fprintf(stderr, "[SPEEDOMETER] alias=%s maximum=%.9g units=%s type=%u\n",
                alias, maximum, metric ? "km/h" : "mph", type);
        strcpy(last_alias, alias); last_maximum = maximum; last_metric = metric;
    }
    return 1;
}
