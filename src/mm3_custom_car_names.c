#include <string.h>
#include "recomp_types.h"

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
        uint32_t saved_ecx = g_ecx, saved_edx = g_edx;
        uint32_t saved_ebx = g_ebx, saved_esi = g_esi, saved_edi = g_edi;
        uint32_t saved_ebp = g_ebp, saved_seh = g_seh_ebp;
        uint32_t saved_fs = g_fs_base;
        double fp[8];
        int top = g_fp_top, cmp = g_fp_cmp, df = g_df;
        uint16_t cc = g_fp_cc, control = g_fp_control_word;
        RecompXmm xmm[] = {g_xmm0, g_xmm1, g_xmm2, g_xmm3,
                          g_xmm4, g_xmm5, g_xmm6, g_xmm7};
        uint32_t text = entry_sp - 64u;
        size_t length = strlen(value);

        memcpy(fp, g_fp_stack, sizeof fp);
        /* Reserve the text above the helper's argument/callee frames. */
        g_esp = text;
        for (i = 0; i <= length; ++i)
            MEM16(text + (uint32_t)i * 2u) = (unsigned char)value[i];
        g_eax = output;
        PUSH32(g_esp, text);
        PUSH32(g_esp, 0x0011C1BEu); /* original UTF-16 constructor return site */
        sub_00016148();

        g_eax = output;
        g_ecx = saved_ecx;
        g_edx = saved_edx;
        g_ebx = saved_ebx;
        g_esi = saved_esi;
        g_edi = saved_edi;
        g_ebp = saved_ebp;
        g_seh_ebp = saved_seh;
        g_fs_base = saved_fs;
        memcpy(g_fp_stack, fp, sizeof fp);
        g_fp_top = top; g_fp_cmp = cmp; g_df = df;
        g_fp_cc = cc; g_fp_control_word = control;
        g_xmm0 = xmm[0]; g_xmm1 = xmm[1];
        g_xmm2 = xmm[2]; g_xmm3 = xmm[3];
        g_xmm4 = xmm[4]; g_xmm5 = xmm[5];
        g_xmm6 = xmm[6]; g_xmm7 = xmm[7];
        g_esp = entry_sp + 12u; /* lookup's RET 8 */
    }
}
