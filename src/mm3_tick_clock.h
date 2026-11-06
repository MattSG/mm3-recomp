#ifndef MM3_TICK_CLOCK_H
#define MM3_TICK_CLOCK_H

#include <stdint.h>

/* Host deadlines only. Guest elapsed time advances when a tick is taken. */
typedef struct {
    double next_tick, next_present, step, present_step, dropped_seconds;
    uint64_t ticks, presents;
} Mm3TickClock;

int mm3_tick_rate(const char *value);
void mm3_tick_clock_reset(Mm3TickClock *clock, double now, int hz, int fps);
int mm3_tick_clock_take(Mm3TickClock *clock, double now);
int mm3_tick_clock_present(Mm3TickClock *clock, double now);

#endif
