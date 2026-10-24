/* Compile with MSVC and the same src/generated/toolkit includes as the game.
 * This checks interop and picking; real UI transitions need runtime proof. */
/* The shared guest-state helpers, as shipped. */
#include "../tools/xboxrecomp/src/kernel/guest_state.c"
#include "../src/mods/menu_mouse.c"
#include <assert.h>
#include "../tools/xboxrecomp/src/input/xinput_device.c"

ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi, g_seh_ebp, g_ebp;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top, g_fp_cmp, g_df;
RECOMP_TLS uint16_t g_fp_cc, g_fp_control_word;
RECOMP_TLS RecompXmm g_xmm0, g_xmm1, g_xmm2, g_xmm3, g_xmm4, g_xmm5, g_xmm6, g_xmm7;
RECOMP_TLS RecompMmx g_mm0, g_mm1, g_mm2, g_mm3, g_mm4, g_mm5, g_mm6, g_mm7;
RECOMP_TLS uint32_t g_fs_base;
static float mouse_x = 0.5f, mouse_y = 0.3125f;
static float viewport[4] = {0, 0, 1, 1};
static int moves = 1, down, calls, focus_calls, viewport_calls;
static int tick_fixture, queue_calls;
static int paint_fixture, button_events;
static int track_quad_fixture, hover_paints;
static int swatch_fixture, orange_rings;
static unsigned expected_button = 1;
static int accept_focus = 1;
static const uint32_t root = 0x10000, context = 0x11000, node = 0x12000;
static HCURSOR (*cursor_handler)(void);
static int driving;
int mm3_keyboard_driving(void) { return driving; }
void xbox_FramebufferSetCursorHandler(HCURSOR (*handler)(void)) { cursor_handler = handler; }
int xbox_FramebufferHostInputCaptured(void) { return 0; }
void xbox_FramebufferSetMousePressHandler(int (*handler)(float, float)) { (void)handler; }


