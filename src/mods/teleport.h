#ifndef MM3_DEBUG_TELEPORT_H
#define MM3_DEBUG_TELEPORT_H

/* Opt in with MM3_DEBUG_TELEPORT=1 before launching the isolated build. */
void mm3_debug_teleport_init(void);

/* Live player updates plus the title's pause state route arrow controls. */
int mm3_keyboard_driving(void);

#endif
