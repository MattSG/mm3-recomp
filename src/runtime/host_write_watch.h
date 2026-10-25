/* Hardware write watchpoint on one 32-bit host word, for diagnostics.
 *
 * The watch is armed on the calling thread and fires on that thread after each
 * write to the word. The callback sees the stored value and the writing
 * instruction's host address; returning 0 disarms the watch. One watch is
 * armed per process at a time. */
#ifndef HOST_WRITE_WATCH_H
#define HOST_WRITE_WATCH_H

#include <stdint.h>

typedef int (*HostWriteWatchFn)(void *user, uint32_t value, uint64_t rip);

/* Returns 1 when the watch is armed. Returns 0 when another watch is armed,
 * when no hardware watchpoint is available, or when the OS refuses it. */
int host_write_watch_arm(volatile uint32_t *word, HostWriteWatchFn fn, void *user);

#endif
