#ifndef MM3_DEBUG_TELEPORT_H
#define MM3_DEBUG_TELEPORT_H

#ifdef __cplusplus
extern "C" {
#endif
/* Opt in with MM3_DEBUG_TELEPORT=1 before launching the isolated build. */
void mm3_debug_teleport_init(void);
typedef struct {
    float position[3], heading;
    int available, pending;
    char result[128];
} Mm3TeleportState;
void mm3_teleport_state(Mm3TeleportState *state);
/* Nonblocking render-thread request; heading radians, NAN keeps heading. */
int mm3_teleport_queue(float x, float z, float heading);
#ifdef __cplusplus
}
#endif

#endif
