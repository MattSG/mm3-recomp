#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform/xbox_winnt.h"
#include "recomp_types.h"
#include "kernel.h"
#include "feature_switch.h"

/* The game's language choice, kept across restarts.
 *
 * MM3 holds its current language in the int at 0x3C5648 (0 English,
 * 1 French, 2 German, 3 Italian, 4 Spanish, 6 Japanese: sub_001DDB23's map of
 * the console language). At start-up, and in every menu's base constructor,
 * sub_001193A6 sets it from the console's language and region; Game Setup
 * writes it through varLanguage and runs the updateLanguage command
 * (sub_00124822), which loads that language's strings. On a console the
 * dashboard keeps the language. Here the console language is always English
 * (kernel_xbox.c), so every restart went back to English.
 *
 * The choice made in Game Setup is kept in UDATA\mm3_language.ini, beside the
 * title's save folder (RECOMP_SAVE_DIR moves it with the saves), and put back
 * after the constructor sets the console default. MM3_REMEMBER_LANGUAGE=0
 * turns this off. */
extern void sub_001193A6_original(void);
extern void sub_00124822_original(void);
extern void sub_000190E0(void);
extern void sub_0022231C(void);

#define LANGUAGE_ADDRESS 0x003C5648u
#define LANGUAGE_MAX 6
#define LANGUAGE_FILE "\\Device\\Harddisk0\\Partition1\\UDATA\\mm3_language.ini"
/* Voices: the sound manager (0x3C5D48) speaks the language named in its
 * +0xB8 string, "English" from its constructor. Only updateLanguage sets it
 * (0x12484B: the name from the table at 0x407C94 via sub_000190E0, then the
 * setter sub_0022231C, which also points the "LANG/" alias at that folder),
 * so a restored language would show its text with English voices. */
#define SOUND_MANAGER 0x003C5D48u
#define LANGUAGE_NAMES 0x00407C94u
#define VOICE_LANGUAGES 4           /* French, German, Italian, Spanish folders */

static int remember_language(void)
{
    static int state;
    return feature_switch_on("MM3_REMEMBER_LANGUAGE", 1, 0, &state);
}

static FILE *language_file(int write)
{
    xbox_host_char path[MAX_PATH];
    if (!xbox_translate_path(LANGUAGE_FILE, path, MAX_PATH)) return NULL;
#if defined(_WIN32)
    return _wfopen(path, write ? L"w" : L"r");
#else
    return fopen(path, write ? "w" : "r");
#endif
}

static int s_saved = -2;            /* -2 unread, -1 none saved */

static int language_saved(void)
{
    char line[64];
    FILE *file;
    if (s_saved != -2) return s_saved;
    s_saved = -1;
    if (!(file = language_file(0))) return s_saved;
    while (fgets(line, sizeof line, file))
        if (!strncmp(line, "language=", 9)) {
            char *end;
            long value = strtol(line + 9, &end, 10);
            if (end != line + 9 && value >= 0 && value <= LANGUAGE_MAX) s_saved = (int)value;
        }
    fclose(file);
    return s_saved;
}

static void language_save(int language)
{
    FILE *file;
    if (language < 0 || language > LANGUAGE_MAX || language_saved() == language) return;
    if (!(file = language_file(1))) {
        fprintf(stderr, "[MM3_LANGUAGE] could not save language %d\n", language);
        return;
    }
    fprintf(file, "; Midtown Madness 3: the language chosen in Options > Game Setup\nlanguage=%d\n", language);
    fclose(file);
    s_saved = language;
    fprintf(stderr, "[MM3_LANGUAGE] saved language %d\n", language);
}

/* Which sound manager speaks which language: a new manager speaks English. */
static uint32_t s_voice_manager;
static int s_voice_language;

static void voice_language_apply(int language)
{
    uint32_t manager = MEM32(SOUND_MANAGER), name;
    uint32_t saved[] = {g_eax, g_ecx, g_edx, g_ebp, g_seh_ebp}, stack_top = g_esp;
    if (manager < 0x10000u || manager >= 0x08000000u) return;
    if (manager != s_voice_manager) { s_voice_manager = manager; s_voice_language = 0; }
    if (language == s_voice_language || language < 0 || language > VOICE_LANGUAGES) return;
    if (MEM32(LANGUAGE_NAMES + 8u) < 5u) return;            /* the name table is not built */
    g_eax = (uint32_t)language;
    PUSH32(g_esp, 0);                                       /* host-owned call, no return address */
    sub_000190E0();
    name = g_eax;
    if (name >= 0x10000u && name < 0x08000000u) {
        PUSH32(g_esp, name);
        PUSH32(g_esp, 0);
        g_ecx = manager;
        sub_0022231C();
        s_voice_language = language;
        fprintf(stderr, "[MM3_LANGUAGE] voices follow language %d\n", language);
    }
    g_eax = saved[0]; g_ecx = saved[1]; g_edx = saved[2]; g_ebp = saved[3]; g_seh_ebp = saved[4];
    g_esp = stack_top;
}

/* Start-up and every menu: the player's saved choice, voices included. */
void sub_001193A6(void)
{
    int saved;
    sub_001193A6_original();
    if (!remember_language()) return;
    saved = language_saved();
    if (saved >= 0 && (int)MEM32(LANGUAGE_ADDRESS) != saved) {
        static int reported;
        if (!reported) {
            reported = 1;
            fprintf(stderr, "[MM3_LANGUAGE] restored language %d (console default %d)\n",
                    saved, (int)MEM32(LANGUAGE_ADDRESS));
        }
        MEM32(LANGUAGE_ADDRESS) = (uint32_t)saved;
    }
    voice_language_apply((int)MEM32(LANGUAGE_ADDRESS));
}

/* Game Setup > Language applied (the game sets the voices itself). */
void sub_00124822(void)
{
    sub_00124822_original();
    if (!remember_language()) return;
    s_voice_manager = MEM32(SOUND_MANAGER);
    s_voice_language = (int)MEM32(LANGUAGE_ADDRESS);
    language_save((int)MEM32(LANGUAGE_ADDRESS));
}
