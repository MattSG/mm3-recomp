/* Guest memory reads that take a 64-bit address, for pointer arithmetic that can
 * leave the 32-bit guest range. */
#ifndef GUEST_READ_H
#define GUEST_READ_H

#include <stdint.h>

/* The word at va, or 0 when va is not a 32-bit guest address or the word is
 * not readable. */
uint32_t xbox_guest_word_at(uint64_t va);

/* A capped step for a clock that only runs while the game is not paused:
 * returns total plus the elapsed time, capped at cap. */
uint64_t recomp_clock_capped_advance(uint64_t total, uint64_t elapsed, uint64_t cap);

#endif
