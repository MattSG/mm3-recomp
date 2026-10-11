#include "recomp_types.h"

extern void sub_001DDFC5_original(void);

/* PAL catalog lookup: ECX is the catalog, [ESP+4] is the vehicle ID and
 * EAX is the matching entry (or NULL); the original function returns with
 * RET 4. Old profiles may still select a retired custom car. Resolve only
 * those IDs to their original donor, without restoring catalog entries or
 * changing profile data. Every other ID keeps the original lookup behavior. */
void sub_001DDFC5(void)
{
    uint32_t argument = g_esp + 4u;
    uint32_t requested = MEM32(argument);
    uint32_t donor;

    switch (requested) {
    case 9002u: donor = 8u; break;   /* Mustang GT -> Mustang Fastback. */
    case 9003u: donor = 11u; break;  /* Viper Race -> Dodge Viper. */
    case 9007u:                    /* Chiron / Bolide -> Koenigsegg. */
    case 9008u: donor = 15u; break;
    default:
        sub_001DDFC5_original();
        return;
    }

    MEM32(argument) = donor;
    sub_001DDFC5_original();
    /* RET 4 has consumed the argument. Restore its bytes as well so the
     * compatibility lookup has no persistent stack or profile mutation. */
    MEM32(argument) = requested;
}