int mm3_mod_on(int mod) { return mod == MM3_MOD_MENU_MOUSE; }
int xbox_FramebufferMouse(float *x, float *y) { *x = mouse_x; *y = mouse_y; return moves; }
int xbox_FramebufferKeyDown(int vk) { assert(vk == VK_LBUTTON); return down; }
void nv2a_d3d_get_present_rect(float present[4])
{
    viewport_calls++;
    memcpy(present, viewport, sizeof viewport);
}
void sub_001C5935_original(void)
{
    if (!tick_fixture) return;
    uint32_t sp = g_esp;
    assert(layout && layout->root == root && !layout->count);
    g_esp = sp - 0x100; g_ecx = node;
    MEM32(g_esp+4) = context; MEM32(g_esp+12) = 0x15000;
    sub_000C2FA7();
    g_esp = sp - 0x100;
    MEM32(g_esp+8) = root;
    sub_00246894();
    g_esp = sp + 8; /* native update's ret 4 */
}
void sub_000C2FA7_original(void) { g_esp += 20; g_eax = 1; }
void sub_00246894_original(void)
{
    if (tick_fixture) {
        assert(MEMF(context+0x38) == -95); /* focus precedes event dispatch */
        queue_calls++;
    }
}
void sub_001118C7_original(void) { g_esp += 20; }
void sub_001C1585_original(void)
{
    uint32_t site = MEM32(g_esp);
    if (site == 0 && painting_colors) {
        uint32_t quad = MEM32(g_esp+8);
        assert(MEM32(g_esp+4) == MEM32(painting_colors->node+0x14));
        assert(MEMF(quad+0x20) == 1 && MEMF(quad+0x24) == .60f && MEMF(quad+0x28) == .10f);
        orange_rings++;
    }
    if (painting_prompt && (site == 0x0010E000 || site == 0x0010E03F || site == 0x0010E084))
        assert(MEM32(g_esp+4) == 42); /* Original gold texture is untouched. */
    if (site == 0x00103ACE && painting_track_item && MEMF(MEM32(g_esp+8)+0x28) == .6f) {
        uint32_t quad = MEM32(g_esp+8);
        assert(MEMF(quad+0x20) == 1 && MEMF(quad+0x24) == 1);
        hover_paints++;
    }
    g_esp += 12;
}
void sub_0010DDC0_original(void)
{
    uint32_t sp = g_esp;
    if (!painting_prompt) return;
    for (unsigned i = 0; i < 3; ++i) {
        g_esp = sp-0x100;
        MEM32(g_esp) = i == 0 ? 0x0010E000 : i == 1 ? 0x0010E03F : 0x0010E084;
        MEM32(g_esp+8) = 0x1E000; MEM32(g_esp+4) = 42;
        MEMF(0x1E00C) = 50+i*20; MEMF(0x1E010) = 180;
        MEMF(0x1E030) = 70+i*20; MEMF(0x1E058) = 210;
        for (unsigned v = 0; v < 4; ++v) {
            uint32_t color = 0x1E020+v*0x24;
            MEMF(color) = MEMF(color+4) = MEMF(color+8) = 1;
            MEMF(color+12) = .75f;
        }
        sub_001C1585();
        for (unsigned v = 0; v < 4; ++v) {
            uint32_t color = 0x1E020+v*0x24;
            assert(MEMF(color) == 1 && MEMF(color+4) == 1 && MEMF(color+8) == 1);
        }
        assert(MEMF(0x1E00C) == 50+i*20 && MEMF(0x1E010) == 180);
        assert(MEMF(0x1E030) == 70+i*20 && MEMF(0x1E058) == 210);
    }
    g_esp = sp+36;
}
static int light_text;
void sub_001C44F8_original(void)
{
    uint32_t color = MEM32(MEM32(g_esp+12)+4);
    if (MEMF(color+4) == 1) {
        assert(MEMF(color+8) == .95f && MEMF(color+12) == .80f);
        light_text++;
    } else assert(MEMF(color+4) == 0 && MEMF(color+8) == 0 && MEMF(color+12) == 0);
    assert(MEMF(color+16) == .75f);
    g_esp += 24;
}
void sub_00104F7B_original(void) {}
void sub_0010514E_original(void) {}
void sub_0010553A_original(void) { if (swatch_fixture) { g_eax = 1; painting_swatch->locked = swatch_fixture == 2; } }
void sub_00117B03_original(void) {}
void sub_00117C99_original(void) {}
void sub_00117D11_original(void) {}
void sub_00103E41_original(void) {}
void sub_00103FBD_original(void) {}
void sub_001038D0_original(void)
{
    if (!track_quad_fixture) return;
    uint32_t sp = g_esp;
    g_esp = sp-0x100; MEM32(g_esp) = 0x00103AFE; MEM32(g_esp+8) = 0x1E000;
    MEMF(0x1E00C) = -200; MEMF(0x1E010) = -100;
    MEMF(0x1E030) = -160; MEMF(0x1E058) = -60;
    MEM32(g_esp) = 0x00103ACE; sub_001C1585();
    g_esp = sp-0x100; MEM32(g_esp) = 0x00103AFE; sub_001C1585();
    g_esp = sp;
}
void sub_00111CBF_original(void)
{
    uint32_t sp = g_esp;
    if (paint_fixture) {
        g_ebp = 0x680000; /* Deliberately unrelated: generated EBP is local. */
        uint32_t frame = 0x17000 + 0xB4;
        MEM32(frame-0x10) = 0x16000; MEM32(frame-0x0C) = 0x16010;
        MEM32(frame+0x34) = 0x16008;
        g_esp = sp-0x100;
        MEM32(g_esp+8) = 0x17000;
        MEMF(0x1700C) = -100; MEMF(0x17010) = 20;
        MEMF(0x17030) = 80; MEMF(0x17058) = 140;
        MEM32(g_esp) = 0x00112029; sub_001C1585(); /* shadow */
        assert(painting_cars->count == 0);
        for (int i = 0; i < paint_fixture; i++) {
            g_esp = sp-0x100;
            MEM32(g_esp) = i & 1 ? 0x00112456 : 0x001120F3;
            sub_001C1585();
        }
    }
    g_esp = sp+24;
}
static void eligible(void)
{
    assert(g_ecx == node && MEM32(g_esp+4) == context);
    calls++;
    g_eax = MEM32(node+4);
    g_esp += 8;
    g_fp_stack[0] = -123; g_fp_top = 7; g_ebp = 0xBAD;
    memset(&g_xmm0, 0xEE, sizeof g_xmm0);
}
static void set_focus(void)
{
    assert(g_ecx == context);
    focus_calls++;
    if (accept_focus) {
        MEMF(context+0x34) = MEMF(g_esp+4);
        MEMF(context+0x38) = MEMF(g_esp+8);
    }
    g_esp += 12;
    g_eax = 1; g_df = 1; g_fp_cc = 0xFFFF;
}
static void queued_button(void)
{
    assert(g_ecx == (button_events % 2 == 0 ? 1 : 2) && g_edx == root);
    assert(MEM32(g_esp+4) == 0 && MEMF(g_esp+8) == expected_button);
    assert(MEMF(g_esp+12) == (button_events % 2 == 0 ? 1 : 0));
    assert(MEMF(g_esp+16) == 0);
    button_events++;
    g_esp += 20; g_eax = 0xBAD; g_fp_stack[0] = -456;
}
recomp_func_t recomp_lookup_manual(uint32_t va) { (void)va; return NULL; }
recomp_func_t recomp_lookup(uint32_t va)
{
    if (va == 0x001C58FC) return queued_button;
    if (va == 0x20000) return eligible;
    if (va == 0x20004) return set_focus;
    return NULL;
}

