/* RECOMP_DEAD_BRANCH: the real header and the real definition, compiled
 * together. Each reached dead branch must log once per target and fall
 * through. Build (vcvars64 first, from a scratch dir):
 *   cl /nologo /W3 /I <scratch>\inc /I src tests\mm3_dead_branch_contract.c src\mm3_unresolved_stubs.c
 * where <scratch>\inc holds recomp_types.h (= src\mm3_recomp_types.h, as the
 * build copies it) and recomp_cpu.h (= tools\xboxrecomp\include\recomp_cpu.h). */
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include "recomp_types.h"

/* The header only declares these; the game defines them elsewhere. */
RECOMP_TLS uint32_t g_esp;
ptrdiff_t g_xbox_mem_offset;

#define LOG_PATH "mm3_dead_branch_stderr.txt"

/* Not assert: the calls must run in release builds too. */
#define CHECK(x) do { if (!(x)) { printf("%s:%d: failed: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)

/* Shaped like a generated function: the translator's marker, with the
 * condition in front. Returns 1 when execution falls through. */
static int jump_a(int cond)
{
    if (cond) RECOMP_DEAD_BRANCH(0x0009398Fu); /* goto loc_0009398F - label not in function */
    return 1;
}

static int jump_b(int cond)
{
    if (cond) RECOMP_DEAD_BRANCH(0x000D7B7Eu); /* goto loc_000D7B7E - label not in function */
    return 1;
}

int main(void)
{
    static char text[4096];
    size_t n;
    int lines = 0, a = 0, b = 0, fell = 0, i;
    FILE *log;

    CHECK(freopen(LOG_PATH, "w", stderr));
    g_esp = 0x00100000u;
    CHECK(jump_a(0) == 1);       /* condition false: no report */
    CHECK(jump_a(1) == 1);       /* first hit for 0x0009398F: logged */
    CHECK(jump_a(1) == 1);       /* repeat: silent, still falls through */
    CHECK(jump_b(1) == 1);       /* first hit for 0x000D7B7E: logged */
    fell = jump_a(1) & jump_b(1);
    CHECK(fell == 1);
    CHECK(g_esp == 0x00100000u); /* the hook does not touch guest state */
    CHECK(fflush(stderr) == 0);
    freopen("NUL", "w", stderr);

    log = fopen(LOG_PATH, "r");
    CHECK(log != NULL);
    n = fread(text, 1, sizeof(text) - 1, log);
    text[n] = '\0';
    fclose(log);
    for (i = 0; text[i]; i++) {
        if (strncmp(text + i, "[DEAD-BRANCH]", 13) == 0) lines++;
        if (strncmp(text + i, "[DEAD-BRANCH] taken jump to 0x0009398F", 38) == 0) a++;
        if (strncmp(text + i, "[DEAD-BRANCH] taken jump to 0x000D7B7E", 38) == 0) b++;
    }
    printf("dead-branch log lines: %d (0x0009398F: %d, 0x000D7B7E: %d)\n", lines, a, b);
    CHECK(lines == 2);
    CHECK(a == 1);
    CHECK(b == 1);
    printf("ok\n");
    return 0;
}
