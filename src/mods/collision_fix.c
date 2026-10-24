#include "recomp_types.h"
#include "xbox_memory_layout.h"

/* Collision cells (PAL MM3). sub_0015A929 brings a cell into the collision
 * cache, decompressing it into a slot on a miss. The cell header holds the
 * face count (+4), a face-index list (+8), a grid of 16-bit [lo,hi) ranges
 * into that list (+0x10, dims at +0x44/+0x48/+0x4C) and a 16-bit "visited"
 * stamp per face (+0x18). Queries (0x15B1B0, 0x15B4E1) stamp
 * stamps[list[i]] without a bound check.
 *
 * The cell around the Paris race start (key 0x3400) decodes with face
 * indices past its face count -- up to 0xFFA4. Stamping those wrote up to
 * 128 KB past the stamp array, into the collision cache's own rings (the
 * lookup at 0x4EB98 then never returned: the game froze) and into other
 * cells (wild pointers: crashes in 0x15B1B0/0x15B4E1, faces lost: the car
 * fell through the road). Grid ranges are clamped to the list, and each
 * out-of-range entry is replaced, at load, by a valid
 * index from the same grid range; the query skips a face it already stamped
 * this pass, so the duplicate costs nothing.
 * ponytail: why that cell's compressed input differs between runs is not
 * known; the guard holds whatever the decoded indices are. */
extern void sub_0015A929_original(void);

static int ok_range(uint32_t a, uint32_t bytes)
{
    return xbox_GuestRangeInWindow(a, bytes);
}

static void sanitize(uint32_t hdr)
{
    uint32_t faces, list, len, grid, cells = 1, g, r;
    if (!ok_range(hdr, 0x50)) return;
    faces = MEM32(hdr + 4); list = MEM32(hdr + 8);
    len = MEM32(hdr + 0xC); grid = MEM32(hdr + 0x10);
    /* Bound each multiplication before it can wrap to a small cell count. */
    for (g = 0; g < 3; ++g) {
        uint32_t dim = MEM32(hdr + 0x44 + 4 * g);
        if (!dim || dim > 0x10000u / cells) return;
        cells *= dim;
    }
    if (!faces || faces > 0xFFFF || len > 0xFFFF ||
        !ok_range(list, 2 * len) || !ok_range(grid, 4 * cells))
        return;
    for (g = 0; g < cells; g++) {
        uint32_t lo = MEM16(grid + 4 * g), hi = MEM16(grid + 4 * g + 2), keep = 0;
        /* a range past the list would read indices from the data after it */
        if (hi > len) MEM16(grid + 4 * g + 2) = (uint16_t)(hi = len);
        if (lo > hi) MEM16(grid + 4 * g) = (uint16_t)(lo = hi);
        for (r = lo; r < hi; r++)
            if (MEM16(list + 2 * r) < faces) { keep = MEM16(list + 2 * r); break; }
        for (r = lo; r < hi; r++) {
            if (MEM16(list + 2 * r) < faces) keep = MEM16(list + 2 * r);
            else MEM16(list + 2 * r) = (uint16_t)keep;
        }
    }
}

void sub_0015A929(void)
{
    uint32_t cell = g_esi, before = ok_range(cell, 4) ? MEM32(cell) : 0;
    sub_0015A929_original();
    if (ok_range(cell, 4) && (g_eax & 0xFF) && MEM32(cell) != before)
        sanitize(MEM32(cell));                 /* this call decoded the cell */
}
