#include "guest_strings.h"
#include "xbox_memory_layout.h"
#include <string.h>

static volatile uint8_t *guest_bytes(uint32_t va)
{
    return (volatile uint8_t *)((uintptr_t)va + (uintptr_t)xbox_GetMemoryOffset());
}

int xbox_guest_cstring_equals(uint32_t va, const char *s)
{
    size_t j, length = strlen(s);
    for (j = 0; j <= length; ++j)
        if (guest_bytes(va + (uint32_t)j)[0] != (unsigned char)s[j])
            return 0;
    return 1;
}

void xbox_guest_write_utf16_ascii(uint32_t va, const char *s)
{
    size_t i, length = strlen(s);
    volatile uint16_t *units = (volatile uint16_t *)guest_bytes(va);
    for (i = 0; i <= length; ++i)
        units[i] = (unsigned char)s[i];
}
