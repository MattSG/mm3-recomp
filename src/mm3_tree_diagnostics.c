#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mm3_recomp_types.h"

extern void sub_0016AEDB_original(void);
extern void sub_000E1F49_original(void);
extern void sub_001D137B_original(void);

#ifndef MM3_TREE_CLOCK
#define MM3_TREE_CLOCK GetTickCount64
#endif
#ifndef MM3_TREE_LOG
#define MM3_TREE_LOG(...) fprintf(stderr, __VA_ARGS__)
#endif
#ifndef MM3_TREE_FLUSH
#define MM3_TREE_FLUSH() fflush(stderr)
#endif

/* Keep the original geometry and effects. Trees use the native solid contact
 * response instead of the retail high-speed upward-hop response. */
#define TREE_LINES_PER_SECOND 32u
#define TREE_NAME_SIZE 48u
static volatile LONG s_tree_enabled = -1;
static RECOMP_TLS uint64_t s_tree_second;
static RECOMP_TLS unsigned s_tree_lines, s_tree_suppressed;
static RECOMP_TLS unsigned s_tree_render_depth;
static RECOMP_TLS uint64_t s_tree_contact_second;
static RECOMP_TLS unsigned s_tree_contact_lines;
static RECOMP_TLS uint32_t s_tree_contact_car, s_tree_contact_body, s_tree_contact_before_count;
static RECOMP_TLS uint32_t s_tree_contact_record, s_tree_contact_metadata;
static RECOMP_TLS uint16_t s_tree_contact_flags;
static RECOMP_TLS unsigned s_tree_contact_trees;
static RECOMP_TLS int s_tree_contact_log;
static RECOMP_TLS float s_tree_contact_velocity[3], s_tree_contact_position[3];

static int tree_enabled(void)
{
    if (s_tree_enabled < 0) {
        const char *value = getenv("MM3_TREE_TRACE");
        LONG enabled = value && *value && strcmp(value, "0") != 0;
        if (enabled && strcmp(value, "contacts") == 0) enabled = 2;
        if (InterlockedCompareExchange(&s_tree_enabled, enabled, -1) == -1 && enabled) {
            MM3_TREE_LOG("[TREE_TRACE] enabled mode=%s object_render=%s collision_events=E1F49 solid_tree_contacts=1D137B lines_per_second=%u metadata=retail_LocalObject\n",
                enabled == 2 ? "contacts" : "full", enabled == 2 ? "off" : "16AEDB", TREE_LINES_PER_SECOND);
            MM3_TREE_FLUSH();
        }
    }
    return (int)s_tree_enabled;
}

static int tree_read(uint32_t address, void *out, size_t size)
{
    SIZE_T copied = 0;
    uintptr_t host;
    if (address < 0x10000u || size > UINT32_MAX || address > UINT32_MAX - size) return 0;
    host = (uintptr_t)address + (uintptr_t)g_xbox_mem_offset;
    if (host > UINTPTR_MAX - size) return 0;
    return ReadProcessMemory(GetCurrentProcess(), (const void *)host, out, size, &copied) && copied == size;
}

static uint32_t tree_word(uint32_t address)
{
    uint32_t value = 0;
    tree_read(address, &value, sizeof value);
    return value;
}

