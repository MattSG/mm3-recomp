/* The language chosen in Game Setup survives a restart, voices included
 * (src/mm3_language.c). Each mode is a separate process, the way a restart
 * is: mm3_language_regression.py runs them in order over one folder. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "platform/xbox_winnt.h"
#include "recomp_types.h"
#include "kernel.h"

ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi, g_ebp, g_seh_ebp;
static unsigned char memory[0x410000];     /* through the name table at 0x407C94 */
static unsigned applied, voiced;
static uint32_t voice_manager, voice_name;
static char folder[1024], path[1100];
#define STACK 0x30000u
#define MANAGER 0x60000u

void sub_001193A6(void);
void sub_00124822(void);
void sub_001193A6_original(void) { MEM32(0x3C5648) = 0; }      /* console default: English */
void sub_00124822_original(void) { ++applied; }
/* The language name table: eax = language -> eax = its std::string. */
void sub_000190E0(void) { assert(g_esp == STACK - 4); g_eax = 0x50000u + g_eax * 0x20u; g_esp += 4; }
/* The sound manager's setter: thiscall, one argument, ret 4. */
void sub_0022231C(void)
{
    assert(g_esp == STACK - 8);
    ++voiced; voice_manager = g_ecx; voice_name = MEM32(g_esp + 4);
    g_ecx = g_edx = 0xDEAD; g_esp += 8;
}

/* The kernel puts UDATA under the save root; here, under the test folder. */
BOOL xbox_translate_path(const char *xbox_path, xbox_host_char *out, DWORD size)
{
    assert(!strcmp(xbox_path, "\\Device\\Harddisk0\\Partition1\\UDATA\\mm3_language.ini"));
#if defined(_WIN32)
    return swprintf(out, size, L"%hs", path) > 0;
#else
    return snprintf(out, size, "%s", path) > 0;
#endif
}

static int file_language(void)
{
    char line[64];
    int language = -1;
    FILE *file = fopen(path, "r");
    if (!file) return -1;
    while (fgets(line, sizeof line, file))
        if (!strncmp(line, "language=", 9)) language = atoi(line + 9);
    fclose(file);
    return language;
}

static void menu(void)
{
    g_esp = STACK; g_ecx = 0x1234; g_edx = 0x5678; g_ebp = 0x9ABC;
    sub_001193A6();
    assert(g_esp == STACK && g_ecx == 0x1234 && g_edx == 0x5678 && g_ebp == 0x9ABC);
}

static void pick(int language)                 /* Game Setup > Language */
{
    MEM32(0x3C5648) = (uint32_t)language;
    sub_00124822();
}

int main(int argc, char **argv)
{
    assert(argc == 3);
    g_xbox_mem_offset = (ptrdiff_t)memory;
    snprintf(folder, sizeof folder, "%s", argv[2]);
    snprintf(path, sizeof path, "%s/mm3_language.ini", folder);
    if (!strcmp(argv[1], "first")) {
        menu(); assert(MEM32(0x3C5648) == 0);                  /* nothing saved: the game's default */
        pick(2); assert(applied == 1 && file_language() == 2);  /* German picked in Game Setup */
        menu(); assert(MEM32(0x3C5648) == 2);
        pick(9); assert(file_language() == 2);                 /* out of range: not saved */
        pick(0); assert(file_language() == 0);
        menu(); assert(MEM32(0x3C5648) == 0);                  /* back to English, kept */
        pick(4); assert(file_language() == 4);                 /* Spanish, for the restart */
    } else if (!strcmp(argv[1], "restart")) {
        /* Spanish was saved: restored at start-up, and the sound manager is
         * pointed at the Spanish folder once, as Game Setup would; again for a
         * new manager; not without a manager or name table, not for English
         * or a language with no voices on the disc; never twice after Game
         * Setup did it. */
        MEM32(0x407C94 + 8) = 7; MEM32(0x3C5D48) = MANAGER;
        menu(); assert(MEM32(0x3C5648) == 4);
        assert(voiced == 1 && voice_manager == MANAGER && voice_name == 0x50080u);
        menu(); menu(); assert(voiced == 1);
        pick(4); MEM32(0x3C5D48) = MANAGER;
        menu(); assert(voiced == 1);                           /* Game Setup already set it */
        MEM32(0x3C5D48) = 0; menu(); assert(voiced == 1);      /* no manager yet */
        MEM32(0x3C5D48) = MANAGER + 0x200; MEM32(0x407C94 + 8) = 0;
        menu(); assert(voiced == 1);                           /* no name table yet */
        MEM32(0x407C94 + 8) = 7; menu(); assert(voiced == 2 && voice_name == 0x50080u);
        pick(0); MEM32(0x3C5D48) = MANAGER + 0x300;
        menu(); assert(voiced == 2);                           /* English: the manager's own */
        pick(6); MEM32(0x3C5D48) = MANAGER + 0x400;
        menu(); assert(voiced == 2);                           /* Japanese: no voices on the disc */
        assert(file_language() == 6);
    } else if (!strcmp(argv[1], "garbage")) {                  /* the driver wrote language=banana */
        MEM32(0x407C94 + 8) = 7; MEM32(0x3C5D48) = MANAGER;
        menu(); assert(MEM32(0x3C5648) == 0 && voiced == 0);
    } else if (!strcmp(argv[1], "disabled")) {                 /* MM3_REMEMBER_LANGUAGE=0, 2 saved */
        MEM32(0x407C94 + 8) = 7; MEM32(0x3C5D48) = MANAGER;
        menu(); assert(MEM32(0x3C5648) == 0 && voiced == 0);
        pick(3); assert(applied == 1 && file_language() == 2);
    } else {
        assert(!"unknown mode");
    }
    printf("PASS language %s\n", argv[1]);
    return 0;
}
