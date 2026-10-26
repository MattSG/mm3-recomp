#include "fixed_step.h"
#include <math.h>

int fixed_step_take(FixedStepClock *clock, float dt, float step, int max_steps, float *alpha)
{
    int n;
    if (!(dt > 0.0f) || dt > 1.0f)
        dt = step;
    clock->clock += dt;
    n = (int)(clock->clock / step);
    if (n > max_steps) {
        n = max_steps;
        clock->clock = (float)max_steps * step;
    }
    clock->clock -= (float)n * step;
    if (alpha)
        *alpha = clock->clock / step;
    return n;
}

uint64_t recomp_clock_capped_advance(uint64_t total, uint64_t elapsed, uint64_t cap)
{
    return total + (elapsed > cap ? cap : elapsed);
}