static int tree_name(uint32_t address, char out[TREE_NAME_SIZE])
{
    unsigned hops, length = 0;
    int emit = 1;
    uint32_t node[3];
    out[0] = 0;
    if (!tree_read(address, node, sizeof node) || ((node[0] & 15u) << 4 | (node[1] & 15u))) return 0;
    /* Retail sub_0017069B decodes an interned trie terminal. Each node
     * carries one character in two low nibbles and an aligned parent in
     * word0. Emit an ancestor only when its word2 backlink matches the
     * child, then reverse the bytes (including the first terminal NUL). */
    for (hops = 0; hops < 160; ++hops) {
        uint32_t parent = node[0] & ~15u;
        if (emit) {
            unsigned value = (node[0] & 15u) << 4 | (node[1] & 15u);
            if (length == TREE_NAME_SIZE - 1 || (length && (value < 33 || value > 126))) break;
            out[length++] = (char)value;
        }
        if (!parent) {
            unsigned i;
            for (i = 0; i < length / 2; ++i) {
                char value = out[i]; out[i] = out[length - i - 1]; out[length - i - 1] = value;
            }
            return length > 0;
        }
        if (!tree_read(parent, node, sizeof node)) break;
        emit = (((node[0] & 15u) << 4 | (node[1] & 15u)) != 0) && node[2] == address;
        address = parent;
    }
    out[0] = 0;
    return 0;
}

static int tree_budget(void)
{
    uint64_t second = MM3_TREE_CLOCK() / 1000;
    if (second != s_tree_second) {
        if (s_tree_suppressed)
            MM3_TREE_LOG("[TREE_TRACE] suppressed=%u previous_second=%llu\n", s_tree_suppressed, (unsigned long long)s_tree_second);
        s_tree_second = second;
        s_tree_lines = s_tree_suppressed = 0;
    }
    if (s_tree_lines == TREE_LINES_PER_SECOND) { ++s_tree_suppressed; return 0; }
    ++s_tree_lines;
    return 1;
}

static int local_object_tree(const unsigned char local[52], char sound[TREE_NAME_SIZE])
{
    uint32_t pointer, events;
    unsigned i;
    memcpy(&pointer, local + 0x10, sizeof pointer);
    tree_name(pointer, sound);
    if (strcmp(sound, "tree") == 0) return 1;
    memcpy(&events, local + 0x2C, sizeof events);
    /* Paris identifies tree impacts in an event sound instead of the main
     * object sound. Bound the scan independently of any malformed count. */
    for (i = 0; i < local[0] && i < 64; ++i) {
        char event_sound[TREE_NAME_SIZE];
        uint64_t address = (uint64_t)events + (uint64_t)i * 86u + 0x27u;
        if (address > UINT32_MAX) break;
        pointer = tree_word((uint32_t)address);
        if (tree_name(pointer, event_sound) && strcmp(event_sound, "tree") == 0) return 1;
    }
    return 0;
}

static int tree_contact_budget(void)
{
    uint64_t second = MM3_TREE_CLOCK() / 1000;
    if (second != s_tree_contact_second) {
        s_tree_contact_second = second;
        s_tree_contact_lines = 0;
    }
    return s_tree_contact_lines++ < TREE_LINES_PER_SECOND;
}

int mm3_tree_use_solid_contact(uint32_t record)
{
    /* Retail static collision producers copy primitive flags+0xC8 and
     * LocalObject metadata+0xA0 into these temporary 0x10C contact records.
     * Above 40km/h, flag 2 makes 1D137B add 10% of speed vertically and exit
     * without registering a solid contact. For trees, let its normal branch
     * register the contact/penetration correction instead. This changes only
     * the hop decision, leaving flags and the final native sound gate intact.
     * Classify only the selected flagged contact; never cache city pointers. */
    unsigned char local[52];
    char sound[TREE_NAME_SIZE];
    uint32_t metadata;
    uint16_t flags;
    if (record > UINT32_MAX - 0x10Cu || !tree_read(record + 0x100, &flags, sizeof flags) || !(flags & 2u)) return 0;
    metadata = tree_word(record + 0x108);
    if (!tree_read(metadata, local, sizeof local) || !local_object_tree(local, sound)) return 0;
    ++s_tree_contact_trees;
    if (!s_tree_contact_log && tree_enabled() && tree_contact_budget()) {
        s_tree_contact_record = record;
        s_tree_contact_metadata = metadata;
        s_tree_contact_flags = flags;
        s_tree_contact_body = tree_word(tree_word(s_tree_contact_car + 0x3C) + 4);
        tree_read(s_tree_contact_body + 0x20, s_tree_contact_velocity, sizeof s_tree_contact_velocity);
        tree_read(s_tree_contact_body + 0xC8, s_tree_contact_position, sizeof s_tree_contact_position);
        s_tree_contact_before_count = tree_word(0x3C0344);
        s_tree_contact_log = 1;
    }
    return 1;
}