int main(void)
{
    void *memory = calloc(1, 0x800000);
    assert(memory);
    g_xbox_mem_offset = (ptrdiff_t)memory;
    MEMF(root+4) = -320; MEMF(root+8) = -240;
    MEMF(root+12) = 640; MEMF(root+16) = 480;
    MEM32(node) = 0x13000; MEM32(node+4) = 1;
    MEM32(0x13000+0xC0) = 0x20000;
    MEM32(context) = 0x14000; MEM32(0x14000+0x54) = 0x20004;
    MenuLayout frame = {0};
    frame.root = root; frame.count = 1;
    frame.tabs[0] = (MenuTab){node, context, -250, -110, 500, 30};
    controls[0] = (MenuControl){context, -110, 30, -250, -110, 150, 30, GetTickCount64()};
    assert(over_control(&frame.tabs[0], -200, -95));
    assert(!over_control(&frame.tabs[0], 0, -95)); /* Same row, beyond the visible pill. */
    assert(!over_control(&frame.tabs[0], -260, -95));
    memset(controls, 0, sizeof controls);
    g_esp = 0x700000; g_ebp = 0x680000; g_eax = 0x12345678;
    g_fp_stack[0] = 42; g_fp_top = 3; g_fp_cc = 17;
    memset(&g_xmm0, 0xAB, sizeof g_xmm0);
    RecompXmm before = g_xmm0;
    pick_native(&frame, NULL);
    assert(focus_calls == 1 && MEMF(context+0x34) == 0 && MEMF(context+0x38) == -95);
    assert(g_eax == 0x12345678 && g_esp == 0x700000 && g_ebp == 0x680000);
    assert(g_fp_stack[0] == 42 && g_fp_top == 3 && g_fp_cc == 17 && !g_df);
    assert(!memcmp(&before, &g_xmm0, sizeof before));
    int polls = viewport_calls;
    MEMF(context+0x38) = 15; /* keyboard/controller changed focus */
    pick_native(&frame, NULL);
    assert(viewport_calls == polls && focus_calls == 1 && MEMF(context+0x38) == 15);
    /* A new layout under a stationary mouse refreshes focus, while controller
     * focus survives further unchanged layouts. */
    frame.tabs[0].x = -240;
    pick_native(&frame, NULL);
    assert(focus_calls == 2 && MEMF(context+0x34) == 10);
    MEMF(context+0x38) = 15;
    pick_native(&frame, NULL); assert(focus_calls == 2 && MEMF(context+0x38) == 15);
    frame.tabs[0].x = -250;
    focus_calls = 1; /* Restore the original fixture's event-count baseline. */
    MenuClick press = {mouse_x, mouse_y};
    down = 1; pick_native(&frame, &press); down = 0;
    assert(focus_calls == 2 && MEMF(context+0x38) == -95);
    MEM32(node+4) = 0; moves++; pick_native(&frame, NULL);
    assert(focus_calls == 2); /* disabled controls cannot gain focus */
    MEM32(node+4) = 1;
    viewport[0] = 0.25f; viewport[2] = 0.5f;
    mouse_x = 0.1f; moves++; polls = calls; pick_native(&frame, NULL);
    assert(calls == polls); /* letterbox is outside the UI */
    mouse_x = NAN; moves++; pick_native(&frame, NULL); assert(calls == polls);
    mouse_x = 0.5f; frame.count = 65; moves++; pick_native(&frame, NULL); assert(calls == polls);
    frame.count = 1; viewport[2] = 0; moves++; pick_native(&frame, NULL); assert(calls == polls);
    /* Hook records native bounds, with bounded overflow and guest return ABI. */
    layout = &frame; frame.count = 0;
    MEMF(0x15000) = -250; MEMF(0x15004) = -110;
    MEMF(0x15008) = 500; MEMF(0x1500C) = 30;
    g_ecx = node; MEM32(g_esp+4) = context; MEM32(g_esp+12) = 0x15000;
    sub_000C2FA7();
    assert(frame.count == 1 && frame.tabs[0].node == node && frame.tabs[0].y == -110);
    layout = NULL;
    viewport[0] = 0; viewport[2] = 1;
    MEMF(context+0x38) = 15; moves++;
    _putenv_s("RECOMP_MENU_KEYS", "1");
    _putenv_s("RECOMP_KEYBOARD", "1");
    tick_fixture = 1; g_ecx = root; g_esp = 0x700000;
    sub_001C5935();
    assert(queue_calls == 1 && !layout && g_esp == 0x700008);
    layout = &frame; g_ecx = 0x18000; MEM32(g_esp+4) = context;
    sub_001118C7();
    assert(frame.car_node == 0x18000 && frame.car_context == context);
    layout = NULL;
    g_ecx = 0x18000; MEM32(g_esp+4) = context; paint_fixture = 2;
    sub_00111CBF();
    assert(!painting_cars && cars.node == 0x18000 && cars.context == context);
    assert(cars.count == 2 && cars.cards[0].index == 2 && cars.cards[1].index == 2);
    assert(cars.cards[0].x == -100 && cars.cards[0].y == 20);
    assert(cars.cards[0].w == 180 && cars.cards[0].h == 120);
    g_ecx = 0x18000; MEM32(g_esp+4) = context; paint_fixture = 65;
    sub_00111CBF(); assert(cars.count == 65); /* caller must reject overflow */
    g_ecx = 0x18000; MEM32(g_esp+4) = context; paint_fixture = 0;
    sub_00111CBF(); assert(cars.count == 0); /* no stale cards after empty paint */
    /* Exercise the real runtime press handler: a consumed click emits no pad A,
     * survives release/movement, and dispatches one native press/release pair. */
    mm3_menu_mouse_init();
    assert(cursor_handler && cursor_handler());
    ICONINFO cursor_info;
    assert(GetIconInfo(cursor_handler(), &cursor_info));
    assert(!cursor_info.fIcon && cursor_info.xHotspot == 2 && cursor_info.yHotspot == 2);
    ICONINFO hand_info;
    assert(GetIconInfo(menu_hand, &hand_info) && !hand_info.fIcon);
    BITMAPINFO hand_bitmap = {0};
    hand_bitmap.bmiHeader.biSize = sizeof hand_bitmap.bmiHeader;
    hand_bitmap.bmiHeader.biWidth = 32; hand_bitmap.bmiHeader.biHeight = -32;
    hand_bitmap.bmiHeader.biPlanes = 1; hand_bitmap.bmiHeader.biBitCount = 32;
    uint32_t hand_pixels[1024];
    HDC hand_dc = GetDC(NULL);
    assert(GetDIBits(hand_dc, hand_info.hbmColor, 0, 32, hand_pixels, &hand_bitmap, DIB_RGB_COLORS));
    ReleaseDC(NULL, hand_dc);
    unsigned gold_hand = 0, transparent_hand = 0;
    for (unsigned i = 0; i < 1024; ++i) {
        gold_hand += hand_pixels[i] == 0xFFFFBB35;
        transparent_hand += hand_pixels[i] == 0;
    }
    assert(gold_hand > 10 && transparent_hand > 100);
    DeleteObject(hand_info.hbmColor); DeleteObject(hand_info.hbmMask);
    BITMAPINFO bitmap = {0};
    bitmap.bmiHeader.biSize = sizeof bitmap.bmiHeader;
    bitmap.bmiHeader.biWidth = 32; bitmap.bmiHeader.biHeight = -40;
    bitmap.bmiHeader.biPlanes = 1; bitmap.bmiHeader.biBitCount = 32;
    uint32_t cursor_pixels[32*40];
    HDC dc = GetDC(NULL);
    assert(GetDIBits(dc, cursor_info.hbmColor, 0, 40, cursor_pixels, &bitmap, DIB_RGB_COLORS));
    ReleaseDC(NULL, dc);
    int blue_pixels = 0, orange_pixels = 0;
    for (unsigned i = 0; i < 32*40; ++i) {
        blue_pixels += cursor_pixels[i] == 0xFFFFDB5D;
        orange_pixels += cursor_pixels[i] == 0xFFFF9B19;
    }
    assert(blue_pixels && orange_pixels); /* Themed ARGB cursor, not the system hand. */
    DeleteObject(cursor_info.hbmColor); DeleteObject(cursor_info.hbmMask);
    driving = 1; assert(!cursor_handler());
    driving = 0; assert(cursor_handler());
    BOOL pad_click;
    down = 0; pad_click = menu_click();
    mouse_x = 0.5f; mouse_y = 0.3125f;
    down = 1; pad_click = menu_click(); assert(!pad_click);
    pad_click = menu_click(); assert(!pad_click); /* held is not another press */
    down = 0; mouse_x = 0.9f; mouse_y = 0.9f; moves++;
    pad_click = menu_click(); assert(!pad_click);
    layout = &frame; frame.count = 1; frame.root = root;
    frame.car_node = frame.car_context = 0;
    g_esp = 0x700000; MEM32(g_esp+8) = root;
    MEMF(context+0x38) = 15;
    tick_fixture = 0;
    sub_00246894();
    assert(button_events == 0 && deferred_click.root == root);
    sub_00246894(); /* Native layout reconciles focus before activation. */
    assert(button_events == 2 && MEMF(context+0x38) == -95);
    assert(g_esp == 0x700000 && g_eax != 0xBAD && g_fp_stack[0] != -456);
    sub_00246894(); assert(button_events == 2); /* consumed once */
    mouse_click(0.9f, 0.9f); sub_00246894(); assert(button_events == 2); /* off-item */
    frame.count = 0; mouse_click(0.5f, 0.3125f);
    sub_00246894(); assert(button_events == 2 && click_pending);
    frame.count = 1; sub_00246894(); assert(button_events == 4 && !click_pending);
    button_events = 2; /* Empty preliminary passes must not consume a click. */
    accept_focus = 0; MEMF(context+0x38) = 15;
    mouse_click(0.5f, 0.3125f); sub_00246894(); assert(button_events == 2);
    accept_focus = 1; /* a failed native focus change must not activate prior focus */
    frame.tabs[0].x = NAN;
    mouse_click(0.5f, 0.3125f); sub_00246894(); assert(button_events == 2);
    frame.tabs[0].x = -250; MEMF(root+12) = NAN;
    mouse_click(0.5f, 0.3125f); sub_00246894(); assert(button_events == 2);
    MEMF(root+12) = 640;
    /* Native car selection advances through original directional actions,
     * observes the resulting target, and never sends A for a side card. */
    frame.car_node = 0x18000; frame.car_context = context;
    frame.tabs[0] = (MenuTab){node, context, -250, -190, 520, 390};
    cars.node = frame.car_node; cars.context = context; cars.count = 2;
    cars.cards[0] = (MenuCar){-100, 20, 180, 120, 2, 1};
    cars.cards[1] = (MenuCar){-100, 20, 500, 120, 3, 1}; /* Overlapping transparent margin. */
    MEM32(frame.car_node+0x28) = 0;
    assert(car_at_point(&cars, 0, 80) == &cars.cards[0]);
    cars.cards[0].unlocked = 0;
    assert(!car_at_point(&cars, 0, 80)); /* No false cue on a locked nearest card. */
    cars.cards[0].unlocked = 1;
    expected_button = 7; mouse_click(0.5f, 2.0f/3.0f);
    sub_00246894(); assert(button_events == 2 && deferred_click.root == root);
    sub_00246894(); assert(button_events == 4 && car_click.node);
    assert(car_click.index == 2); /* Nearest native centre, not paint order. */
    MEM32(frame.car_node+0x28) = 1;
    sub_00246894(); assert(button_events == 6);
    MEM32(frame.car_node+0x28) = 2;
    sub_00246894(); assert(button_events == 6 && !car_click.node);
    expected_button = 1; mouse_click(0.5f, 2.0f/3.0f);
    sub_00246894(); assert(button_events == 8); /* selected card confirms */
    expected_button = 5; cars.cards[0].index = 0;
    mouse_click(0.5f, 2.0f/3.0f); sub_00246894(); assert(button_events == 10);
    sub_00246894(); assert(button_events == 10 && !car_click.node); /* no native progress: stop */
    mouse_click(0.5f, 2.0f/3.0f); sub_00246894(); assert(button_events == 12);
    MEM32(frame.car_node+0x28) = 0;
    sub_00246894(); assert(!car_click.node); /* native skip reaches target */
    car_click.root = root; car_click.node = 0xFFFFF000;
    frame.car_node = 0;
    advance_car(&frame); assert(!car_click.node); /* departed widget never dereferenced */
    /* Track tiles sit below the single Race tab, outside its navigation row. */
    frame.race_node = 0x19000;
    frame.tabs[0] = (MenuTab){node, context, -170, -150, 340, 30};
    tracks.node = frame.race_node; tracks.context = context; tracks.tick = GetTickCount64();
    tracks.count = 2; tracks.current = 0;
    tracks.items[0] = (MenuTrack){-250, -110, 50, 40, 0, 1};
    tracks.items[1] = (MenuTrack){-190, -110, 50, 40, 2, 1};
    expected_button = 7; mouse_click(0.2421875f, 0.3125f);
    sub_00246894(); assert(button_events == 12 && deferred_click.root == root);
    sub_00246894(); assert(button_events == 14 && track_click.index == 2);
    sub_00246894(); assert(button_events == 14); /* Wait for native paint to observe the action. */
    tracks.current = 1; sub_00246894(); assert(button_events == 16);
    tracks.current = 2; sub_00246894(); assert(button_events == 16 && !track_click.node);
    expected_button = 1; mouse_click(0.2421875f, 0.3125f);
    sub_00246894(); assert(button_events == 18); /* Selected track confirms once. */
    tracks.items[1].unlocked = 0; mouse_click(0.2421875f, 0.3125f);
    sub_00246894(); assert(button_events == 18); /* Locked tiles do not activate old focus. */
    tracks.count = 65; mouse_click(0.2421875f, 0.3125f);
    sub_00246894(); assert(button_events == 18);
    frame.race_node = 0; track_click.root = root; track_click.node = 0xFFFFF000;
    advance_track(&frame); assert(!track_click.node);
    MenuTracks captured_tracks = {0};
    painting_tracks = &captured_tracks; track_quad_fixture = 1;
    g_esp = 0x700000; MEM32(g_esp) = 0x00103E3B;
    MEM32(g_esp+20) = 0x1A000; MEM32(0x1A000) = 0x1B000;
    MEM32(0x1B00C) = 3; MEM32(g_esp+36) = 3; MEM32(g_esp+24) = 3;
    sub_001038D0();
    assert(captured_tracks.count == 1 && captured_tracks.current == 3);
    assert(!captured_tracks.items[0].unlocked); /* Native comparison is strict <. */
    assert(captured_tracks.items[0].x == -200 && captured_tracks.items[0].w == 40);
    MEM32(0x1B00C) = 2; MEM32(g_esp+24) = 2;
    sub_001038D0(); assert(captured_tracks.items[1].unlocked);
    captured_tracks.context = context;
    MEMF(context+4) = -320; MEMF(context+8) = -240;
    MEMF(context+12) = 640; MEMF(context+16) = 480;
    float prior_x = mouse_x, prior_y = mouse_y;
    mouse_x = .21875f; mouse_y = .3125f;
    MEM32(0x1B00C) = 2; MEM32(g_esp+24) = 2;
    tracks.node = captured_tracks.node = 0x19000;
    tracks.count = 1; tracks.items[0] = (MenuTrack){-200,-100,40,40,2,1};
    tracks.tick = GetTickCount64();
    int before_hover = hover_paints;
    sub_001038D0(); assert(hover_paints == before_hover+1);
    MEM32(0x1B00C) = 3;
    sub_001038D0(); assert(hover_paints == before_hover+1); /* Locked track has no cue. */
    mouse_x = prior_x; mouse_y = prior_y;
    painting_tracks = NULL; track_quad_fixture = 0;
    /* Native button paint, including all three pill pieces, narrows the
     * navigation row. Reconstruct its local frame from the quad argument. */
    MEM32(0x1F064+0x6C) = 0x1F100;
    MEMF(0x1F104) = -110; MEMF(0x1F10C) = 30;
    MEMF(0x1F00C) = -270; MEMF(0x1F010) = -110;
    MEMF(0x1F030) = -200; MEMF(0x1F058) = -80;
    MEMF(context+0x38) = -95;
    g_edx = context; g_esp = 0x700000; MEM32(g_esp+8) = 0x1F000;
    MEM32(g_esp+4) = 1001;
    MEM32(g_esp) = 0x0010C2E2; sub_001C1585();
    MEMF(0x1F00C) = -200; MEMF(0x1F030) = -120;
    g_esp = 0x700000; MEM32(g_esp+4) = 1002; MEM32(g_esp) = 0x0010C322; sub_001C1585();
    MEMF(0x1F00C) = -120; MEMF(0x1F030) = -100;
    g_esp = 0x700000; MEM32(g_esp+4) = 1003; MEM32(g_esp) = 0x0010C35E; sub_001C1585();
    MenuTab bounded = {node, context, -250, -110, 500, 30};
    assert(over_control(&bounded, -200, -95) && !over_control(&bounded, 0, -95));
    /* Actual help-painter call sites capture A/B/X/Y bounds and dispatch the
     * corresponding native button once, without changing the current focus. */
    for (unsigned button = 1; button <= 4; ++button) {
        const uint32_t sites[] = {0x0010D7EE,0x0010D823,0x0010D858,0x0010D88D};
        const unsigned ids[] = {1,3,21,22};
        memset(prompts, 0, sizeof prompts);
        g_esp = 0x700000; g_ecx = node; MEM32(g_esp) = sites[button-1];
        MEM32(g_esp+4) = context;
        sub_0010DDC0();
        assert(prompts[0].button == ids[button-1] && prompts[0].w == 60);
        MEMF(context+4) = -320; MEMF(context+8) = -240;
        MEMF(context+12) = 640; MEMF(context+16) = 480;
        float old_x = mouse_x, old_y = mouse_y;
        mouse_x = .625f; mouse_y = .9f;
        HCURSOR arrow = menu_cursor;
        MenuPrompt *parent_prompt = painting_prompt;
        painting_prompt = &prompts[0];
        int before_text = light_text;
        for (unsigned pass = 0; pass < 2; ++pass) {
            mouse_x = pass ? .8f : .625f; mouse_y = .9f;
            g_esp = 0x700000; MEM32(g_esp) = 0x0010E173; MEM32(g_esp+12) = 0x1D000;
            MEM32(0x1D004) = 0x1D100; MEM32(0x1D100) = 0x0037D478;
            MEMF(0x1D104) = MEMF(0x1D108) = MEMF(0x1D10C) = 0; MEMF(0x1D110) = .75f;
            sub_001C44F8();
            assert(light_text == before_text+1);
            assert(MEMF(0x1D104) == 0 && MEMF(0x1D108) == 0 && MEMF(0x1D10C) == 0);
        }
        painting_prompt = parent_prompt;
        mouse_x = .625f;
        MEM32(g_esp) = sites[button-1]; MEM32(g_esp+4) = context;

        g_esp = 0x700000; g_ecx = node; MEM32(g_esp) = sites[button-1]; MEM32(g_esp+4) = context; sub_0010DDC0();
        assert(cursor_handler() == menu_hand && menu_hand != arrow);
        mouse_x = .8f; /* Same row outside the clickable backing. */
        g_esp = 0x700000; g_ecx = node; MEM32(g_esp) = sites[button-1]; MEM32(g_esp+4) = context; sub_0010DDC0();
        assert(cursor_handler() == arrow);
        mouse_x = old_x; mouse_y = old_y;
        g_esp = 0x700000; MEM32(g_esp+8) = root;
        expected_button = ids[button-1];
        int before = button_events, focused = focus_calls;
        mouse_click(0.625f, 0.9f); sub_00246894();
        assert(button_events == before+2 && focus_calls == focused);
        sub_00246894(); assert(button_events == before+2);
        mouse_click(0.8f, 0.9f); sub_00246894();
        assert(button_events == before+2); /* Same row, outside prompt. */
        prompts[0].tick = GetTickCount64()-251;
        mouse_click(0.625f, 0.9f); sub_00246894();
        assert(button_events == before+2); /* Hidden/stale prompts ignored. */
    }
    /* Arrow bounds belong to the option node, outside its label pill. */
    memset(prompts, 0, sizeof prompts); memset(controls, 0, sizeof controls);
    frame.tabs[0] = (MenuTab){node,context,-250,-110,500,30};
    MEMF(context+0x34) = 0; MEMF(context+0x38) = -95;
    arrows[0] = (MenuArrow){0x25000,context,200,-110,20,30,7,GetTickCount64()}; /* Style node differs from tab node. */
    expected_button = 7;
    int before_arrows = button_events;
    mouse_click(.828125f,.3125f); sub_00246894();
    assert(button_events == before_arrows+2);
    arrows[0].button = 5; expected_button = 5;
    mouse_click(.828125f,.3125f); sub_00246894();
    assert(button_events == before_arrows+4);
    arrows[0].tick = GetTickCount64()-251;
    controls[0] = (MenuControl){context,-110,30,-250,-110,150,30,GetTickCount64()};
    mouse_click(.828125f,.3125f); sub_00246894();
    assert(button_events == before_arrows+4);
    memset(arrows, 0, sizeof arrows); memset(controls, 0, sizeof controls);
    /* Swatches select through native up/down and stop at their target. */
    frame.color_node = colors.node = 0x24000; colors.context = root;
    colors.tick = GetTickCount64(); colors.count = 1; colors.current = 0;
    colors.items[0] = (MenuSwatch){-20,-20,40,40,2,0};
    expected_button = 8;
    int before_colors = button_events;
    mouse_click(.5f,.5f); sub_00246894();
    assert(button_events == before_colors+2 && color_click.index == 2);
    sub_00246894(); assert(button_events == before_colors+2);
    colors.current = 1; sub_00246894(); assert(button_events == before_colors+4);
    colors.current = 2; sub_00246894(); assert(!color_click.node);
    expected_button = 6; colors.items[0].index = 0;
    mouse_click(.5f,.5f); sub_00246894(); assert(button_events == before_colors+6);
    colors.current = 0; sub_00246894(); assert(!color_click.node);
    colors.items[0].locked = 1; colors.items[0].index = 2;
    mouse_click(.5f,.5f); sub_00246894(); assert(button_events == before_colors+6);
    frame.color_node = 0;
    /* A modal key uses its native caret and one activation; disabled keys,
     * blank space and stale paint cannot activate underlying menu controls. */
    memset(prompts, 0, sizeof prompts);
    frame.keyboard_node = keyboard.node = 0x22000;
    keyboard.count = 2; keyboard.tick = GetTickCount64();
    keyboard.keys[0] = (MenuKey){-20,-20,40,40,2,1,0};
    keyboard.keys[1] = (MenuKey){20,-20,40,40,3,1,1};
    expected_button = 1;
    int before_keys = button_events;
    mouse_click(.5f,.5f); sub_00246894();
    assert(button_events == before_keys+2);
    assert(MEM32(keyboard.node+0x70) == 2 && MEM32(keyboard.node+0x74) == 1);
    mouse_click(.5625f,.5f); sub_00246894();
    mouse_click(.8f,.5f); sub_00246894();
    assert(button_events == before_keys+2);
    keyboard.tick = GetTickCount64()-251;
    mouse_click(.5f,.5f); sub_00246894();
    assert(button_events == before_keys+2);
    frame.keyboard_node = 0;
    MenuKeyboard captured_keys = {0}; painting_keyboard = &captured_keys;
    g_esp = 0x700000; MEM32(g_esp) = 0x00118902; MEM32(g_esp+8) = 0x23050;
    MEM32(0x23044) = 2; MEM32(0x23048) = 4; MEM32(g_esp+24) = 0;
    MEMF(0x23050) = 20; MEMF(0x23054) = 30; MEMF(0x23058) = 40; MEMF(0x2305C) = 50;
    sub_00117D11();
    assert(captured_keys.count == 1 && captured_keys.keys[0].col == 5 && captured_keys.keys[0].row == 2);
    assert(captured_keys.keys[0].x == 20 && captured_keys.keys[0].h == 50);
    painting_keyboard = NULL;
    MenuColors captured_colors = {0};
    captured_colors.node = 0x24000; captured_colors.context = root;
    painting_colors = &captured_colors; mouse_x = mouse_y = .5f;
    MEM32(0x24014) = 42;
    MEMF(0x2600C) = -20; MEMF(0x26010) = -20;
    MEMF(0x26030) = 20; MEMF(0x26058) = 20;
    for (unsigned v = 0; v < 4; ++v) for (unsigned c = 0; c < 3; ++c) MEMF(0x26020+v*0x24+c*4) = 1;
    for (unsigned pass = 0; pass < 3; ++pass) {
        g_esp = 0x700000; MEM32(g_esp) = 0x00105263; MEM32(g_esp+8) = 0x26000;
        MEM32(g_esp+12) = 0x27000; MEM32(g_esp+16) = 0x28000; MEM32(0x28000) = 1;
        captured_colors.current = pass == 2 ? 1 : 0;
        swatch_fixture = pass == 1 ? 2 : 1;
        sub_0010553A();
        assert(orange_rings == 1); /* Hover ring only on an unlocked, unselected swatch. */
        assert(MEMF(0x26024) == 1 && MEMF(0x26028) == 1 && g_esp == 0x700000);
    }
    painting_colors = NULL; swatch_fixture = 0;
    layout = NULL;
    xbox_InputSetMenuClickHandler(NULL);
    down = 1; pad_click = menu_click(); assert(pad_click); /* default input preserved */
    free(memory);
    puts("native menu mouse contracts passed");
    return 0;
}
