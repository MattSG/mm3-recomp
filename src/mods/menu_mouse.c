#if defined(XBOXRECOMP_SDL3)
/* No <windows.h> on this path: the runtime's portable headers supply the
 * NT types, SRWLOCK and GetTickCount64 (the real SDK on a Windows host). */
#include <SDL3/SDL.h>
#include "platform/xbox_winnt.h"
#include "platform/win32_compat.h"
#else
#include <windows.h>
#endif
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "recomp_funcs.h"
#include "nv2a_d3d11.h"
#include "mods.h"
#include "fb_present.h"

/* Native layout supplies bounds and the exact focus context. Pick after
 * layout, before queued input, on the title thread. The existing keyboard
 * click handler records presses for native dispatch. Composite controls supply
 * their geometry inside native paint; renderer frames are never scanned. */
extern void sub_001C5935_original(void);
extern void sub_000C2FA7_original(void);
extern void sub_00246894_original(void);
extern void sub_001118C7_original(void);
extern void sub_00111CBF_original(void);
extern void sub_001C1585_original(void);
extern void sub_00103E41_original(void);
extern void sub_00103FBD_original(void);
extern void sub_001038D0_original(void);
extern void sub_0010DDC0_original(void);
extern void sub_001C44F8_original(void);
extern void sub_00117B03_original(void);
extern void sub_00117C99_original(void);
extern void sub_00117D11_original(void);
extern void sub_00104F7B_original(void);
extern void sub_0010514E_original(void);
extern void sub_0010553A_original(void);
extern int xbox_FramebufferMouse(float *x, float *y);
extern void xbox_InputSetMenuClickHandler(int (*click)(float, float));
static int trace_native(void);
static int enabled(void);
extern int mm3_keyboard_driving(void);
static XboxCursor menu_cursor, menu_hand;
static const float menu_orange[3] = {1.f, .60f, .10f};
typedef struct { float x, y, w, h; ULONGLONG tick; } CursorPrompt;
static CursorPrompt cursor_prompts[16];
static SRWLOCK cursor_lock = SRWLOCK_INIT;
#include "menu_cursor.h"

#if defined(XBOXRECOMP_SDL3)
/* SDL cursors need the video subsystem, which starts after mods init, so they
 * are made on the window thread at first use. */
static void make_menu_cursors(void)
{
    static int made;
    if (made) return;
    made = 1;
    menu_cursor = create_menu_cursor();
    menu_hand = create_menu_hand();
}
#endif

static XboxCursor native_cursor(void)
{
    if (!enabled()) return NULL;
#if defined(XBOXRECOMP_SDL3)
    make_menu_cursors();
#endif
    float x, y;
    xbox_FramebufferMouse(&x, &y);
    XboxCursor cursor = menu_cursor;
    AcquireSRWLockShared(&cursor_lock);
    for (unsigned i = 0; i < 16; ++i) {
        const CursorPrompt *p = &cursor_prompts[i];
        if (GetTickCount64()-p->tick < 250 && x >= p->x && x < p->x+p->w && y >= p->y && y < p->y+p->h) {
            cursor = menu_hand;
            break;
        }
    }
    ReleaseSRWLockShared(&cursor_lock);
    return cursor;
}

typedef struct {
    uint32_t node, context;
    float x, y, w, h;
} MenuTab;
/* ponytail: cap at 64 native tab stops; skip overflowing layouts rather than
 * pick from incomplete data. Grow if a real menu needs it. */
typedef struct {
    uint32_t root;
    uint32_t car_node, car_context;
    uint32_t race_node, keyboard_node, color_node;
    unsigned count;
    MenuTab tabs[64];
} MenuLayout;
static RECOMP_TLS MenuLayout *layout;
static RECOMP_TLS int last_moves;
static RECOMP_TLS MenuLayout previous_layout;

/* Native tab stops deliberately span the whole navigation row. The button
 * style paints narrower left/middle/right pill pieces; retain those exact
 * bounds, without inspecting renderer frames, textures or highlight alpha. */
typedef struct {
    uint32_t context;
    float row_y, row_h, x, y, w, h;
    ULONGLONG tick;
} MenuControl;
static RECOMP_TLS MenuControl controls[64], painting_control;
typedef struct {
    uint32_t node, context;
    float x, y, w, h;
    unsigned button;
    ULONGLONG tick;
} MenuArrow;
static RECOMP_TLS MenuArrow arrows[128];
static RECOMP_TLS unsigned picked_button;

static int over_control(const MenuTab *tab, float x, float y)
{
    for (unsigned i = 0; i < 64; ++i) {
        const MenuControl *c = &controls[i];
        if (c->context == tab->context && c->row_y == tab->y && c->row_h == tab->h &&
            GetTickCount64() - c->tick < 250)
            return x >= c->x && x < c->x+c->w && y >= c->y && y < c->y+c->h;
    }
    return 1; /* Other native widgets retain their own layout bounds. */
}

/* The car widget has one tab stop. Keep its own card geometry and vector
 * index, not renderer textures or inferred rows. Published only after paint. */
