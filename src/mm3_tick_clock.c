#include "mm3_tick_clock.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

int mm3_tick_rate(const char *value)
{
    char *end;
    long hz;
    if (!value || !*value) return 30;
    hz = strtol(value, &end, 10);
    return end != value && !*end && hz >= 15 && hz <= 120 ? (int)hz : 30;
}

void mm3_tick_clock_reset(Mm3TickClock *c, double now, int hz, int fps)
{
    memset(c, 0, sizeof *c);
    c->step = 1.0 / (double)hz;
    c->present_step = fps > 0 ? 1.0 / (double)fps : 0.0;
    c->next_tick = c->next_present = now;
}

int mm3_tick_clock_take(Mm3TickClock *c, double now)
{
    double late;
    if (!isfinite(now) || now + 1e-9 < c->next_tick) return 0;
    /* Discard suspension/load time, while allowing 133 ms of ordinary catch-up. */
    late = now - c->next_tick;
    if (late > 2.0 / 15.0) {
        double drop = floor((late - 2.0 / 15.0) / c->step) * c->step;
        c->dropped_seconds += drop;
        c->next_tick += drop;
    }
    c->next_tick += c->step;
    ++c->ticks;
    return 1;
}

int mm3_tick_clock_present(Mm3TickClock *c, double now)
{
    if (!isfinite(now) || now + 1e-9 < c->next_present) return 0;
    /* Missed presentations are replaced by one current image, never a burst. */
    if (c->present_step > 0) {
        double missed = floor(fmax(0.0, now - c->next_present) / c->present_step);
        c->next_present += (missed + 1.0) * c->present_step;
    } else {
        c->next_present = now;
    }
    ++c->presents;
    return 1;
}
