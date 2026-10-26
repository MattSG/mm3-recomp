#include "string_ci.h"

static unsigned ascii_lower(unsigned c)
{
    return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
}

int recomp_strings_equal_ci(const char *a, const char *b)
{
    for (; *a && *b; ++a, ++b)
        if (ascii_lower((unsigned char)*a) != ascii_lower((unsigned char)*b))
            return 0;
    return *a == *b;
}
