/* Exercise the production framebuffer message handler without a game process. */
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "../tools/xboxrecomp/src/video/fb_present.c"

ptrdiff_t xbox_GetMemoryOffset(void) { return 0; }
bool nv2a_hook_pvideo_snapshot(uint32_t regs[0x1000 / 4])
{ (void)regs; return false; }
void nv2a_hook_pvideo_consume(unsigned bank) { (void)bank; }
float nv2a_d3d_display_aspect(void) { return 1.6f; }

static unsigned close_count;
static void on_close(void) { ++close_count; }

int main(void)
{
    xbox_FramebufferSetCloseHandler(on_close);
    s_fb_running = 1;
    assert(fb_wndproc(NULL, WM_CLOSE, 0, 0) == 0);
    assert(close_count == 1 && s_fb_running == 0);
    /* Destroy after normal shutdown is cleanup, not another user request. */
    s_fb_running = 1;
    assert(fb_wndproc(NULL, WM_DESTROY, 0, 0) == 0);
    assert(close_count == 1 && s_fb_running == 0);
    xbox_FramebufferSetCloseHandler(NULL);
    s_fb_running = 1;
    assert(fb_wndproc(NULL, WM_CLOSE, 0, 0) == 0);
    assert(close_count == 1 && s_fb_running == 0);
    puts("PASS window close: user callback, destroy cleanup, optional handler");
    return 0;
}
