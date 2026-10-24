#define WIN32_LEAN_AND_MEAN
#include "xbox_memory_layout.h"
#include "platform/xbox_winnt.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mm3_recomp_types.h"
#include "mm3_building_diagnostics.h"

extern void sub_00168A77_original(void);
extern void sub_001C7B15_original(void);
extern void sub_001A67BA_original(void);
extern void sub_00166E01_original(void);
extern void sub_001681FB_original(void);
extern void sub_00167DA5_original(void);
extern void sub_00166B3E_original(void);
extern void sub_00162F75_original(void);

#ifndef MM3_BUILDING_CLOCK
#define MM3_BUILDING_CLOCK GetTickCount64
#endif
#ifndef MM3_BUILDING_LOG
#define MM3_BUILDING_LOG(...) fprintf(stderr, __VA_ARGS__)
#endif
#ifndef MM3_BUILDING_FLUSH
#define MM3_BUILDING_FLUSH() fflush(stderr)
#endif

/* Sampling also bounds checked guest reads. State changes remain pending if
 * the line budget is exhausted, so the next second can report them instead
 * of permanently losing a building encountered during a busy first frame. */
#define BUILDING_SLOTS 1024u
#define BUILDING_SAMPLE_MS 100u
#define BUILDING_OBJECT_REPORT_MS 250u
#define BUILDING_LINES_PER_SECOND 64u
#define BUILDING_FAILURE_LINES_PER_SECOND 8u

typedef struct BuildingSlot {
    uint32_t object;
    uint64_t sample_ms, report_ms, reported_key;
    uint64_t failure_report_ms;
    int sampled, reported;
    int failure_reported;
} BuildingSlot;

typedef struct BuildingStream {
    uint32_t before[3], after[3]; /* Native head, tail and vertex capacity. */
    int before_read, after_read;
} BuildingStream;

typedef struct BuildingAllocation {
    uint32_t calls, successes, failures, quads, successful_quads;
} BuildingAllocation;

typedef struct BuildingFrame {
    struct BuildingFrame *previous;
    BuildingSlot *slot;
    uint64_t now;
    uint32_t object, stack, caller, camera, planes, plane_count, mask;
    uint32_t owner, renderer, queue, auxiliary, camera_object, render_mode;
    uint32_t floor_cap, descriptor_before, descriptor_after, draw_before, draw_after;
    uint32_t batch_before, batch_after, head_before, head_after, tail_before, tail_after, vertex_cap;
    uint16_t columns, rows;
    float center[3], extent[3], camera_xyz[3], center_distance, distance_squared;
    int object_read, camera_read, renderer_read, queue_read, counters_read;
    int frustum_reject, auxiliary_reject, camera_inside, distance_read, tier;
    BuildingStream streams[2];
    BuildingAllocation mid_alloc[2], upper_alloc[2];
    uint32_t alloc_unknown_stream, alloc_caller, alloc_queue, alloc_quads, alloc_result;
    uint32_t failed_caller, failed_queue, failed_quads;
    int alloc_stream;
    int failed_stream;
    uint32_t cell_frustum_calls, cell_frustum_rejects, cell_auxiliary_calls, cell_auxiliary_rejects;
} BuildingFrame;

typedef struct BuildingSummary {
    uint64_t start;
    uint32_t calls, samples, tiers[4], frustum, auxiliary, empty_geometry, unknown_geometry, lines, suppressed;
    uint32_t allocation_successes, allocation_failures, cell_frustum_rejects, cell_auxiliary_rejects;
    uint32_t failure_lines, failure_suppressed;
} BuildingSummary;

static volatile LONG s_enabled = -1;
static RECOMP_TLS BuildingSlot s_slots[BUILDING_SLOTS];
static RECOMP_TLS BuildingFrame *s_active;
static RECOMP_TLS BuildingSummary s_summary;

void mm3_building_diagnostics_init(void)
{
    if (s_enabled < 0) {
        const char *value = getenv("MM3_BUILDING_TRACE");
        LONG enabled = value && *value && strcmp(value, "0") != 0;
        if (InterlockedCompareExchange(&s_enabled, enabled, -1) == -1 && enabled) {
            MM3_BUILDING_LOG("[BUILDING_TRACE] enabled sample_ms=%u lines_per_second=%u failure_lines_per_second=%u tracked=%u clock=GetTickCount64 tier=actual_title_path geometry=guest_queue_delta streams=28byte_1EC8_1F24 alloc=mid_168070_upper_166441 allocation_fields=calls:successes:failures:requested_quads\n",
                             BUILDING_SAMPLE_MS, BUILDING_LINES_PER_SECOND,
                             BUILDING_FAILURE_LINES_PER_SECOND, BUILDING_SLOTS);
            MM3_BUILDING_FLUSH();
        }
    }
}

