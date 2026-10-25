/* Helpers for NUL-terminated strings in guest memory. */
#ifndef GUEST_STRINGS_H
#define GUEST_STRINGS_H

#include <stdint.h>

/* Returns 1 when the guest C string at va is exactly s (including its end). */
int xbox_guest_cstring_equals(uint32_t va, const char *s);

/* Writes s as UTF-16 code units at va, with a terminating zero unit. The caller
 * guarantees the space; the string is ASCII. */
void xbox_guest_write_utf16_ascii(uint32_t va, const char *s);

#endif
