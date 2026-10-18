/* Compile with the project recomp_types.h and mm3_unresolved_stubs.c.
 * Child processes prove unresolved paths stop before a fallthrough sentinel. */
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>
#include "recomp_types.h"

RECOMP_TLS uint32_t g_esp;
ptrdiff_t g_xbox_mem_offset;
int mm3_stub_hit(uint32_t va);
#define CHECK(x) do { if (!(x)) { fprintf(stdout,"%s:%d: failed: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)

static int jump(int cond)
{
    if (cond) RECOMP_DEAD_BRANCH(0x0009398Fu);
    return 99;
}

int main(int argc, char **argv)
{
    if (argc == 2) {
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
        CHECK(freopen("mm3_dead_branch_stderr.txt", "w", stderr));
        g_esp = 0x10000u; /* Unmapped: diagnostics must not dereference it. */
        if (!strcmp(argv[1], "stub")) mm3_stub_hit(0x123456u);
        else return jump(1);
        return 99;
    }
    CHECK(jump(0) == 99);
    for (int mode = 0; mode < 2; ++mode) {
        const char *argument = mode ? "stub" : "branch";
        intptr_t status = _spawnl(_P_WAIT, argv[0], argv[0], argument, NULL);
        CHECK(status == 3); /* CRT abort, neither fallthrough nor access violation. */
        FILE *log = fopen("mm3_dead_branch_stderr.txt", "r");
        char text[1024];
        CHECK(log);
        size_t length = fread(text, 1, sizeof text - 1, log);
        text[length] = 0;
        fclose(log);
        CHECK(strstr(text, mode ? "[STUB] unresolved 0x00123456" : "[DEAD-BRANCH] taken jump to 0x0009398F"));
        CHECK(strstr(text, "stopping"));
    }
    remove("mm3_dead_branch_stderr.txt");
    puts("PASS: untaken branch falls through; residual branch and unresolved stub abort safely before fake execution");
    return 0;
}