void sub_001D137B(void)
{
    s_tree_contact_car = tree_word(g_esp + 4);
    s_tree_contact_trees = 0;
    s_tree_contact_log = 0;
    sub_001D137B_original();
    if (s_tree_contact_log) {
        float velocity[3] = {0}, position[3] = {0};
        uint32_t contacts = tree_word(0x3C0344);
        tree_read(s_tree_contact_body + 0x20, velocity, sizeof velocity);
        tree_read(s_tree_contact_body + 0xC8, position, sizeof position);
        MM3_TREE_LOG("[TREE_SOLID_CONTACT] car=%08X contact=%08X metadata=%08X flags=%04X trees=%u native_hop_bit=2 response=solid original_flags=preserved velocity=(%.3f,%.3f,%.3f)->(%.3f,%.3f,%.3f) position=(%.3f,%.3f,%.3f)->(%.3f,%.3f,%.3f) solid_contacts=%u->%u\n",
            s_tree_contact_car, s_tree_contact_record, s_tree_contact_metadata, s_tree_contact_flags,
            s_tree_contact_trees, s_tree_contact_velocity[0], s_tree_contact_velocity[1], s_tree_contact_velocity[2],
            velocity[0], velocity[1], velocity[2], s_tree_contact_position[0], s_tree_contact_position[1], s_tree_contact_position[2],
            position[0], position[1], position[2], s_tree_contact_before_count, contacts);
        MM3_TREE_FLUSH();
    }
}

void sub_0016AEDB(void)
{
    unsigned char instance[0xF0], local[52], owner_flags[3] = {0};
    uint32_t stack, caller, object, owner = 0, metadata = 0, flags = 0;
    uint32_t pool_before[2] = {0, 0}, pool_after[2] = {0, 0};
    uint32_t contact_geometry = 0;
    uint16_t pieces[4] = {0};
    float bounds[6] = {0}, center[3] = {0};
    float transform_position[3] = {0};
    char sound[TREE_NAME_SIZE] = {0};
    int valid, local_valid = 0, is_tree = 0;
    unsigned i;
    if (tree_enabled() != 1 || !tree_budget()) { sub_0016AEDB_original(); return; }
    stack = g_esp;
    caller = tree_word(stack);
    object = tree_word(stack + 4);
    tree_read(tree_word(stack + 24) + 0x24, transform_position, sizeof transform_position);
    valid = tree_read(object, instance, sizeof instance);
    if (valid) {
        memcpy(&owner, instance, 4);
        memcpy(&metadata, instance + 0xA0, 4);
        memcpy(&flags, instance + 0xC8, 4);
        memcpy(&contact_geometry, instance + 0x40, 4);
        memcpy(pieces, instance + 0xA4, 4);
        memcpy(pieces + 2, instance + 0xEC, 4);
        tree_read(owner + 0xA0, owner_flags, sizeof owner_flags);
        memcpy(bounds, instance + 0x20, sizeof bounds);
        for (i = 0; i < 3; ++i) center[i] = (bounds[i] + bounds[i + 3]) * 0.5f;
        pool_before[0] = tree_word(owner + 0x112C);
        pool_before[1] = tree_word(owner + 0x13E38);
        local_valid = tree_read(metadata, local, sizeof local);
        if (local_valid) is_tree = local_object_tree(local, sound);
    }
    ++s_tree_render_depth;
    sub_0016AEDB_original();
    --s_tree_render_depth;
    if (valid) {
        pool_after[0] = tree_word(owner + 0x112C);
        pool_after[1] = tree_word(owner + 0x13E38);
    }
    {
        MM3_TREE_LOG("[TREE_OBJECT_RENDER] caller=%08X object=%08X read=%d tree=%d local=%08X local_read=%d sound=%s flags=%08X owner_flags=%u,%u,%u geometry40=%08X piecesA4=%u,%u piecesEC=%u,%u transform_position=(%.3f,%.3f,%.3f) local_bounds_center=(%.3f,%.3f,%.3f) local_bounds=(%.3f,%.3f,%.3f,%.3f,%.3f,%.3f) local_bytes=%u,%u,%u,%u render_pool0=%u->%u render_pool1=%u->%u result=%02X stack=%08X->%08X\n",
            caller, object, valid, is_tree, metadata, local_valid, sound[0] ? sound : "none", flags,
            owner_flags[0], owner_flags[1], owner_flags[2], contact_geometry,
            pieces[0], pieces[1], pieces[2], pieces[3],
            transform_position[0], transform_position[1], transform_position[2],
            center[0], center[1], center[2], bounds[0], bounds[1], bounds[2], bounds[3], bounds[4], bounds[5],
            local_valid ? local[0] : 0, local_valid ? local[1] : 0,
            local_valid ? local[2] : 0, local_valid ? local[3] : 0,
            pool_before[0], pool_after[0], pool_before[1], pool_after[1], g_eax & 255u, stack, g_esp);
        MM3_TREE_FLUSH();
    }
}

