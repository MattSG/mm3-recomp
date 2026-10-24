#include <string.h>
#include "recomp_types.h"
#include "guest_state.h"

extern void sub_0011C194_original(void);
extern void sub_00016148(void);

/* The normal catalog enumerator already accepts additional Cars/Tweak/*.txt.
 * Retail compressed locale resources contain no strings for the new IDs.
 * Extend only the variant localization keys; every other lookup retains the
 * original implementation. All installed languages use these vehicle names.
 * The output is constructed by the same guest UTF-16 string helper used by
 * the original lookup's missing-key path, so guest allocation and destruction
 * retain their normal ownership. No host pointers enter guest strings. */
static const struct {
    const char *key;
    const char *value;
} car_names[] = {
    { "SID_CAR_MF_9004", "Panoz" },
    { "SID_CAR_MODEL_9004", "GTR-1" },
    { "SID_CAR_SHORT_9004", "Panoz GTR-1" },
    { "SID_CAR_MF_9005", "McLaren" },
    { "SID_CAR_MODEL_9005", "F1" },
    { "SID_CAR_SHORT_9005", "McLaren F1" },
    { "SID_CAR_MF_9006", "Formula" },
    { "SID_CAR_MODEL_9006", "1" },
    { "SID_CAR_SHORT_9006", "Formula 1" },
    { "SID_CAR_MF_9009", "Ford" },
    { "SID_CAR_MODEL_9009", "GT" },
    { "SID_CAR_SHORT_9009", "Ford GT" },
    { "SID_CAR_MF_9010", "McLaren" },
    { "SID_CAR_MODEL_9010", "Senna" },
    { "SID_CAR_SHORT_9010", "McLaren Senna" },
    { "SID_CAR_MF_9011", "McLaren" },
    { "SID_CAR_MODEL_9011", "Senna GTR" },
    { "SID_CAR_SHORT_9011", "McLaren Senna GTR" },
    { "SID_CAR_MF_9012", "Ferrari" },
    { "SID_CAR_MODEL_9012", "FXX K" },
    { "SID_CAR_SHORT_9012", "Ferrari FXX K" },
    { "SID_CAR_MF_9013", "Koenigsegg" },
    { "SID_CAR_MODEL_9013", "Regera" },
    { "SID_CAR_SHORT_9013", "Koenigsegg Regera" },
    { "SID_CAR_MF_9014", "Lamborghini" },
    { "SID_CAR_MODEL_9014", "Aventador" },
    { "SID_CAR_SHORT_9014", "Lamborghini Aventador" }
};

void sub_0011C194(void)
{
    uint32_t entry_sp = g_esp;
    uint32_t output = MEM32(entry_sp + 4u);
    uint32_t key = MEM32(entry_sp + 8u);
    const char *value = NULL;
    size_t i;

    /* The guest key is NUL-terminated by the existing locale API. Prefix
     * matching bounds every read to the exact custom key, including its NUL. */
    for (i = 0; i < sizeof car_names / sizeof car_names[0]; ++i) {
        size_t j, length = strlen(car_names[i].key);
        for (j = 0; j <= length; ++j)
            if (MEM8(key + (uint32_t)j) != (unsigned char)car_names[i].key[j])
                break;
        if (j > length) {
            value = car_names[i].value;
            break;
        }
    }
    if (!value) {
        sub_0011C194_original();
        return;
    }

    {
        xbox_guest_state guest;
        uint32_t text = entry_sp - 64u;
        size_t length = strlen(value);

        xbox_guest_state_save(&guest);
        /* Reserve the text above the helper's argument/callee frames. */
        g_esp = text;
        for (i = 0; i <= length; ++i)
            MEM16(text + (uint32_t)i * 2u) = (unsigned char)value[i];
        g_eax = output;
        PUSH32(g_esp, text);
        PUSH32(g_esp, 0x0011C1BEu); /* original UTF-16 constructor return site */
        sub_00016148();

        xbox_guest_state_restore(&guest, output);
        g_esp = entry_sp + 12u; /* lookup's RET 8 */
    }
}
