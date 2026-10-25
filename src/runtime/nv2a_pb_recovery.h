/* Opt-in stop recovery for a malformed push-buffer submission.
 *
 * After a submission stops on data that is not a command, this searches the
 * skipped words for a semaphore release (method 0x1D70 with an odd value one
 * to 128 past the last release) and executes the last one found, so a title
 * waiting on that fence is not stuck. It guesses, so it is never a default:
 * register it with nv2a_pb_set_stop_handler only where the title needs it. */
#ifndef NV2A_PB_RECOVERY_H
#define NV2A_PB_RECOVERY_H

#include <stdint.h>

void nv2a_pb_recover_semaphore_stop(uint32_t from, uint32_t to, const char *reason);

#endif