typedef struct {
    float x, y, w, h;
    unsigned index, unlocked;
} MenuCar;
typedef struct {
    uint32_t node, context;
    unsigned count;
    MenuCar cards[64];
} MenuCars;
static MenuCar *car_at_point(MenuCars *list, float x, float y)
{
    MenuCar *hit = NULL;
    float nearest = INFINITY;
    if (list->count > 64) return NULL;
    for (unsigned i = 0; i < list->count; ++i) {
        MenuCar *c = &list->cards[i];
        if (!(c->w > 0 && c->h > 0) || !isfinite(c->x) || !isfinite(c->y) ||
            !isfinite(c->w*c->h) || x < c->x || x >= c->x+c->w || y < c->y || y >= c->y+c->h) continue;
        float dx = x-(c->x+c->w*.5f), dy = y-(c->y+c->h*.5f), distance = dx*dx+dy*dy;
        if (distance < nearest) { nearest = distance; hit = c; }
    }
    return hit && hit->unlocked ? hit : NULL;
}
static RECOMP_TLS MenuCars cars;
static RECOMP_TLS MenuCars *painting_cars;
typedef struct {
    float x, y, w, h;
    unsigned index, unlocked;
} MenuTrack;
typedef struct {
    uint32_t node, context;
    unsigned count, current;
    ULONGLONG tick;
    MenuTrack items[64];
} MenuTracks;
static RECOMP_TLS MenuTracks tracks, *painting_tracks;
static RECOMP_TLS MenuTrack *painting_track_item;
static RECOMP_TLS struct {
    uint32_t node, root;
    unsigned index, previous, remaining;
    ULONGLONG deadline;
} track_click;
static RECOMP_TLS struct {
    uint32_t root, node, context;
    unsigned index, previous, remaining;
} car_click;

/* Native help painter draws each enabled A/B/X/Y prompt separately. */
typedef struct {
    uint32_t node, context;
    unsigned button;
    float x, y, w, h;
    ULONGLONG tick;
} MenuPrompt;
static RECOMP_TLS MenuPrompt prompts[16], *painting_prompt;
static RECOMP_TLS uint32_t last_help_node;
static uint32_t cursor_help_node;
typedef struct { float x, y, w, h; unsigned col, row, disabled; } MenuKey;
typedef struct {
    uint32_t node, context;
    unsigned count;
    ULONGLONG tick;
    MenuKey keys[64];
} MenuKeyboard;
static RECOMP_TLS MenuKeyboard keyboard, *painting_keyboard;

void sub_00117B03(void)
{
    if (layout) layout->keyboard_node = g_ecx;
    sub_00117B03_original();
}

void sub_00117C99(void)
{
    if (!enabled()) { sub_00117C99_original(); return; }
    MenuKeyboard next = {0}, *parent = painting_keyboard;
    next.node = g_ecx; next.context = MEM32(g_esp+4);
    painting_keyboard = &next;
    sub_00117C99_original();
    painting_keyboard = parent;
    next.tick = GetTickCount64();
    keyboard = next;
}

void sub_00117D11(void)
{
    if (painting_keyboard) {
        uint32_t site = MEM32(g_esp), rect = MEM32(g_esp+8);
        unsigned col = 0, row = 0, known = 1;
        switch (site) {
        case 0x0011858A: break;
        case 0x00118600: row = 1; break;
        case 0x00118686: row = 2; break;
        case 0x0011870E: col = 7; row = 3; break;
        case 0x0011878A: col = 1; row = 4; break;
        case 0x001187F8: col = 7; row = 4; break;
        case 0x00118864: col = 9; row = 4; break;
        case 0x00118902:
            row = MEM32(rect-0x50+0x44);
            col = MEM32(rect-0x50+0x48)+1;
            break;
        default: known = 0; break;
        }
        if (known) {
            unsigned i = painting_keyboard->count++;
            if (i < 64) painting_keyboard->keys[i] = (MenuKey){
                MEMF(rect), MEMF(rect+4), MEMF(rect+8), MEMF(rect+12),
                col, row, MEM32(g_esp+24)};
        }
    }
    sub_00117D11_original();
}

typedef struct { float x, y, w, h; unsigned index, locked; } MenuSwatch;
typedef struct {
    uint32_t node, context;
    unsigned count, current;
    ULONGLONG tick;
    MenuSwatch items[8];
} MenuColors;
static RECOMP_TLS MenuColors colors, *painting_colors;
static RECOMP_TLS MenuSwatch *painting_swatch;
static RECOMP_TLS struct {
    uint32_t root, node;
    unsigned index, previous, remaining;
    ULONGLONG deadline;
} color_click;

void sub_00104F7B(void)
{
    if (layout) layout->color_node = g_ecx;
    sub_00104F7B_original();
}

void sub_0010514E(void)
{
    if (!enabled()) { sub_0010514E_original(); return; }
    MenuColors next = {0}, *parent = painting_colors;
    next.node = g_ecx; next.context = MEM32(g_esp+4);
    painting_colors = &next;
    sub_0010514E_original();
    painting_colors = parent;
    next.tick = GetTickCount64();
    colors = next;
}


static int menu_point(uint32_t root, float *x, float *y)
{
    NvMenuRect unused;
    float present[4];
    nv2a_d3d_menu_rects(&unused, 0, present); /* viewport only; no quad copy */
    if (!isfinite(*x) || !isfinite(*y) || !(present[2] > 0) || !(present[3] > 0)) return 0;
    *x = (*x-present[0])/present[2]; *y = (*y-present[1])/present[3];
    if (!(*x >= 0 && *x <= 1 && *y >= 0 && *y <= 1)) return 0;
    *x = MEMF(root+4) + *x*MEMF(root+12);
    *y = MEMF(root+8) + *y*MEMF(root+16);
    return isfinite(*x) && isfinite(*y);
}

static int over_prompt(const MenuPrompt *p, float x, float y)
{
    return p->button && GetTickCount64()-p->tick < 250 &&
        x >= p->x && x < p->x+p->w && y >= p->y && y < p->y+p->h;
}

