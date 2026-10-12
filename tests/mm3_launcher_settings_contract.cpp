#include "../src/mm3_launcher.cpp"
extern "C" {
#include "nv2a_d3d11.h"
}
#include <assert.h>
extern "C" {
void xbox_InputSetMenuProbe(int (*)(void)) { assert(false); }
void xbox_InputSetMenuPointer(int (*)(float,float,int *)) { assert(false); }
void nv2a_d3d_set_bspline_blend(uint32_t,uint32_t) { assert(false); }
void nv2a_d3d_set_overlay_hooks(const NvOverlayHooks *) { assert(false); }
extern const NvOverlayHooks mm3_hud_hooks = {};
int mm3_menu_pointer(float,float,int *) { assert(false); return 0; }
void mm3_dev_tools_tick(void) { assert(false); }
void mm3_debug_teleport_init(void) { assert(false); }
void mm3_debug_overlay_init(void) { assert(false); }
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    bool write = !strcmp(argv[1], "--write");
    assert(write || !strcmp(argv[1], "--read"));
    load_settings();
    const char *keys[] = {"MM3_DEBUG_OVERLAY", "MM3_OVERLAY_KEY", "MM3_OVERLAY_PAD", "MM3_OVERLAY_MOUSE"};
    const char *values[] = {"1", "77", "C0", "5"};
    for (unsigned i=0; i<4; ++i) {
        bool found = false;
        for (Option &o : g_options) if (o.env == keys[i]) {
            found = true;
            assert(valid(o, values[i]));
            if (write) o.value = values[i];
            else assert(o.value == values[i]);
        }
        assert(found);
    }
    save_and_export();
    for (unsigned i=0; i<4; ++i) assert(getenv(keys[i]) && !strcmp(getenv(keys[i]), values[i]));
    puts(write ? "PASS: actual launcher saves overlay enable/bindings and exports them" :
                 "PASS: fresh process reloads and exports saved overlay enable/bindings");
}