static int enabled(void)
{
    if (s_enabled < 0) mm3_building_diagnostics_init();
    return s_enabled != 0;
}

/* Diagnostic reads must not introduce a guest fault. Reads are checked even
 * for pointer fields which retail subsequently dereferences on its own. */
static int read_guest(uint32_t address, void *out, size_t size)
{
    return xbox_GuestRead(address, out, size);
}

static uint32_t read_word(uint32_t address)
{
    uint32_t result = 0;
    read_guest(address, &result, sizeof result);
    return result;
}

static BuildingSlot *find_slot(uint32_t object)
{
    unsigned home = ((object >> 4) ^ (object >> 14)) & (BUILDING_SLOTS - 1u), i;
    BuildingSlot *oldest = &s_slots[home];
    for (i = 0; i < BUILDING_SLOTS; ++i) {
        BuildingSlot *slot = &s_slots[(home + i) & (BUILDING_SLOTS - 1u)];
        if (slot->object == object) return slot;
        if (!slot->object) { slot->object = object; return slot; }
        if (slot->sample_ms < oldest->sample_ms) oldest = slot;
    }
    memset(oldest, 0, sizeof *oldest);
    oldest->object = object;
    return oldest;
}

static void snapshot_streams(BuildingFrame *frame, int after)
{
    static const uint32_t offsets[2] = {0x1EC8u, 0x1F24u};
    unsigned i;
    for (i = 0; i < 2; ++i) {
        BuildingStream *stream = &frame->streams[i];
        uint32_t values[3];
        int valid = frame->queue && frame->queue <= UINT32_MAX - offsets[i] &&
                    read_guest(frame->queue + offsets[i], values, sizeof values);
        if (after) stream->after_read = valid;
        else stream->before_read = valid;
        if (valid) memcpy(after ? stream->after : stream->before, values, sizeof values);
    }
}

static void snapshot(BuildingFrame *frame)
{
    unsigned char object[0x70], renderer[0x16C];
    uint32_t counters[2], queue[4];
    if (read_guest(frame->object, object, sizeof object)) {
        double squared = 0;
        unsigned i;
        frame->object_read = 1;
        memcpy(&frame->owner, object, 4);
        memcpy(&frame->columns, object + 8, 2);
        memcpy(&frame->rows, object + 10, 2);
        memcpy(frame->center, object + 0x48, sizeof frame->center);
        memcpy(frame->extent, object + 0x54, sizeof frame->extent);
        frame->camera_read = read_guest(frame->camera, frame->camera_xyz, sizeof frame->camera_xyz);
        if (frame->camera_read) {
            for (i = 0; i < 3; ++i) {
                double delta = (double)frame->camera_xyz[i] - frame->center[i];
                squared += delta * delta;
            }
            frame->center_distance = (float)sqrt(squared);
        }
        frame->renderer = read_word(frame->owner + 0x374u);
        /* PAL facade renderer fields 15214..1537F: render mode, auxiliary
         * exclusion object, current camera, and geometry queue pointer. */
        if (frame->renderer && read_guest(frame->renderer + 0x15214u, renderer, sizeof renderer)) {
            frame->renderer_read = 1;
            memcpy(&frame->render_mode, renderer, 4);
            memcpy(&frame->auxiliary, renderer + 8, 4);
            memcpy(&frame->camera_object, renderer + 0x154, 4);
            memcpy(&frame->queue, renderer + 0x164, 4);
        }
    }
    frame->floor_cap = read_word(0x003A8C3Cu);
    if (read_guest(0x003C57A0u, counters, sizeof counters)) {
        frame->counters_read = 1;
        frame->descriptor_before = counters[0];
        frame->draw_before = counters[1];
    }
    if (frame->queue && read_guest(frame->queue + 0x1F80u, queue, sizeof queue)) {
        frame->queue_read = 1;
        frame->head_before = queue[0];
        frame->tail_before = queue[1];
        frame->vertex_cap = queue[2];
        frame->batch_before = read_word(frame->queue + 0x1E4Cu);
    }
    snapshot_streams(frame, 0);
}