void sub_0010DDC0(void)
{
    uint32_t site = MEM32(g_esp);
    unsigned button = site == 0x0010D7EE ? 1 : site == 0x0010D823 ? 3 :
                      site == 0x0010D858 ? 21 : site == 0x0010D88D ? 22 : 0;
    MenuPrompt next = {0}, *parent = painting_prompt;
    next.node = g_ecx; next.context = MEM32(g_esp+4); next.button = button;
    painting_prompt = enabled() && button ? &next : NULL;
    sub_0010DDC0_original();
    painting_prompt = parent;
    if (next.w > 0 && next.h > 0) {
        unsigned target = 0;
        for (unsigned i = 0; i < 16; ++i) {
            if (prompts[i].node == next.node && prompts[i].button == button) { target = i; break; }
            if (prompts[i].tick < prompts[target].tick) target = i;
        }
        next.tick = GetTickCount64();
        prompts[target] = next;
        last_help_node = next.node;
        NvMenuRect unused;
        float present[4], rw = MEMF(next.context+12), rh = MEMF(next.context+16);
        nv2a_d3d_menu_rects(&unused, 0, present);
        if (rw > 0 && rh > 0 && present[2] > 0 && present[3] > 0) {
            CursorPrompt p = {present[0]+(next.x-MEMF(next.context+4))/rw*present[2],
                present[1]+(next.y-MEMF(next.context+8))/rh*present[3],
                next.w/rw*present[2], next.h/rh*present[3], next.tick};
            AcquireSRWLockExclusive(&cursor_lock);
            if (cursor_help_node != next.node) {
                memset(cursor_prompts, 0, sizeof cursor_prompts);
                cursor_help_node = next.node;
            }
            cursor_prompts[target] = p;
            ReleaseSRWLockExclusive(&cursor_lock);
        }
    }
}

typedef struct { float x, y; } MenuClick;
static SRWLOCK click_lock = SRWLOCK_INIT;
static MenuClick pending_click;
static int click_pending;
static RECOMP_TLS struct { uint32_t root; MenuClick click; } deferred_click;

static int mouse_click(float x, float y)
{
    AcquireSRWLockExclusive(&click_lock);
    /* ponytail: latest press wins if multiple clicks arrive before one UI
     * update; a queue is unnecessary unless that loses real input. */
    pending_click = (MenuClick){x, y};
    click_pending = 1;
    ReleaseSRWLockExclusive(&click_lock);
    return 1;
}

void mm3_menu_mouse_init(void)
{
    (void)enabled(); /* Cache environment settings before worker threads start. */
#if !defined(XBOXRECOMP_SDL3)
    /* Windows composites the themed pointer independently of guest frames. */
    menu_cursor = create_menu_cursor();
    if (!menu_cursor) menu_cursor = LoadCursorA(NULL, IDC_ARROW);
    menu_hand = create_menu_hand();
    if (!menu_hand) menu_hand = LoadCursorA(NULL, IDC_HAND);
#endif
    xbox_FramebufferSetCursorHandler(native_cursor);
    xbox_InputSetMenuClickHandler(mouse_click);
}

static int take_click(MenuClick *click)
{
    AcquireSRWLockExclusive(&click_lock);
    int pending = click_pending;
    *click = pending_click;
    click_pending = 0;
    ReleaseSRWLockExclusive(&click_lock);
    return pending;
}

typedef struct {
    uint32_t regs[9];
    double fp[8];
    int top, cmp, df;
    uint16_t cc, control;
    RecompXmm xmm[8];
} GuestState;

static GuestState save_guest(void)
{
    GuestState s = {{g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi, g_seh_ebp, g_ebp},
        {0}, g_fp_top, g_fp_cmp, g_df, g_fp_cc, g_fp_control_word,
        {g_xmm0, g_xmm1, g_xmm2, g_xmm3, g_xmm4, g_xmm5, g_xmm6, g_xmm7}};
    memcpy(s.fp, g_fp_stack, sizeof s.fp);
    return s;
}

static void restore_guest(const GuestState *s)
{
    g_eax = s->regs[0]; g_ecx = s->regs[1]; g_edx = s->regs[2]; g_esp = s->regs[3];
    g_ebx = s->regs[4]; g_esi = s->regs[5]; g_edi = s->regs[6];
    g_seh_ebp = s->regs[7]; g_ebp = s->regs[8];
    memcpy(g_fp_stack, s->fp, sizeof s->fp);
    g_fp_top = s->top; g_fp_cmp = s->cmp; g_df = s->df;
    g_fp_cc = s->cc; g_fp_control_word = s->control;
    g_xmm0 = s->xmm[0]; g_xmm1 = s->xmm[1]; g_xmm2 = s->xmm[2]; g_xmm3 = s->xmm[3];
    g_xmm4 = s->xmm[4]; g_xmm5 = s->xmm[5]; g_xmm6 = s->xmm[6]; g_xmm7 = s->xmm[7];
}

