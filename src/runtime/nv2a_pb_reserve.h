/* Reserve space in a guest NV2A push buffer.
 *
 * The device object holds the write cursor at +0 and the soft limit at +4,
 * both guest addresses. A reservation needs an aligned cursor and room for
 * the whole record. When the room is short, the title's own rollover runs
 * first (it is the title's native code that moves the cursor to a fresh
 * segment), then the check is repeated. */
#ifndef NV2A_PB_RESERVE_H
#define NV2A_PB_RESERVE_H

#include <stdint.h>

/* Runs the title's rollover; it must leave the device cursor and limit updated. */
typedef void (*Nv2aPbRolloverFn)(void *user);

/* Returns the guest address of the reserved bytes and advances the cursor past
 * them, or returns 0 when no room exists even after rollover. */
uint32_t xbox_pb_reserve(uint32_t device, uint32_t bytes, Nv2aPbRolloverFn rollover, void *user);

#endif
