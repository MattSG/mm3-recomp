#ifndef MM3_DEBUG_TELEPORT_H
#define MM3_DEBUG_TELEPORT_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
/* Opt in with MM3_DEBUG_TELEPORT=1 before launching the isolated build. */
void mm3_debug_teleport_init(void);
typedef struct {
    float position[3], heading;
    uint32_t player, car, world;
    int available, pending;
    char result[128];
} Mm3TeleportState;
void mm3_teleport_state(Mm3TeleportState *state);
/* Nonblocking render-thread request; heading radians, NAN keeps heading. */
int mm3_teleport_queue(float x, float z, float heading);
/* Game thread only: revalidate the native repair prerequisites. */
uint32_t mm3_debug_repair_player(void);
#ifdef __cplusplus
}
#endif

/* Live player updates plus the title's pause state route arrow controls. */
int mm3_keyboard_driving(void);

#endif