static void queue_button(uint32_t context, unsigned button)
{
    recomp_func_t fn = recomp_lookup_manual(0x001C58FC);
    if (!fn) fn = recomp_lookup(0x001C58FC);
    if (!fn) return;
    GuestState s = save_guest();
    for (int down = 1; down >= 0; down--) {
        g_esp = s.regs[3] - 0x100;
        g_esp -= 4; MEMF(g_esp) = 0;
        g_esp -= 4; MEMF(g_esp) = (float)down;
        g_esp -= 4; MEMF(g_esp) = (float)button;
        PUSH32(g_esp, 0); /* player/controller zero */
        /* Native input uses event type 1 for press and 2 for release.
         * Two type-1 events activate twice, even with a zero pressure. */
        g_ecx = down ? 1 : 2; g_edx = context;
        PUSH32(g_esp, 0);
        fn();
    }
    restore_guest(&s);
    if (trace_native()) {
        static unsigned reports;
        if (reports++ < 64)
        fprintf(stderr, "[MENU_NATIVE_ENQUEUE] context=%08X button=%u tick=%llu\n", context, button, GetTickCount64());
    }
}

static int enabled(void)
{
    static int keys = -1;
    if (keys < 0) {
        const char *v = getenv("RECOMP_MENU_KEYS");
        keys = v && *v && *v != '0';
        v = getenv("RECOMP_KEYBOARD");
        keys = keys && v && *v && *v != '0';
    }
    return keys && mm3_mod_on(MM3_MOD_MENU_MOUSE) &&
        !mm3_keyboard_driving() && !xbox_FramebufferHostInputCaptured();
}

static int trace_native(void)
{
    static int trace = -1;
    if (trace < 0) trace = getenv("MM3_MENU_NATIVE_TRACE") != NULL;
    return trace;
}

static int call_virtual(uint32_t object, unsigned slot)
{
    uint32_t target = MEM32(MEM32(object) + slot);
    recomp_func_t fn = recomp_lookup_manual(target);
    if (!fn) fn = recomp_lookup(target);
    if (!fn) return 0;
    g_ecx = object;
    PUSH32(g_esp, 0);
    fn();
    return 1;
}