static void finish_snapshot(BuildingFrame *frame)
{
    uint32_t counters[2], queue[2];
    if (frame->counters_read && read_guest(0x003C57A0u, counters, sizeof counters)) {
        frame->descriptor_after = counters[0];
        frame->draw_after = counters[1];
    }
    if (frame->queue_read && read_guest(frame->queue + 0x1F80u, queue, sizeof queue)) {
        frame->head_after = queue[0];
        frame->tail_after = queue[1];
        frame->batch_after = read_word(frame->queue + 0x1E4Cu);
    }
    frame->mask = read_word(frame->stack + 16u);
    snapshot_streams(frame, 1);
}

static int64_t delta(uint32_t before, uint32_t after)
{
    /* -1 signals a title queue reset/flush, rather than huge unsigned output. */
    return after >= before ? (int64_t)(after - before) : -1;
}

/* A mid facade and the upper-floor near fallback use separate 28-byte
 * streams. Unchanged near/far counters alone do not mean absent geometry. */
static int geometry_empty(const BuildingFrame *frame)
{
    unsigned i;
    int changed = frame->descriptor_after != frame->descriptor_before ||
                  frame->draw_after != frame->draw_before ||
                  frame->head_after != frame->head_before ||
                  frame->tail_after != frame->tail_before ||
                  frame->batch_after != frame->batch_before;
    if (!frame->queue_read || !frame->counters_read) return -1;
    for (i = 0; i < 2; ++i) {
        const BuildingStream *stream = &frame->streams[i];
        if (!stream->before_read || !stream->after_read) return -1;
        if (stream->before[0] != stream->after[0] || stream->before[1] != stream->after[1] ||
            frame->mid_alloc[i].successful_quads || frame->upper_alloc[i].successful_quads)
            changed = 1;
    }
    return !changed;
}

static uint64_t state_key(const BuildingFrame *frame)
{
    uint64_t key = 0xCBF29CE484222325ull;
    uint32_t words[] = {
        (uint32_t)frame->tier, (uint32_t)(frame->frustum_reject + 1),
        (uint32_t)(frame->auxiliary_reject + 1), (uint32_t)(frame->camera_inside + 1),
        frame->owner, frame->renderer, frame->queue, frame->auxiliary,
        frame->columns, frame->rows, frame->floor_cap,
        (uint32_t)delta(frame->descriptor_before, frame->descriptor_after),
        (uint32_t)delta(frame->draw_before, frame->draw_after),
        (uint32_t)delta(frame->batch_before, frame->batch_after),
        (uint32_t)delta(frame->head_before, frame->head_after),
        (uint32_t)delta(frame->tail_before, frame->tail_after),
        frame->cell_frustum_calls, frame->cell_frustum_rejects,
        frame->cell_auxiliary_calls, frame->cell_auxiliary_rejects,
        frame->alloc_unknown_stream, frame->alloc_caller, (uint32_t)frame->alloc_stream
    };
    unsigned i;
    for (i = 0; i < sizeof words / sizeof words[0]; ++i)
        key = (key ^ words[i]) * 0x100000001B3ull;
    for (i = 0; i < 2; ++i) {
        const BuildingStream *stream = &frame->streams[i];
        const BuildingAllocation *allocations[2] = {&frame->mid_alloc[i], &frame->upper_alloc[i]};
        unsigned j;
        key = (key ^ (uint32_t)stream->before_read) * 0x100000001B3ull;
        key = (key ^ (uint32_t)stream->after_read) * 0x100000001B3ull;
        key = (key ^ (uint32_t)delta(stream->before[0], stream->after[0])) * 0x100000001B3ull;
        key = (key ^ (uint32_t)delta(stream->before[1], stream->after[1])) * 0x100000001B3ull;
        key = (key ^ stream->after[2]) * 0x100000001B3ull;
        for (j = 0; j < 2; ++j) {
            key = (key ^ allocations[j]->calls) * 0x100000001B3ull;
            key = (key ^ allocations[j]->successes) * 0x100000001B3ull;
            key = (key ^ allocations[j]->failures) * 0x100000001B3ull;
            key = (key ^ allocations[j]->quads) * 0x100000001B3ull;
        }
    }
    return key;
}

