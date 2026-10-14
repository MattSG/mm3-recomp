#include "../src/mods/offline.c"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

RECOMP_TLS uint32_t g_eax, g_esp;
static int mod_on = 1, original_calls;
int mm3_mod_on(int mod) { assert(mod == MM3_MOD_OFFLINE); return mod_on; }
void sub_0019531D_original(void) { ++original_calls; g_eax = 0x1234; g_esp += 4; }

int main(int argc, char **argv)
{
    assert(argc == 2);
    mod_on = strcmp(argv[1], "mod-off") != 0;
    const char *mode = mod_on ? argv[1] : "";
    assert(!_putenv_s("RECOMP_NET", mode));
    int bypass = mod_on && (!*mode || !strcmp(mode, "0"));
    for (int i = 0; i < 2; ++i) {
        g_esp = 0x10000; g_eax = 99;
        sub_0019531D();
        assert(g_esp == 0x10004 && g_eax == (bypass ? 1u : 0x1234u));
    }
    assert(original_calls == (bypass ? 0 : 2));
    puts("PASS: offline bypass respects mod and network settings");
    return 0;
}