static int pick_native(MenuLayout *frame, const MenuClick *click)
{
    float x, y, best_area = INFINITY;
    MenuTab *hit = NULL;
    MenuTrack *track_hit = NULL;
    MenuArrow *arrow_hit = NULL;
    picked_button = 1;
    int changed_focus = 0;
    int moves = xbox_FramebufferMouse(&x, &y);
    if (frame->count > 64) return 0;
    int changed_layout = previous_layout.root != frame->root || previous_layout.count != frame->count ||
        previous_layout.car_node != frame->car_node || previous_layout.race_node != frame->race_node ||
        previous_layout.keyboard_node != frame->keyboard_node || previous_layout.color_node != frame->color_node ||
        memcmp(previous_layout.tabs, frame->tabs, frame->count*sizeof frame->tabs[0]);
    if (!click && moves == last_moves && !changed_layout) return 0;
    previous_layout = *frame;
    last_moves = moves;
    if (click) { x = click->x; y = click->y; }
    if (!menu_point(frame->root, &x, &y)) return 0;
    if (click) for (unsigned i = 0; i < 16; ++i) {
        const MenuPrompt *p = &prompts[i];
        int context_matches = p->context == frame->root;
        for (unsigned j = 0; j < frame->count && !context_matches; ++j)
            context_matches = p->context == frame->tabs[j].context;
        if (p->node == last_help_node && context_matches && over_prompt(p, x, y)) {
            queue_button(frame->root, p->button);
            return 0;
        }
    }
    if (frame->keyboard_node) {
        /* The modal owns input: never click the car menu underneath it.
         * These fields are the native key caret, not the text binding. */
        if (keyboard.node == frame->keyboard_node && keyboard.count <= 64 &&
            GetTickCount64()-keyboard.tick < 250) {
            for (unsigned i = 0; i < keyboard.count; ++i) {
                MenuKey *k = &keyboard.keys[i];
                if (!k->disabled && k->w > 0 && k->h > 0 &&
                    x >= k->x && x < k->x+k->w && y >= k->y && y < k->y+k->h) {
                    MEM32(keyboard.node+0x70) = k->col;
                    MEM32(keyboard.node+0x74) = k->row;
                    if (click) queue_button(frame->root, 1);
                    break;
                }
            }
        }
        return 0;
    }
    if (click && frame->color_node && colors.node == frame->color_node &&
        colors.context == frame->root && colors.count <= 8 && GetTickCount64()-colors.tick < 250) {
        for (unsigned i = 0; i < colors.count; ++i) {
            MenuSwatch *c = &colors.items[i];
            float dx = (x-c->x)/c->w*2-1, dy = (y-c->y)/c->h*2-1;
            if (c->w > 0 && c->h > 0 && dx*dx+dy*dy <= 1) {
                if (!c->locked && c->index != colors.current) {
                    color_click.root = frame->root; color_click.node = colors.node;
                    color_click.index = c->index; color_click.previous = UINT32_MAX;
                    color_click.remaining = 64; color_click.deadline = GetTickCount64()+250;
                }
                return 0;
            }
        }
    }
    if (frame->race_node && tracks.node == frame->race_node &&
        tracks.count <= 64 && GetTickCount64()-tracks.tick < 250) {
        for (unsigned i = 0; i < tracks.count; ++i) {
            MenuTrack *t = &tracks.items[i];
            if (x >= t->x && x < t->x+t->w && y >= t->y && y < t->y+t->h)
                track_hit = t;
        }
        if (track_hit && !track_hit->unlocked) return 0;
    }

    for (unsigned i = 0; i < 128; ++i) {
        MenuArrow *a = &arrows[i];
        if (a->button && GetTickCount64()-a->tick < 250 &&
            x >= a->x && x < a->x+a->w && y >= a->y && y < a->y+a->h)
            arrow_hit = a;
    }
    if (click && arrow_hit && arrow_hit->context == frame->root &&
        ((frame->car_node && arrow_hit->node == frame->car_node) ||
         (frame->color_node && arrow_hit->node == frame->color_node))) {
        queue_button(frame->root, arrow_hit->button);
        return 0;
    }
    GuestState s = save_guest();
    for (unsigned i = 0; i < frame->count; ++i) {
        MenuTab *tab = &frame->tabs[i];
        float area = tab->w * tab->h;
        int track_tab = track_hit && tab->context == tracks.context;
        /* Option paint belongs to the child style node, while the tab stop
         * belongs to its parent button. Associate their native context/row. */
        int arrow_tab = arrow_hit && tab->context == arrow_hit->context &&
            arrow_hit->y+arrow_hit->h*.5f >= tab->y && arrow_hit->y+arrow_hit->h*.5f < tab->y+tab->h;
        if (!(tab->w > 0 && tab->h > 0) || !isfinite(tab->x) || !isfinite(tab->y) ||
            !isfinite(area) || area > best_area ||
            (!track_tab && !arrow_tab && (x < tab->x || x >= tab->x + tab->w || y < tab->y || y >= tab->y + tab->h ||
            !over_control(tab, x, y)))) continue;
        g_esp = s.regs[3] - 0x100;
        PUSH32(g_esp, tab->context);
        if (call_virtual(tab->node, 0xC0) && (g_eax & 0xFF)) {
            hit = tab;
            best_area = area;
        }
    }
    if (hit) {
        float fx = hit->x + hit->w * 0.5f, fy = hit->y + hit->h * 0.5f;
        if (MEMF(hit->context+0x34) != fx || MEMF(hit->context+0x38) != fy) {
            changed_focus = 1;
            g_esp = s.regs[3] - 0x100;
            g_esp -= 4; MEMF(g_esp) = fy;
            g_esp -= 4; MEMF(g_esp) = fx;
            int applied = call_virtual(hit->context, 0x54);
            if (trace_native())
                fprintf(stderr, "[MENU_NATIVE_PICK] root=%08X context=%08X node=%08X point=%g,%g focus=%g,%g applied=%d tick=%llu\n",
                        frame->root, hit->context, hit->node, x, y, fx, fy, applied, GetTickCount64());
        }
    }
    restore_guest(&s);
    if (click && hit &&
        (MEMF(hit->context+0x34) != hit->x + hit->w*0.5f ||
         MEMF(hit->context+0x38) != hit->y + hit->h*0.5f)) return 0;
    /* Native nodes reconcile their focused state during layout, which has
     * already run this update. Activating now would hit the previous item. */
    if (click && hit && changed_focus) return 2;
    if (click && hit && arrow_hit && hit->context == arrow_hit->context &&
        arrow_hit->y+arrow_hit->h*.5f >= hit->y && arrow_hit->y+arrow_hit->h*.5f < hit->y+hit->h) {
        picked_button = arrow_hit->button;
        return 1;
    }
    if (click && hit && track_hit) {
        if (track_hit->index == tracks.current) return 1;
        track_click.node = tracks.node; track_click.root = frame->root;
        track_click.index = track_hit->index; track_click.previous = UINT32_MAX;
        track_click.remaining = 64; track_click.deadline = GetTickCount64()+250;
        return 0;
    }
    if (click && hit && frame->car_node && hit->context == frame->car_context) {
        /* A card click selects through the original left/right actions.
         * Clicking the current vehicle confirms. Ignore gaps in the strip. */
        if (cars.node != frame->car_node || cars.context != frame->car_context || cars.count > 64)
            return 0;
        MenuCar *picked_car = car_at_point(&cars, x, y);
        if (picked_car) {
            if (picked_car->index == MEM32(frame->car_node+0x28)) return 1;
            car_click.root = frame->root;
            car_click.node = frame->car_node;
            car_click.context = frame->car_context;
            car_click.index = picked_car->index;
            car_click.previous = UINT32_MAX;
            car_click.remaining = 64;
            return 0;
        }
        return 0;
    }
    return hit != NULL;
}

static void advance_car(MenuLayout *frame)
{
    if (!car_click.node || car_click.root != frame->root) return;
    if (frame->car_node != car_click.node || frame->car_context != car_click.context) {
        car_click.node = 0;
        return;
    }
    unsigned current = MEM32(car_click.node+0x28);
    if (!car_click.remaining || current == car_click.index || (int32_t)current < 0 ||
        (car_click.previous != UINT32_MAX &&
         (current == car_click.previous ||
          (car_click.previous < car_click.index ? current < car_click.previous || current > car_click.index
                                               : current > car_click.previous || current < car_click.index)))) {
        car_click.node = 0;
        return;
    }
    /* Re-read the native binding's target each UI update. Native actions can
     * skip unavailable vehicles; counting raw card indices would be wrong. */
    queue_button(frame->root, current < car_click.index ? 7 : 5);
    car_click.previous = current;
    car_click.remaining--;
}

static void advance_track(MenuLayout *frame)
{
    if (!track_click.node || track_click.root != frame->root) return;
    unsigned current = tracks.current;
    if (frame->race_node != track_click.node || tracks.node != track_click.node ||
        !track_click.remaining || current == track_click.index || GetTickCount64() > track_click.deadline) {
        track_click.node = 0;
        return;
    }
    /* The selected index is published by native paint. Wait for the queued
     * action to reach that binding before issuing another direction. */
    if (current == track_click.previous) return;
    queue_button(frame->root, current < track_click.index ? 7 : 5);
    track_click.previous = current;
    track_click.remaining--;
    track_click.deadline = GetTickCount64()+250;
}