static void summary(uint64_t now)
{
    if (!s_summary.start) s_summary.start = now;
    if (now - s_summary.start < 1000u) return;
    MM3_BUILDING_LOG("[BUILDING_SUMMARY] ms=%llu calls=%u samples=%u culled=%u auxiliary_reject=%u near=%u mid=%u far=%u no_path=%u zero_queue_delta=%u unknown_queue_delta=%u allocation_successes=%u allocation_failures=%u cell_frustum_rejects=%u cell_auxiliary_rejects=%u failure_events=%u failure_suppressed=%u events=%u suppressed=%u\n",
        (unsigned long long)now, s_summary.calls, s_summary.samples,
        s_summary.frustum, s_summary.auxiliary, s_summary.tiers[1],
        s_summary.tiers[2], s_summary.tiers[3], s_summary.tiers[0],
        s_summary.empty_geometry, s_summary.unknown_geometry,
        s_summary.allocation_successes, s_summary.allocation_failures,
        s_summary.cell_frustum_rejects, s_summary.cell_auxiliary_rejects,
        s_summary.failure_lines, s_summary.failure_suppressed,
        s_summary.lines, s_summary.suppressed);
    MM3_BUILDING_FLUSH();
    memset(&s_summary, 0, sizeof s_summary);
    s_summary.start = now;
}

static void report_allocation_failure(BuildingFrame *frame)
{
    BuildingSlot *slot = frame->slot;
    if (!frame->failed_caller) return;
    if (s_summary.failure_lines >= BUILDING_FAILURE_LINES_PER_SECOND ||
        (slot->failure_reported && frame->now - slot->failure_report_ms < BUILDING_OBJECT_REPORT_MS)) {
        ++s_summary.failure_suppressed;
        return;
    }
    ++s_summary.failure_lines;
    slot->failure_reported = 1;
    slot->failure_report_ms = frame->now;
    MM3_BUILDING_LOG("[BUILDING_ALLOCATION_FAILURE] ms=%llu facade=%08X camera_xyz=%.3f,%.3f,%.3f closest_distance=%.3f caller=%08X queue=%08X stream=%d requested_quads=%u result=00000000 stream0_head=%u:%u stream0_tail=%u:%u stream0_cap=%u:%u stream0_read=%d,%d stream1_head=%u:%u stream1_tail=%u:%u stream1_cap=%u:%u stream1_read=%d,%d failures_mid0_mid1_upper0_upper1=%u,%u,%u,%u\n",
        (unsigned long long)frame->now, frame->object,
        frame->camera_xyz[0], frame->camera_xyz[1], frame->camera_xyz[2],
        frame->distance_read && isfinite(frame->distance_squared) && frame->distance_squared >= 0
            ? sqrt((double)frame->distance_squared) : -1.0,
        frame->failed_caller, frame->failed_queue, frame->failed_stream, frame->failed_quads,
        frame->streams[0].before[0], frame->streams[0].after[0],
        frame->streams[0].before[1], frame->streams[0].after[1],
        frame->streams[0].before[2], frame->streams[0].after[2],
        frame->streams[0].before_read, frame->streams[0].after_read,
        frame->streams[1].before[0], frame->streams[1].after[0],
        frame->streams[1].before[1], frame->streams[1].after[1],
        frame->streams[1].before[2], frame->streams[1].after[2],
        frame->streams[1].before_read, frame->streams[1].after_read,
        frame->mid_alloc[0].failures, frame->mid_alloc[1].failures,
        frame->upper_alloc[0].failures, frame->upper_alloc[1].failures);
}

