#ifndef MM3_RENDER_COMMANDS_H
#define MM3_RENDER_COMMANDS_H
#include "nv2a_d3d11.h"
/* MM3 extended-distance fog, ordered in its native NO_OPERATION stream. */
#define MM3_FOG_HEADER 0x402C0100u
#define MM3_FOG_TAG 0x58424647u
#define MM3_FOG_WORDS 12u
void mm3_render_commands_init(void);
#endif