static void advance_color(MenuLayout *frame)
{
    if (!color_click.node || color_click.root != frame->root) return;
    unsigned current = colors.current;
    if (frame->color_node != color_click.node || colors.node != color_click.node ||
        !color_click.remaining || current == color_click.index || GetTickCount64() > color_click.deadline) {
        color_click.node = 0;
        return;
    }
    if (current == color_click.previous) return;
    queue_button(frame->root, current < color_click.index ? 8 : 6);
    color_click.previous = current;
    color_click.remaining--;
    color_click.deadline = GetTickCount64()+250;
}

void sub_001C5935(void)
{
    if (!enabled()) { sub_001C5935_original(); return; }
    MenuLayout frame = {0};
    MenuLayout *parent = layout;
    frame.root = g_ecx;
    layout = &frame;
    sub_001C5935_original();
    layout = parent;
}

void sub_000C2FA7(void)
{
    if (layout) {
        uint32_t rect = MEM32(g_esp+12);
        unsigned i = layout->count++;
        if (i < 64) {
            MenuTab *tab = &layout->tabs[i];
            tab->node = g_ecx;
            tab->context = MEM32(g_esp+4);
            tab->x = MEMF(rect); tab->y = MEMF(rect+4);
            tab->w = MEMF(rect+8); tab->h = MEMF(rect+12);
        }
    }
    sub_000C2FA7_original();
}

void sub_00246894(void)
{
    if (layout && layout->count && layout->root == MEM32(g_esp+8)) {
        if (trace_native()) {
            static RECOMP_TLS uint32_t previous_root, previous_node;
            static RECOMP_TLS unsigned previous_count, reports;
            uint32_t first = layout->count <= 64 ? layout->tabs[0].node : 0;
            if ((previous_root != layout->root || previous_count != layout->count || previous_node != first) && reports++ < 64) {
                previous_root = layout->root; previous_node = first; previous_count = layout->count;
                fprintf(stderr, "[MENU_NATIVE_LAYOUT] root=%08X tabs=%u car=%08X\n", layout->root, layout->count, layout->car_node);
                for (unsigned i = 0; i < layout->count && i < 64; ++i) {
                    MenuTab *t = &layout->tabs[i];
                    fprintf(stderr, "[MENU_NATIVE_TAB] node=%08X context=%08X rect=%g,%g,%g,%g\n", t->node, t->context, t->x, t->y, t->w, t->h);
                }
            }
        }
        MenuClick click;
        int clicked = take_click(&click);
        if (clicked) {
            car_click.node = 0;
            track_click.node = 0;
            color_click.node = 0;
            deferred_click.root = 0;
        } else if (deferred_click.root == layout->root) {
            click = deferred_click.click;
            clicked = 1;
            deferred_click.root = 0;
        }
        int picked = pick_native(layout, clicked ? &click : NULL);
        if (clicked && picked == 2) {
            deferred_click.root = layout->root;
            deferred_click.click = click;
        } else if (clicked && picked == 1) {
            queue_button(layout->root, picked_button);
        }
        advance_car(layout);
        advance_track(layout);
        advance_color(layout);
    }
    sub_00246894_original();
}

void sub_001118C7(void)
{
    if (layout) {
        layout->car_node = g_ecx;
        layout->car_context = MEM32(g_esp+4);
    }
    sub_001118C7_original();
}

void sub_00103E41(void)
{
    if (layout) layout->race_node = g_ecx;
    sub_00103E41_original();
}

void sub_00103FBD(void)
{
    if (!enabled()) { sub_00103FBD_original(); return; }
    MenuTracks next = {0};
    MenuTracks *parent = painting_tracks;
    next.node = g_ecx; next.context = MEM32(g_esp+4);
    painting_tracks = &next;
    sub_00103FBD_original();
    painting_tracks = parent;
    next.tick = GetTickCount64();
    tracks = next;
}

void sub_001038D0(void)
{
    MenuTrack *parent = painting_track_item;
    painting_track_item = NULL;
    if (painting_tracks) {
        unsigned i = painting_tracks->count++;
        if (i < 64) {
            MenuTrack *t = &painting_tracks->items[i];
            t->index = MEM32(g_esp+24);
            t->unlocked = MEM32(MEM32(MEM32(g_esp+20))+0xC) < MEM32(g_esp+36);
            t->x = MEMF(g_esp+44); t->y = MEMF(g_esp+40);
            t->w = MEMF(g_esp+48); t->h = MEMF(g_esp+52);
            painting_track_item = t;
            if (MEM32(g_esp) == 0x00103E3B) painting_tracks->current = t->index;
            if (trace_native()) {
                static unsigned reports;
                if (reports++ < 32)
                    fprintf(stderr, "[MENU_NATIVE_TRACK] index=%u unlocked=%u rect=%g,%g,%g,%g\n",
                        t->index, t->unlocked, t->x, t->y, t->w, t->h);
            }
        }
    }
    sub_001038D0_original();
    painting_track_item = parent;
}

void sub_00111CBF(void)
{
    if (!enabled()) { sub_00111CBF_original(); return; }
    MenuCars next;
    MenuCars *parent = painting_cars;
    next.node = g_ecx;
    next.context = MEM32(g_esp+4);
    next.count = 0;
    painting_cars = &next;
    sub_00111CBF_original();
    painting_cars = parent;
    cars.node = next.node; cars.context = next.context; cars.count = next.count;
    if (next.count <= 64)
        memcpy(cars.cards, next.cards, next.count * sizeof next.cards[0]);
    if (trace_native()) {
        static unsigned reports;
        if (reports++ < 8)
            fprintf(stderr, "[MENU_NATIVE_CARS] node=%08X context=%08X cards=%u\n",
                    cars.node, cars.context, cars.count);
    }
}

