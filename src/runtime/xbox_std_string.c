#include "xbox_std_string.h"
#include "xbox_memory_layout.h"

/* Layout: +4 is the inline buffer (or, when the capacity is 16 or more, a
 * pointer to the heap buffer), +20 the size, +24 the capacity. */
size_t xbox_read_msvc_std_string(uint32_t string_va, char *out, size_t cap)
{
    uint32_t inline_or_ptr, size, capacity, text;
    size_t n;
    if (!out || !cap)
        return 0;
    out[0] = 0;
    if (!xbox_GuestRead(string_va + 4u, &inline_or_ptr, 4) ||
        !xbox_GuestRead(string_va + 20u, &size, 4) ||
        !xbox_GuestRead(string_va + 24u, &capacity, 4))
        return 0;
    text = capacity >= 16u ? inline_or_ptr : string_va + 4u;
    if (size >= cap)
        size = (uint32_t)cap - 1u;
    if (size && !xbox_GuestRangeInWindow(text, size))
        return 0;
    if (size && !xbox_GuestRead(text, out, size))
        return 0;
    n = size;
    out[n] = 0;
    return n;
}
