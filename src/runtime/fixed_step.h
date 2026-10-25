/* Fixed-step accumulator: turns a variable frame time into whole steps.
 *
 * Each frame's time is added to a clock; whole steps are taken from it and the
 * remainder carries to the next frame. A frame that would take more than
 * max_steps is cut to max_steps, and the rest of the time is dropped, as a
 * hitch. alpha is how far the clock is into the next step, 0..1. */
#ifndef FIXED_STEP_H
#define FIXED_STEP_H

typedef struct {
    float clock;
} FixedStepClock;

/* Returns the whole steps for this frame. dt values that are not positive,
 * or longer than one second, count as one step (a load or a reset timer). */
int fixed_step_take(FixedStepClock *clock, float dt, float step, int max_steps, float *alpha);

#endif