static int over_native_rect(uint32_t context, float x, float y, float w, float h)
{
    float mx, my;
    xbox_FramebufferMouse(&mx, &my);
    return enabled() && w > 0 && h > 0 && menu_point(context, &mx, &my) &&
        mx >= x && mx < x+w && my >= y && my < y+h;
}

void sub_0010553A(void)
{
    MenuSwatch next = {0}, *parent = painting_swatch;
    uint32_t quad = MEM32(g_esp+8), paint = MEM32(g_esp+12);
    if (painting_colors) {
        next.index = MEM32(MEM32(g_esp+16));
        if (MEM32(g_esp) == 0x0010522B) painting_colors->current = next.index+2;
        next.x = MEMF(quad+0x0C); next.y = MEMF(quad+0x10);
        next.w = MEMF(quad+0x30)-next.x; next.h = MEMF(quad+0x58)-next.y;
        painting_swatch = &next;
    }
    sub_0010553A_original();
    painting_swatch = parent;
    if (painting_colors && (g_eax & 0xFF)) {
        unsigned i = painting_colors->count++;
        if (i < 8) painting_colors->items[i] = next;
        if (!next.locked && next.index != painting_colors->current &&
            over_native_rect(painting_colors->context, next.x, next.y, next.w, next.h)) {
            /* Reuse the native selected-swatch ring, with orange RGB. */
            GuestState state = save_guest();
            float rgb[12];
            for (unsigned v = 0; v < 4; ++v) for (unsigned c = 0; c < 3; ++c) {
                uint32_t color = quad+0x20+v*0x24+c*4;
                rgb[v*3+c] = MEMF(color);
                MEMF(color) = menu_orange[c];
            }
            g_esp = state.regs[3]-0x100;
            PUSH32(g_esp, quad);
            PUSH32(g_esp, MEM32(painting_colors->node+0x14));
            PUSH32(g_esp, 0);
            g_eax = paint; g_edx = painting_colors->context;
            sub_001C1585_original();
            for (unsigned v = 0; v < 4; ++v) for (unsigned c = 0; c < 3; ++c)
                MEMF(quad+0x20+v*0x24+c*4) = rgb[v*3+c];
            restore_guest(&state);
        }
    }
}

void sub_001C44F8(void)
{
    MenuPrompt *p = painting_prompt;
    if (p && MEM32(g_esp) == 0x0010E173 &&
        over_native_rect(p->context, p->x, p->y, p->w, p->h)) {
        /* The help painter supplies a local Float4Data colour in its native
         * text descriptor. Change only RGB, retaining alpha and font styling. */
        uint32_t color = MEM32(MEM32(g_esp+12)+4);
        if (MEM32(color) == 0x0037D478) {
            float saved[3] = {MEMF(color+4), MEMF(color+8), MEMF(color+12)};
            MEMF(color+4) = 1; MEMF(color+8) = .95f; MEMF(color+12) = .80f;
            sub_001C44F8_original();
            for (unsigned i = 0; i < 3; ++i) MEMF(color+4+i*4) = saved[i];
            return;
        }
    }
    sub_001C44F8_original();
}

