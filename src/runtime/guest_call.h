/* Calls a guest function several times as one call would.
 *
 * The guest stack's return address and nargs arguments are put back before
 * each call, and the registers the caller keeps (eax, ecx, edx, ebx, esi, edi,
 * ebp) are restored, so each repetition sees what the first did. ret_pop is
 * the number of bytes the callee pops, which the caller's ESP then advances by. */
#ifndef GUEST_CALL_H
#define GUEST_CALL_H

#include <stdint.h>

void xbox_guest_call_repeated(void (*fn)(void), int n, int nargs, uint32_t ret_pop);

#endif