void sub_000E1F49(void)
{
    unsigned char event[86];
    uint32_t stack, caller, event_address, flags = 0, particle_ptr = 0, sound_ptr = 0;
    uint32_t velocity_ptr, output_ptr, output = 0;
    float velocity[3] = {0}, threshold = 0;
    char particle[TREE_NAME_SIZE] = {0}, sound[TREE_NAME_SIZE] = {0};
    int valid, velocity_valid;
    unsigned render_depth;
    if (!tree_enabled()) { sub_000E1F49_original(); return; }
    /* Reject render-only events before the expensive metadata/name reads. */
    flags = tree_word(g_ecx + 0xD);
    if ((flags & 3u) != 1u || !tree_budget()) {
        sub_000E1F49_original();
        return;
    }
    stack = g_esp;
    caller = tree_word(stack);
    event_address = g_ecx;
    render_depth = s_tree_render_depth;
    valid = tree_read(event_address, event, sizeof event);
    if (valid) {
        memcpy(&flags, event + 0xD, 4);
        memcpy(&particle_ptr, event + 9, 4);
        memcpy(&sound_ptr, event + 0x27, 4);
        memcpy(&threshold, event + 0x1A, 4);
        tree_name(particle_ptr, particle);
        tree_name(sound_ptr, sound);
    }
    velocity_ptr = tree_word(stack + 24);
    velocity_valid = tree_read(velocity_ptr, velocity, sizeof velocity);
    output_ptr = tree_word(stack + 36);
    sub_000E1F49_original();
    if (output_ptr) output = tree_word(output_ptr);
    /* Render-time leaf effects are too frequent to be useful. Only genuine
     * collision-kind events reach this report. */
    {
        double speed2 = (double)velocity[0] * velocity[0] + (double)velocity[1] * velocity[1] + (double)velocity[2] * velocity[2];
        MM3_TREE_LOG("[TREE_COLLISION_EVENT] caller=%08X event=%08X read=%d during_render=%u kind=%u flags=%08X particle=%s sound=%s velocity_read=%d velocity=(%.3f,%.3f,%.3f) speed_squared=%.3f threshold_squared=%.3f result=%02X output=%08X stack=%08X->%08X\n",
            caller, event_address, valid, render_depth, flags & 3u, flags,
            particle[0] ? particle : "none", sound[0] ? sound : "none", velocity_valid,
            velocity[0], velocity[1], velocity[2], speed2, threshold, g_eax & 255u, output, stack, g_esp);
        MM3_TREE_FLUSH();
    }
}