void sub_001C1585(void)
{
    uint32_t site = MEM32(g_esp);
    int hovered = 0, track_hover = 0, orange_arrow_hover = 0;
    if (painting_swatch && site == 0x001056D3) painting_swatch->locked = 1;
    if (enabled() && (site == 0x0010BF7A || site == 0x0010BFE5 ||
        (painting_colors && (site == 0x00105410 || site == 0x001054BE)) ||
        (painting_cars && (site == 0x001125C8 || site == 0x0011263F)))) {
        /* Native option painter emits only directions allowed by its binding.
         * Both standard and centred option buttons share this painter. */
        uint32_t quad = MEM32(g_esp+8);
        uint32_t node = painting_colors ? painting_colors->node : painting_cars ? painting_cars->node : g_ebx;
        unsigned button = site == 0x00105410 ? 6 : site == 0x001054BE ? 8 :
            site == 0x0010BF7A || site == 0x001125C8 ? 5 : 7;
        MenuArrow a = {node, g_edx, MEMF(quad+0x0C), MEMF(quad+0x10),
            MEMF(quad+0x30)-MEMF(quad+0x0C), MEMF(quad+0x58)-MEMF(quad+0x10),
            button, GetTickCount64()};
        unsigned target = 0;
        for (unsigned i = 0; i < 128; ++i) {
            if (arrows[i].node == a.node && arrows[i].button == a.button) { target = i; break; }
            if (arrows[i].tick < arrows[target].tick) target = i;
        }
        arrows[target] = a;
        if (trace_native()) {
            static unsigned reports;
            if (reports++ < 16) fprintf(stderr, "[MENU_NATIVE_ARROW] node=%08X context=%08X button=%u rect=%g,%g,%g,%g\n",
                a.node, a.context, a.button, a.x, a.y, a.w, a.h);
        }
        hovered = over_native_rect(a.context, a.x, a.y, a.w, a.h);
        orange_arrow_hover = hovered && ((painting_cars && (site == 0x001125C8 || site == 0x0011263F)) ||
            (painting_colors && (site == 0x00105410 || site == 0x001054BE)));
    }
    if (painting_prompt && (site == 0x0010E000 || site == 0x0010E03F || site == 0x0010E084)) {
        uint32_t quad = MEM32(g_esp+8);
        float x = MEMF(quad+0x0C), y = MEMF(quad+0x10);
        float right = MEMF(quad+0x30), bottom = MEMF(quad+0x58);
        MenuPrompt *p = painting_prompt;
        if (site == 0x0010E000) { p->x = x; p->y = y; p->w = right-x; p->h = bottom-y; }
        else {
            float left = fminf(p->x, x), top = fminf(p->y, y);
            p->w = fmaxf(p->x+p->w, right)-left;
            p->h = fmaxf(p->y+p->h, bottom)-top;
            p->x = left; p->y = top;
        }
    }
    if (site == 0x00103AFE && painting_track_item) {
        uint32_t quad = MEM32(g_esp+8);
        painting_track_item->x = MEMF(quad+0x0C);
        painting_track_item->y = MEMF(quad+0x10);
        painting_track_item->w = MEMF(quad+0x30) - painting_track_item->x;
        painting_track_item->h = MEMF(quad+0x58) - painting_track_item->y;
    }
    if (site == 0x00103ACE && painting_track_item && painting_tracks &&
        tracks.node == painting_tracks->node && tracks.count <= 64 && GetTickCount64()-tracks.tick < 250) {
        for (unsigned i = 0; i < tracks.count; ++i) {
            MenuTrack *t = &tracks.items[i];
            if (t->index == painting_track_item->index && t->unlocked && painting_track_item->unlocked &&
                over_native_rect(painting_tracks->context, t->x, t->y, t->w, t->h))
                hovered = track_hover = 1;
        }
    }
    if ((site == 0x0010C2E2 || site == 0x0010C322 || site == 0x0010C35E) && enabled()) {
        uint32_t quad = MEM32(g_esp+8), frame = quad+0x64;
        uint32_t rect = MEM32(frame+0x6C);
        float x = MEMF(quad+0x0C), y = MEMF(quad+0x10);
        float right = MEMF(quad+0x30), bottom = MEMF(quad+0x58);
        if (site == 0x0010C2E2)
            painting_control = (MenuControl){g_edx, MEMF(rect+4), MEMF(rect+12), x, y, right-x, bottom-y, GetTickCount64()};
        else if (painting_control.context == g_edx) {
            float left = fminf(painting_control.x, x), top = fminf(painting_control.y, y);
            painting_control.w = fmaxf(painting_control.x+painting_control.w, right)-left;
            painting_control.h = fmaxf(painting_control.y+painting_control.h, bottom)-top;
            painting_control.x = left; painting_control.y = top;
        }
        if (site == 0x0010C35E && painting_control.context == g_edx) {
            unsigned target = 0;
            for (unsigned i = 0; i < 64; ++i) {
                if (controls[i].context == g_edx && controls[i].row_y == painting_control.row_y) { target = i; break; }
                if (controls[i].tick < controls[target].tick) target = i;
            }
            controls[target] = painting_control;
            if (trace_native()) {
                static unsigned reports;
                if (reports++ < 32)
                    fprintf(stderr, "[MENU_NATIVE_CONTROL] context=%08X row=%g,%g rect=%g,%g,%g,%g\n",
                        g_edx, painting_control.row_y, painting_control.row_h,
                        painting_control.x, painting_control.y, painting_control.w, painting_control.h);
            }
        }
    }
    /* These are the car images in the widget's two directional loops.
     * Shadows, locked badges and arrows use other call sites. */
    uint32_t caller = painting_cars ? MEM32(g_esp) : 0;
    if (caller == 0x001120F3 || caller == 0x00112456) {
        uint32_t rect = MEM32(g_esp+8);
        /* Both native call sites pass their local quad at EBP-0xB4.
         * Generated code keeps EBP in a C local, not the global g_ebp. */
        uint32_t frame = rect + 0xB4, begin = MEM32(frame-0x10);
        uint32_t end = MEM32(frame-0x0C), item = MEM32(frame+0x34);
        if (item >= begin && item < end && !((item-begin) & 3)) {
            unsigned i = painting_cars->count++;
            if (i < 64) {
                MenuCar *card = &painting_cars->cards[i];
                card->x = MEMF(rect+0x0C); card->y = MEMF(rect+0x10);
                card->w = MEMF(rect+0x30) - card->x;
                card->h = MEMF(rect+0x58) - card->y;
                card->index = (item-begin) / 4;
                card->unlocked = caller == 0x001120F3 ? !MEM8(frame+0x63) : !(g_ebx & 0xFF);
            }
        }
    }
    if (hovered) {
        uint32_t quad = MEM32(g_esp+8);
        float saved[12];
        for (unsigned v = 0; v < 4; ++v) for (unsigned c = 0; c < 3; ++c) {
            uint32_t color = quad+0x20+v*0x24+c*4;
            saved[v*3+c] = MEMF(color);
            /* Track paths are natively gold-tinted monochrome textures;
             * pale gold on hover keeps the original frame and silhouette. */
            MEMF(color) = orange_arrow_hover ? menu_orange[c] :
                track_hover ? (c == 2 ? .6f : 1.f) : MEMF(color)+(1-MEMF(color))*.65f;
        }
        sub_001C1585_original();
        for (unsigned v = 0; v < 4; ++v) for (unsigned c = 0; c < 3; ++c)
            MEMF(quad+0x20+v*0x24+c*4) = saved[v*3+c];
        return;
    }
    sub_001C1585_original();
}
