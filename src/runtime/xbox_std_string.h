/* Reads an MSVC std::string (the x86 layout: a 16-byte inline buffer, a
 * pointer when the capacity is larger) out of guest memory. */
#ifndef XBOX_STD_STRING_H
#define XBOX_STD_STRING_H

#include <stddef.h>
#include <stdint.h>

/* Copies the string into out, at most cap - 1 bytes, and NUL-terminates it.
 * Returns the number of bytes copied; 0 when the object is not in guest RAM. */
size_t xbox_read_msvc_std_string(uint32_t string_va, char *out, size_t cap);

#endif
