#ifndef MM3_X87_COMPAT_H
#define MM3_X87_COMPAT_H
#include <math.h>
#include "recomp_types.h"

/* Read the complete little-endian x87 extended operand. The recomp runtime
 * stores x87 values as double; preserve that precision, not a four-byte read. */
static double mm3_read_fp80(uint32_t address)
{
    uint64_t significand = (uint64_t)MEM32(address) |
                          ((uint64_t)MEM32(address + 4u) << 32);
    uint16_t sign_exponent = MEM16(address + 8u);
    unsigned exponent = sign_exponent & 0x7FFFu;
    double value;
    if (exponent == 0x7FFFu)
        value = significand == UINT64_C(0x8000000000000000) ? INFINITY : NAN;
    else
        value = ldexp((double)significand, (exponent ? (int)exponent : 1) - 16383 - 63);
    return sign_exponent & 0x8000u ? -value : value;
}
#endif
