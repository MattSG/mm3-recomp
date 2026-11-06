#include "mm3_tick_clock.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

int main(void)
{
    const int fps[] = {15, 20, 30, 60, 75, 144, 240, 1000};
    unsigned k;
    for (k = 0; k < sizeof fps / sizeof fps[0]; ++k) {
        Mm3TickClock c;
        int frame, ticks = 0;
        mm3_tick_clock_reset(&c, 0, 30, fps[k]);
        for (frame = 0; frame < fps[k] * 10; ++frame) {
            double now = (double)frame / fps[k];
            while (mm3_tick_clock_take(&c, now)) ++ticks;
        }
        /* Last host sample excludes the final 1/FPS seconds. */
        assert(ticks >= 299 && ticks <= 300);
        assert(c.dropped_seconds == 0);
    }
    {
        Mm3TickClock c;
        int n = 0;
        mm3_tick_clock_reset(&c, 10, 30, 60);
        assert(!mm3_tick_clock_take(&c, NAN));
        assert(!mm3_tick_clock_take(&c, 9));
        while (mm3_tick_clock_take(&c, 11)) ++n;
        assert(n >= 4 && n <= 5 && c.dropped_seconds > .8);
        assert(mm3_tick_clock_present(&c, 11));
        assert(!mm3_tick_clock_present(&c, 11));
        mm3_tick_clock_reset(&c, 12, 75, 0);
        assert(c.ticks == 0 && c.dropped_seconds == 0);
        assert(mm3_tick_clock_take(&c, 12));
        assert(mm3_tick_clock_present(&c, 12));
        assert(mm3_tick_clock_present(&c, 12));
    }
    {
        Mm3TickClock c;
        mm3_tick_clock_reset(&c, 0, 30, 100);
        assert(mm3_tick_clock_present(&c, .001));
        assert(fabs(c.next_present - .010) < 1e-9);
        assert(mm3_tick_clock_present(&c, .011));
        assert(fabs(c.next_present - .020) < 1e-9);
        assert(mm3_tick_clock_present(&c, .055));
        assert(fabs(c.next_present - .060) < 1e-9);
        assert(!mm3_tick_clock_present(&c, .055));
    }
    assert(mm3_tick_rate(NULL) == 30 && mm3_tick_rate("garbage") == 30);
    assert(mm3_tick_rate("60oops") == 30 && mm3_tick_rate("0") == 30);
    assert(mm3_tick_rate("75") == 75 && mm3_tick_rate("120") == 120);
    puts("MM3 tick clock contracts passed");
    return 0;
}
