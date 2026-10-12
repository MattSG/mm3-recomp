#ifndef MM3_MODS_H
#define MM3_MODS_H

/* Every change this project makes to how MM3 plays or looks is a mod: one
 * entry here, switched by its environment variable ("1" on, "0" off, unset:
 * the default). The launcher's Mods tab lists them; every one can be
 * switched off, some at the cost of the game no longer running.
 * The translation corrections (src/mm3_*.c, recomp_manual.c) are one mod. */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *id;
    const char *env;
    const char *name;
    const char *description;
    int default_on;
    int required;   /* the game does not run without it: always on */
} Mm3Mod;

enum { MM3_MOD_WIDESCREEN, MM3_MOD_DRAW_DISTANCE, MM3_MOD_SKY, MM3_MOD_MENU_MOUSE,
       MM3_MOD_DEV_TOOLS, MM3_MOD_TELEPORT, MM3_MOD_OFFLINE, MM3_MOD_COLLISION_FIX,
       MM3_MOD_DISC_CHECK, MM3_MOD_TRANSLATION, MM3_MOD_COUNT };

extern const Mm3Mod mm3_mods[MM3_MOD_COUNT];

/* Whether a mod is on (its variable is read once). */
int mm3_mod_on(int mod);
/* Start the mods that are on; before the graphics and input start. */
void mm3_mods_init(void);
/* Every player update (gameplay only): mods that act during a race. */
void mm3_mods_player_update(void);

#ifdef __cplusplus
}
#endif

#endif