static void report(BuildingFrame *frame)
{
    static const char *names[] = {"none", "near", "mid", "far"};
    BuildingSlot *slot = frame->slot;
    uint64_t key = state_key(frame);
    int empty = geometry_empty(frame);
    unsigned i;
    ++s_summary.samples;
    ++s_summary.tiers[frame->tier];
    if (frame->frustum_reject == 1) ++s_summary.frustum;
    if (frame->auxiliary_reject == 1) ++s_summary.auxiliary;
    if (frame->tier && empty == 1) ++s_summary.empty_geometry;
    if (frame->tier && empty < 0) ++s_summary.unknown_geometry;
    for (i = 0; i < 2; ++i) {
        s_summary.allocation_successes += frame->mid_alloc[i].successes + frame->upper_alloc[i].successes;
        s_summary.allocation_failures += frame->mid_alloc[i].failures + frame->upper_alloc[i].failures;
    }
    s_summary.cell_frustum_rejects += frame->cell_frustum_rejects;
    s_summary.cell_auxiliary_rejects += frame->cell_auxiliary_rejects;
    /* Critical allocation failures have their own small renewable budget.
     * Unrelated ordinary facades cannot consume their available log lines. */
    report_allocation_failure(frame);
    if (slot->reported && slot->reported_key == key) return;
    if (s_summary.lines >= BUILDING_LINES_PER_SECOND ||
        (slot->reported && frame->now - slot->report_ms < BUILDING_OBJECT_REPORT_MS)) {
        ++s_summary.suppressed;
        return;
    }
    ++s_summary.lines;
    slot->reported = 1;
    slot->reported_key = key;
    slot->report_ms = frame->now;
    MM3_BUILDING_LOG("[BUILDING] ms=%llu facade=%08X owner=%08X caller=%08X tier=%s frustum_reject=%d auxiliary_reject=%d camera_inside=%d camera=%08X camera_xyz=%.3f,%.3f,%.3f center=%.3f,%.3f,%.3f extent=%.3f,%.3f,%.3f center_distance=%.3f closest_distance=%.3f grid=%ux%u detail_floor_cap=%u renderer=%08X mode=%u auxiliary=%08X camera_object=%08X planes=%08X count=%u mask=%08X queue=%08X descriptors=%u:%u draws=%u:%u batches=%u:%u head=%u:%u tail=%u:%u vertex_cap=%u read=%d,%d,%d,%d,%d stream0_head=%u:%u stream0_tail=%u:%u stream0_cap=%u:%u stream0_read=%d,%d stream1_head=%u:%u stream1_tail=%u:%u stream1_cap=%u:%u stream1_read=%d,%d mid0_alloc=%u:%u:%u:%u mid1_alloc=%u:%u:%u:%u upper0_alloc=%u:%u:%u:%u upper1_alloc=%u:%u:%u:%u alloc_unknown_stream=%u alloc_last_caller=%08X alloc_last_queue=%08X alloc_last_stream=%d alloc_last_quads=%u alloc_last_result=%08X cell_frustum=%u:%u cell_auxiliary=%u:%u zero_queue_delta=%d\n",
        (unsigned long long)frame->now, frame->object, frame->owner, frame->caller,
        names[frame->tier], frame->frustum_reject, frame->auxiliary_reject, frame->camera_inside,
        frame->camera, frame->camera_xyz[0], frame->camera_xyz[1], frame->camera_xyz[2],
        frame->center[0], frame->center[1], frame->center[2],
        frame->extent[0], frame->extent[1], frame->extent[2], frame->center_distance,
        frame->distance_read && isfinite(frame->distance_squared) && frame->distance_squared >= 0
            ? sqrt((double)frame->distance_squared) : -1.0,
        (unsigned)frame->columns, (unsigned)frame->rows, frame->floor_cap,
        frame->renderer, frame->render_mode, frame->auxiliary, frame->camera_object,
        frame->planes, frame->plane_count, frame->mask, frame->queue,
        frame->descriptor_before, frame->descriptor_after, frame->draw_before, frame->draw_after,
        frame->batch_before, frame->batch_after, frame->head_before, frame->head_after,
        frame->tail_before, frame->tail_after, frame->vertex_cap,
        frame->object_read, frame->camera_read, frame->renderer_read, frame->queue_read, frame->counters_read,
        frame->streams[0].before[0], frame->streams[0].after[0],
        frame->streams[0].before[1], frame->streams[0].after[1],
        frame->streams[0].before[2], frame->streams[0].after[2],
        frame->streams[0].before_read, frame->streams[0].after_read,
        frame->streams[1].before[0], frame->streams[1].after[0],
        frame->streams[1].before[1], frame->streams[1].after[1],
        frame->streams[1].before[2], frame->streams[1].after[2],
        frame->streams[1].before_read, frame->streams[1].after_read,
        frame->mid_alloc[0].calls, frame->mid_alloc[0].successes, frame->mid_alloc[0].failures, frame->mid_alloc[0].quads,
        frame->mid_alloc[1].calls, frame->mid_alloc[1].successes, frame->mid_alloc[1].failures, frame->mid_alloc[1].quads,
        frame->upper_alloc[0].calls, frame->upper_alloc[0].successes, frame->upper_alloc[0].failures, frame->upper_alloc[0].quads,
        frame->upper_alloc[1].calls, frame->upper_alloc[1].successes, frame->upper_alloc[1].failures, frame->upper_alloc[1].quads,
        frame->alloc_unknown_stream, frame->alloc_caller, frame->alloc_queue,
        frame->alloc_stream, frame->alloc_quads, frame->alloc_result,
        frame->cell_frustum_calls, frame->cell_frustum_rejects,
        frame->cell_auxiliary_calls, frame->cell_auxiliary_rejects, empty);
}

