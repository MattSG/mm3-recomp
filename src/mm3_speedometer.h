#ifndef MM3_SPEEDOMETER_H
#define MM3_SPEEDOMETER_H

#include <stdint.h>

/* Called by the native HUD after its speed getter. The renderer remains in
 * g_esi; the helper restores every guest register before returning. */
int mm3_speedometer_draw(uint32_t node, uint32_t frame, double speed_kmh);

#endif
