/* Exercises the toolkit hooks through mm3's real SDL3 implementations. */
#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xinput_xbox.h"
#include "fb_present.h"
#include "nv2a_d3d11.h"
#include "video_player.h"
#include "kernel.h"
#include "nv2a_pb.h"
#include "icall_diagnostics.h"
#include "apu_xaudio2.h"

typedef void (*recomp_func_t)(void);
recomp_func_t recomp_lookup(uint32_t va) { (void)va; return NULL; }
recomp_func_t recomp_lookup_manual(uint32_t va) { (void)va; return NULL; }
extern void mm3_sdl3_install_pads(void);

static int fails, skips;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); ++fails; } } while (0)
#define PASS(n) printf("PASS %s\n", n)

static void on_close(void) {}
static XboxCursor on_cursor(void) { return NULL; }
static int on_press(float x, float y) { (void)x; (void)y; return 0; }
static void on_present(unsigned f) { (void)f; }
static void on_stop(uint32_t a, uint32_t b, const char *r) { (void)a; (void)b; (void)r; }
static void on_icall(const XboxIcallReport *r) { (void)r; }
static int on_scsi(uint32_t v, uint32_t l, uint32_t *i) { (void)v; (void)l; (void)i; return 0; }
static void on_override(void) {}
static void on_kbd(XBOX_INPUT_STATE *s, BOOL m) { (void)s; (void)m; }
static int in_menu(void) { return 0; }

int main(void)
{
    _putenv("SDL_AUDIO_DRIVER=dummy");

    /* --- pads: a virtual SDL gamepad through mm3's provider and the toolkit's shared logic */
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT, "0x1209/0xF00D");   /* only the virtual pad */
    mm3_sdl3_install_pads();
    SDL_Init(SDL_INIT_GAMEPAD);
    {
        SDL_VirtualJoystickDesc d;
        SDL_JoystickID id;
        SDL_INIT_INTERFACE(&d);
        d.type = SDL_JOYSTICK_TYPE_GAMEPAD;
        d.vendor_id = 0x1209;
        d.product_id = 0xF00D;
        d.naxes = SDL_GAMEPAD_AXIS_COUNT;
        d.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
        id = SDL_AttachVirtualJoystick(&d);
        CHECK(id != 0);
        SDL_Joystick *j = SDL_OpenJoystick(id);
        CHECK(j != NULL);
        SDL_Delay(30); SDL_UpdateGamepads();
        SDL_SetJoystickVirtualButton(j, SDL_GAMEPAD_BUTTON_SOUTH, true);
        SDL_SetJoystickVirtualAxis(j, SDL_GAMEPAD_AXIS_LEFTX, 20000);
        SDL_SetJoystickVirtualAxis(j, SDL_GAMEPAD_AXIS_LEFTY, -32768);
        SDL_UpdateGamepads();
        SDL_Delay(30); SDL_UpdateGamepads();
        SDL_UpdateGamepads();
        xbox_InputInit();
        XBOX_INPUT_STATE st;
        DWORD r = xbox_InputGetState(0, &st);
        CHECK(r == ERROR_SUCCESS);
        if (r == ERROR_SUCCESS) {
            CHECK(st.Gamepad.bAnalogButtons[XBOX_BUTTON_A] == 255);
            CHECK(st.Gamepad.sThumbLX == 20000 && st.Gamepad.sThumbLY == 32767);   /* Y flips: SDL points down */
            PASS("pads: SDL3 virtual gamepad -> provider -> Xbox pad (A, left stick X/Y)");
        }
        uint16_t b; uint8_t lt, rt; int16_t lx, ly, rx, ry;
        CHECK(xbox_InputHostPad(0, &b, &lt, &rt, &lx, &ly, &rx, &ry) == 0);
        CHECK(lx == 20000 && ly == 32767);
        CHECK(xbox_InputSetState(0, &(XBOX_VIBRATION){1000, 1000}) == ERROR_SUCCESS);
        CHECK(xbox_InputIsConnected(0));
        PASS("pads: xbox_InputHostPad, rumble, connected");
    }
    xbox_InputSetKeyboardHook(on_kbd);
    xbox_InputSetMenuProbe(in_menu);
    xbox_InputSetMenuClickHandler(NULL);
    PASS("input hooks register");

    /* --- audio device (SDL dummy driver) */
    CHECK(xa2_init() == 1);
    {
        int16_t buf[512 * 2];
        memset(buf, 0, sizeof buf);
        CHECK(xa2_is_active());
        CHECK(xa2_submit_samples(buf, 512));
        CHECK(xa2_queued_samples() >= 0);
        CHECK(xa2_get_buffer_size() > 0 && xa2_get_buffer_count() > 0);
        xa2_shutdown();
        PASS("audio: xa2_* on SDL3 (dummy driver)");
    }

    /* --- window / GPU renderer */
    xbox_FramebufferSetCloseHandler(on_close);
    xbox_FramebufferSetCursorHandler(on_cursor);
    xbox_FramebufferSetMousePressHandler(on_press);
    xbox_FramebufferSetHostOverlay(NULL);
    xbox_VideoSetPresentHandler(on_present);
    PASS("window / video handlers register");
    if (nv2a_d3d_init()) {
        CHECK(nv2a_d3d_active());
        CHECK(nv2a_d3d_display_aspect() >= 1.0f);
        PASS("renderer: nv2a_d3d_init created an SDL_GPU device");
    } else {
        printf("SKIP renderer: no GPU device here (%s)\n", SDL_GetError());
        ++skips;
    }

    /* --- kernel / NV2A hooks */
    nv2a_pb_set_stop_handler(on_stop);
    xbox_icall_set_report_hook(on_icall);
    xbox_kernel_set_dvd_scsi_hook(on_scsi);
    xbox_kernel_set_inline_event_callers(NULL, 0);
    PASS("kernel / NV2A hooks register");
    (void)on_override;

    printf("%s: %d failure(s), %d skipped\n", fails ? "FAILED" : "OK", fails, skips);
    return fails ? 1 : 0;
}