void sub_00168A77(void)
{
    BuildingFrame frame;
    BuildingSlot *slot;
    uint32_t arguments[5];
    uint64_t now;
    if (!enabled()) { sub_00168A77_original(); return; }
    now = MM3_BUILDING_CLOCK();
    summary(now);
    ++s_summary.calls;
    slot = find_slot(g_eax);
    if (slot->sampled && now - slot->sample_ms < BUILDING_SAMPLE_MS) {
        BuildingFrame *previous = s_active;
        s_active = NULL;
        sub_00168A77_original();
        s_active = previous;
        return;
    }
    slot->sampled = 1;
    slot->sample_ms = now;
    memset(&frame, 0, sizeof frame);
    frame.previous = s_active;
    frame.slot = slot;
    frame.now = now;
    frame.object = g_eax;
    frame.stack = g_esp;
    frame.frustum_reject = frame.auxiliary_reject = frame.camera_inside = -1;
    frame.alloc_stream = -1;
    frame.failed_stream = -1;
    if (read_guest(g_esp, arguments, sizeof arguments)) {
        frame.caller = arguments[0];
        frame.camera = arguments[1];
        frame.planes = arguments[2];
        frame.plane_count = arguments[3];
        frame.mask = arguments[4];
    }
    snapshot(&frame);
    s_active = &frame;
    sub_00168A77_original();
    s_active = frame.previous;
    finish_snapshot(&frame);
    report(&frame);
}

void sub_001C7B15(void)
{
    BuildingFrame *frame = s_active;
    uint32_t caller = frame ? read_word(g_esp) : 0;
    sub_001C7B15_original();
    if (caller == 0x00168A92u) frame->frustum_reject = (int)(g_eax & 255u);
    if (caller == 0x0016886Au) {
        ++frame->cell_frustum_calls;
        if (g_eax & 255u) ++frame->cell_frustum_rejects;
    }
}

void sub_001A67BA(void)
{
    BuildingFrame *frame = s_active;
    uint32_t caller = frame ? read_word(g_esp) : 0;
    sub_001A67BA_original();
    if (caller == 0x00168AB6u) frame->auxiliary_reject = (int)(g_eax & 255u);
    if (caller == 0x00168889u) {
        ++frame->cell_auxiliary_calls;
        if (g_eax & 255u) ++frame->cell_auxiliary_rejects;
    }
}

void sub_00166E01(void)
{
    BuildingFrame *frame = s_active;
    uint32_t distance = g_esi, inside = g_ebx;
    int relevant = frame && read_word(g_esp) == 0x00168B0Eu;
    unsigned char value;
    sub_00166E01_original();
    if (relevant) {
        frame->distance_read = read_guest(distance, &frame->distance_squared, sizeof(float));
        if (read_guest(inside, &value, sizeof value)) frame->camera_inside = value;
    }
}

void sub_001681FB(void)
{
    if (s_active) s_active->tier = 1;
    sub_001681FB_original();
}

void sub_00167DA5(void)
{
    if (s_active) s_active->tier = 2;
    sub_00167DA5_original();
}

void sub_00166B3E(void)
{
    if (s_active) s_active->tier = 3;
    sub_00166B3E_original();
}

void sub_00162F75(void)
{
    BuildingFrame *frame = s_active;
    BuildingAllocation *allocation = NULL;
    uint32_t caller, queue_address, quads;
    unsigned char stream_flag;
    int stream = -1;
    if (!frame) { sub_00162F75_original(); return; }
    caller = read_word(g_esp);
    queue_address = g_eax;
    quads = g_edx;
    if (caller == 0x00168070u || caller == 0x00166441u) {
        if (read_guest(g_esp + 4u, &stream_flag, sizeof stream_flag)) {
            stream = stream_flag ? 0 : 1;
            allocation = caller == 0x00168070u ? &frame->mid_alloc[stream] : &frame->upper_alloc[stream];
        } else ++frame->alloc_unknown_stream;
    }
    sub_00162F75_original();
    if (caller != 0x00168070u && caller != 0x00166441u) return;
    frame->alloc_caller = caller;
    frame->alloc_queue = queue_address;
    frame->alloc_stream = stream;
    frame->alloc_quads = quads;
    frame->alloc_result = g_eax;
    if (!g_eax) {
        frame->failed_caller = caller;
        frame->failed_queue = queue_address;
        frame->failed_stream = stream;
        frame->failed_quads = quads;
    }
    if (allocation) {
        ++allocation->calls;
        allocation->quads += quads;
        if (g_eax) {
            ++allocation->successes;
            allocation->successful_quads += quads;
        } else ++allocation->failures;
    }
}
