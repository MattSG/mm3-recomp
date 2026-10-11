/* Exercise the real keyboard window handler, XInput translation and USB
 * report encoder without running MM3 or requiring a physical controller. */
#include <windows.h>
#include <xinput.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static XINPUT_STATE host_state;
static BOOL host_connected;
static BOOL driving_context;

static BOOL fixture_driving(void) { return driving_context; }

static DWORD WINAPI fixture_get_state(DWORD port, XINPUT_STATE *state)
{
    if (!host_connected || port != 0) return ERROR_DEVICE_NOT_CONNECTED;
    *state = host_state;
    return ERROR_SUCCESS;
}

static DWORD WINAPI fixture_set_state(DWORD port, XINPUT_VIBRATION *vibration)
{
    (void)port; (void)vibration;
    return ERROR_SUCCESS;
}

static DWORD WINAPI fixture_get_caps(DWORD port, DWORD flags,
                                    XINPUT_CAPABILITIES *caps)
{
    (void)port; (void)flags;
    memset(caps, 0, sizeof(*caps));
    return ERROR_SUCCESS;
}

#define XInputGetState fixture_get_state
#define XInputSetState fixture_set_state
#define XInputGetCapabilities fixture_get_caps
#include "../tools/xboxrecomp/src/input/xinput_device.c"
#include "../tools/xboxrecomp/src/video/fb_present.c"
#include "../tools/xboxrecomp/src/usb/usb_gamepad.c"

/* Rendering is outside this fixture. No framebuffer thread is started. */
ptrdiff_t xbox_GetMemoryOffset(void) { return 0; }
bool nv2a_hook_pvideo_snapshot(uint32_t regs[0x1000 / 4])
{
    (void)regs; return false;
}
void nv2a_hook_pvideo_consume(unsigned bank) { (void)bank; }
float nv2a_d3d_display_aspect(void) { return 4.0f / 3.0f; }

static unsigned checks, failures;
#define CHECK(condition) do { \
    ++checks; \
    if (!(condition)) { \
        fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

static void report(uint8_t out[20])
{
    CHECK(usb_gamepad_report(0, out, 20) == 20);
    CHECK(out[0] == 0 && out[1] == 20 && out[3] == 0);
}

static int16_t report_axis(const uint8_t out[20], int offset)
{
    return (int16_t)((uint16_t)out[offset] | ((uint16_t)out[offset + 1] << 8));
}

static void expect_triggers(unsigned left, unsigned right)
{
    uint8_t out[20];
    report(out);
    CHECK(out[10] == left);
    CHECK(out[11] == right);
}

static void key(HWND window, int vk, BOOL down)
{
    SendMessageA(window, down ? WM_KEYDOWN : WM_KEYUP, (WPARAM)vk, 0);
}

static void check_host_report(void)
{
    static const uint8_t pressures[] = {0, 1, 29, 30, 31, 127, 128, 254, 255};
    uint8_t out[20], short_report[20];
    unsigned i, j;
    host_connected = TRUE;
    memset(&host_state, 0, sizeof(host_state));
    /* No deadzone, truncation or coupling may be introduced between triggers. */
    for (i = 0; i < sizeof pressures; ++i) {
        for (j = 0; j < sizeof pressures; ++j) {
            host_state.Gamepad.bLeftTrigger = pressures[i];
            host_state.Gamepad.bRightTrigger = pressures[j];
            expect_triggers(pressures[i], pressures[j]);
        }
    }
    memset(&host_state, 0, sizeof(host_state));
    host_state.Gamepad.wButtons = XINPUT_GAMEPAD_RIGHT_SHOULDER;
    report(out);
    CHECK(out[8] == 255 && out[9] == 0); /* RB is Xbox Black. */
    host_state.Gamepad.wButtons = XINPUT_GAMEPAD_LEFT_SHOULDER;
    report(out);
    CHECK(out[8] == 0 && out[9] == 255); /* LB is Xbox White. */
    host_state.Gamepad.wButtons = XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_B |
        XINPUT_GAMEPAD_X | XINPUT_GAMEPAD_Y | XINPUT_GAMEPAD_START |
        XINPUT_GAMEPAD_DPAD_RIGHT | XINPUT_GAMEPAD_LEFT_THUMB;
    host_state.Gamepad.sThumbLX = -32768;
    host_state.Gamepad.sThumbLY = 32767;
    host_state.Gamepad.sThumbRX = -12345;
    host_state.Gamepad.sThumbRY = 12345;
    report(out);
    CHECK(out[2] == (XBOX_GAMEPAD_START | XBOX_GAMEPAD_DPAD_RIGHT |
                     XBOX_GAMEPAD_LEFT_THUMB));
    CHECK(out[4] == 255 && out[5] == 255 && out[6] == 255 && out[7] == 255);
    CHECK(report_axis(out, 12) == -32768 && report_axis(out, 14) == 32767);
    CHECK(report_axis(out, 16) == -12345 && report_axis(out, 18) == 12345);
    memset(short_report, 0xA5, sizeof short_report);
    CHECK(usb_gamepad_report(0, short_report, 19) == 0);
    for (i = 0; i < sizeof short_report; ++i) CHECK(short_report[i] == 0xA5);
    host_connected = FALSE;
}

static void check_keyboard(HWND window, BOOL mm3)
{
    uint8_t out[20];
    XBOX_INPUT_STATE before, after;
    unsigned i;
    expect_triggers(0, 0);
    key(window, '3', TRUE);
    for (i = 0; i < 5; ++i) expect_triggers(0, 255);
    CHECK(xbox_InputGetState(0, &before) == ERROR_SUCCESS);
    key(window, '3', FALSE);
    CHECK(xbox_InputGetState(0, &after) == ERROR_SUCCESS);
    CHECK(before.dwPacketNumber != after.dwPacketNumber);
    expect_triggers(0, 0);
    key(window, '1', TRUE); expect_triggers(255, 0);
    key(window, '1', FALSE); expect_triggers(0, 0);

    key(window, VK_UP, TRUE);
    for (i = 0; i < 5; ++i) expect_triggers(0, mm3 ? 255 : 0);
    report(out);
    CHECK((out[2] & XBOX_GAMEPAD_DPAD_UP) == (mm3 ? 0 : XBOX_GAMEPAD_DPAD_UP));
    key(window, '3', TRUE);
    key(window, VK_UP, FALSE); expect_triggers(0, 255);
    key(window, '3', FALSE); expect_triggers(0, 0);
    key(window, VK_DOWN, TRUE);
    expect_triggers(mm3 ? 255 : 0, 0);
    report(out);
    CHECK((out[2] & XBOX_GAMEPAD_DPAD_DOWN) == (mm3 ? 0 : XBOX_GAMEPAD_DPAD_DOWN));
    key(window, VK_DOWN, FALSE); expect_triggers(0, 0);

    key(window, VK_LEFT, TRUE);
    report(out);
    CHECK(report_axis(out, 12) == (mm3 ? -32767 : 0));
    CHECK((out[2] & XBOX_GAMEPAD_DPAD_LEFT) == (mm3 ? 0 : XBOX_GAMEPAD_DPAD_LEFT));
    key(window, VK_NUMPAD4, TRUE);
    report(out); CHECK(report_axis(out, 12) == -32767);
    key(window, VK_RIGHT, TRUE);
    report(out); CHECK(report_axis(out, 12) == (mm3 ? 0 : -32767));
    key(window, VK_LEFT, FALSE);
    report(out); CHECK(report_axis(out, 12) == (mm3 ? 0 : -32767));
    key(window, VK_NUMPAD4, FALSE);
    report(out); CHECK(report_axis(out, 12) == (mm3 ? 32767 : 0));
    key(window, VK_NUMPAD6, TRUE);
    report(out); CHECK(report_axis(out, 12) == 32767);
    key(window, VK_RIGHT, FALSE);
    key(window, VK_NUMPAD4, TRUE);
    report(out); CHECK(report_axis(out, 12) == 0);
    key(window, VK_NUMPAD4, FALSE); key(window, VK_NUMPAD6, FALSE);
    key(window, VK_NUMPAD8, TRUE);
    report(out); CHECK(report_axis(out, 14) == 32767);
    key(window, VK_NUMPAD8, FALSE);

    /* A focused keyboard press must supplement, then release back to, the
     * host pad's partial throttle rather than latch or erase its pressure. */
    host_connected = TRUE;
    memset(&host_state, 0, sizeof(host_state));
    host_state.Gamepad.bLeftTrigger = 37;
    host_state.Gamepad.bRightTrigger = 128;
    key(window, '3', TRUE); expect_triggers(37, 255);
    key(window, '3', FALSE); expect_triggers(37, 128);
    host_connected = FALSE;

    key(window, '3', TRUE); key(window, '1', TRUE);
    key(window, VK_LEFT, TRUE); key(window, VK_NUMPAD6, TRUE);
    SendMessageA(window, WM_KILLFOCUS, 0, 0);
    report(out);
    for (i = 2; i < sizeof out; ++i) CHECK(out[i] == 0);
    CHECK(xbox_FramebufferKeyDown('3') == 0);
    CHECK(xbox_FramebufferKeyDown(256) == 0);
}

static void check_context(HWND window, BOOL mm3)
{
    uint8_t out[20];
    /* Until a title registers a probe, arrows are only navigation keys. */
    xbox_InputSetKeyboardDrivingProbe(NULL);
    key(window, VK_UP, TRUE); key(window, VK_DOWN, TRUE);
    key(window, VK_LEFT, TRUE); key(window, VK_RIGHT, TRUE);
    report(out);
    CHECK((out[2] & 0x0F) == 0x0F);
    CHECK(out[10] == 0 && out[11] == 0 && report_axis(out, 12) == 0);

    xbox_InputSetKeyboardDrivingProbe(fixture_driving);
    driving_context = FALSE;
    report(out);
    CHECK((out[2] & 0x0F) == 0x0F);
    CHECK(out[10] == 0 && out[11] == 0 && report_axis(out, 12) == 0);
    driving_context = TRUE;
    report(out);
    CHECK((out[2] & 0x0F) == (mm3 ? 0 : 0x0F));
    CHECK(out[10] == (mm3 ? 255 : 0) && out[11] == (mm3 ? 255 : 0));
    CHECK(report_axis(out, 12) == 0); /* Opposing arrows cancel. */

    key(window, VK_DOWN, FALSE); key(window, VK_RIGHT, FALSE);
    report(out);
    CHECK((out[2] & 0x0F) == (mm3 ? 0 : 0x05));
    CHECK(out[10] == 0 && out[11] == (mm3 ? 255 : 0));
    CHECK(report_axis(out, 12) == (mm3 ? -32767 : 0));
    /* Pausing while keys remain held immediately switches back to menus. */
    driving_context = FALSE;
    report(out);
    CHECK((out[2] & 0x0F) == 0x05);
    CHECK(out[10] == 0 && out[11] == 0 && report_axis(out, 12) == 0);
    key(window, '3', TRUE); key(window, '1', TRUE); key(window, VK_NUMPAD6, TRUE);
    report(out);
    CHECK(out[10] == 255 && out[11] == 255 && report_axis(out, 12) == 32767);
    key(window, '3', FALSE); key(window, '1', FALSE); key(window, VK_NUMPAD6, FALSE);
    key(window, VK_UP, FALSE); key(window, VK_LEFT, FALSE);

    /* Driving suppresses keyboard d-pad aliases, never physical pad buttons. */
    driving_context = TRUE;
    host_connected = TRUE;
    memset(&host_state, 0, sizeof(host_state));
    host_state.Gamepad.wButtons = XINPUT_GAMEPAD_DPAD_RIGHT;
    key(window, VK_LEFT, TRUE);
    report(out);
    CHECK((out[2] & 0x0F) == (mm3 ? 0x08 : 0x0C));
    CHECK(report_axis(out, 12) == (mm3 ? -32767 : 0));
    key(window, VK_LEFT, FALSE);
    host_connected = FALSE;
}

static void check_gears(HWND window, BOOL mm3)
{
    uint8_t out[20];
    unsigned i;
    driving_context = FALSE;
    key(window, 'R', TRUE); key(window, 'F', TRUE);
    report(out); CHECK(out[5] == 0 && out[6] == 0);
    driving_context = TRUE;
    report(out);
    CHECK(out[5] == (mm3 ? 255 : 0) && out[6] == (mm3 ? 255 : 0));
    CHECK(out[2] == 0 && out[4] == 0 && out[7] == 0);
    key(window, 'F', FALSE); key(window, '3', TRUE);
    for (i = 0; i < 3; ++i) {
        report(out);
        CHECK(out[5] == (mm3 ? 255 : 0) && out[6] == 0);
        CHECK(out[10] == 0 && out[11] == 255);
    }
    key(window, 'R', FALSE); key(window, '3', FALSE);
    report(out); CHECK(out[5] == 0 && out[6] == 0 && out[11] == 0);
    key(window, 'F', TRUE);
    report(out); CHECK(out[5] == 0 && out[6] == (mm3 ? 255 : 0));
    key(window, 'F', FALSE);
    report(out); CHECK(out[5] == 0 && out[6] == 0);

    /* Releasing an alias must retain the original face-button key. */
    key(window, 'X', TRUE); key(window, 'A', TRUE);
    key(window, 'R', TRUE); key(window, 'F', TRUE);
    report(out); CHECK(out[5] == 255 && out[6] == 255);
    key(window, 'R', FALSE); key(window, 'F', FALSE);
    report(out); CHECK(out[5] == 255 && out[6] == 255);
    driving_context = FALSE;
    report(out); CHECK(out[5] == 255 && out[6] == 255);
    key(window, 'X', FALSE); key(window, 'A', FALSE);
    report(out); CHECK(out[5] == 0 && out[6] == 0);
    driving_context = TRUE;
    key(window, 'R', TRUE); key(window, 'F', TRUE);
    driving_context = FALSE; /* Pause with gear keys still held. */
    report(out); CHECK(out[5] == 0 && out[6] == 0);
    driving_context = TRUE;
    SendMessageA(window, WM_KILLFOCUS, 0, 0);
    report(out); CHECK(out[5] == 0 && out[6] == 0);
    CHECK(xbox_FramebufferKeyDown('R') == 0 && xbox_FramebufferKeyDown('F') == 0);
}

static void check_camera(HWND window, BOOL mm3)
{
    uint8_t out[20];
    XBOX_INPUT_STATE before, after;
    unsigned i, held;
    driving_context = FALSE;
    key(window, 'C', TRUE);
    report(out);
    for (i = 2; i < sizeof out; ++i) CHECK(out[i] == 0);
    xbox_InputSetKeyboardDrivingProbe(NULL);
    report(out); CHECK(out[8] == 0);
    xbox_InputSetKeyboardDrivingProbe(fixture_driving);

    driving_context = TRUE;
    for (held = 0; held < 3; ++held) {
        report(out);
        CHECK(out[8] == (mm3 ? 255 : 0)); /* MM3 camera is Xbox Black. */
        for (i = 2; i < sizeof out; ++i) if (i != 8) CHECK(out[i] == 0);
    }
    key(window, '3', TRUE);
    report(out); CHECK(out[8] == (mm3 ? 255 : 0) && out[11] == 255);
    CHECK(out[10] == 0); /* Changing the camera cannot apply the brake. */
    CHECK(xbox_InputGetState(0, &before) == ERROR_SUCCESS);
    key(window, 'C', FALSE);
    CHECK(xbox_InputGetState(0, &after) == ERROR_SUCCESS);
    CHECK(before.dwPacketNumber != after.dwPacketNumber);
    report(out); CHECK(out[8] == 0 && out[11] == 255);
    key(window, '3', FALSE);

    /* Releasing C must retain the original E key or physical right bumper. */
    key(window, 'E', TRUE); key(window, 'C', TRUE);
    report(out); CHECK(out[8] == 255);
    key(window, 'C', FALSE);
    report(out); CHECK(out[8] == 255);
    driving_context = FALSE;
    report(out); CHECK(out[8] == 255);
    key(window, 'E', FALSE);
    report(out); CHECK(out[8] == 0);
    driving_context = TRUE;
    host_connected = TRUE;
    memset(&host_state, 0, sizeof(host_state));
    host_state.Gamepad.wButtons = XINPUT_GAMEPAD_RIGHT_SHOULDER;
    key(window, 'C', TRUE);
    report(out); CHECK(out[8] == 255 && out[9] == 0);
    key(window, 'C', FALSE);
    report(out); CHECK(out[8] == 255);
    host_connected = FALSE;

    key(window, 'C', TRUE);
    driving_context = FALSE; /* Pausing with C held cannot send menu input. */
    report(out);
    for (i = 2; i < sizeof out; ++i) CHECK(out[i] == 0);
    driving_context = TRUE;
    key(window, 'E', TRUE);
    SendMessageA(window, WM_KILLFOCUS, 0, 0);
    report(out);
    for (i = 2; i < sizeof out; ++i) CHECK(out[i] == 0);
    CHECK(xbox_FramebufferKeyDown('C') == 0 && xbox_FramebufferKeyDown('E') == 0);
}

static void check_handbrake(HWND window, BOOL mm3)
{
    uint8_t out[20];
    XBOX_INPUT_STATE before, after;
    unsigned i, held;
    driving_context = FALSE;
    key(window, VK_SPACE, TRUE);
    report(out);
    for (i = 2; i < sizeof out; ++i) CHECK(out[i] == 0);
    driving_context = TRUE;
    xbox_InputSetKeyboardDrivingProbe(NULL);
    report(out);
    for (i = 2; i < sizeof out; ++i) CHECK(out[i] == 0);
    xbox_InputSetKeyboardDrivingProbe(fixture_driving);

    for (held = 0; held < 3; ++held) {
        report(out);
        CHECK(out[4] == (mm3 ? 255 : 0)); /* MM3 default handbrake is Xbox A. */
        for (i = 2; i < sizeof out; ++i) if (i != 4) CHECK(out[i] == 0);
    }
    key(window, '3', TRUE);
    report(out); CHECK(out[4] == (mm3 ? 255 : 0) && out[11] == 255);
    CHECK(out[10] == 0); /* The handbrake must not apply brake/reverse. */
    CHECK(xbox_InputGetState(0, &before) == ERROR_SUCCESS);
    key(window, VK_SPACE, FALSE);
    CHECK(xbox_InputGetState(0, &after) == ERROR_SUCCESS);
    CHECK(before.dwPacketNumber != after.dwPacketNumber);
    report(out); CHECK(out[4] == 0 && out[11] == 255);
    key(window, '3', FALSE);

    /* The original Z key and physical A retain their normal menu behavior. */
    key(window, 'Z', TRUE); key(window, VK_SPACE, TRUE);
    report(out); CHECK(out[4] == 255);
    key(window, VK_SPACE, FALSE);
    report(out); CHECK(out[4] == 255);
    driving_context = FALSE;
    report(out); CHECK(out[4] == 255);
    key(window, 'Z', FALSE);
    report(out); CHECK(out[4] == 0);
    driving_context = TRUE;
    host_connected = TRUE;
    memset(&host_state, 0, sizeof(host_state));
    host_state.Gamepad.wButtons = XINPUT_GAMEPAD_A;
    key(window, VK_SPACE, TRUE);
    report(out); CHECK(out[4] == 255);
    key(window, VK_SPACE, FALSE);
    report(out); CHECK(out[4] == 255);
    driving_context = FALSE;
    report(out); CHECK(out[4] == 255);
    host_connected = FALSE;

    driving_context = TRUE;
    key(window, VK_SPACE, TRUE);
    driving_context = FALSE; /* Held handbrake cannot select a pause-menu item. */
    report(out);
    for (i = 2; i < sizeof out; ++i) CHECK(out[i] == 0);
    driving_context = TRUE;
    report(out); CHECK(out[4] == (mm3 ? 255 : 0));
    key(window, 'Z', TRUE);
    SendMessageA(window, WM_KILLFOCUS, 0, 0);
    report(out);
    for (i = 2; i < sizeof out; ++i) CHECK(out[i] == 0);
    CHECK(xbox_FramebufferKeyDown(VK_SPACE) == 0 && xbox_FramebufferKeyDown('Z') == 0);
}

static void check_escape(HWND window, BOOL mm3)
{
    uint8_t out[20];
    unsigned context, held, i;
    for (context = 0; context < 2; ++context) {
        driving_context = context ? TRUE : FALSE;
        key(window, VK_ESCAPE, TRUE);
        CHECK(xbox_FramebufferKeyDown(VK_ESCAPE) != 0);
        CHECK(IsWindow(window)); /* Esc must not close the host window. */
        for (held = 0; held < 3; ++held) {
            report(out);
            CHECK(out[2] == 0); /* Menu back must not pause the game. */
            CHECK(out[5] == (mm3 ? 255 : 0)); /* Xbox B, like X. */
            for (i = 3; i < sizeof out; ++i)
                if (i != 5) CHECK(out[i] == 0);
        }
        key(window, VK_RETURN, TRUE);
        report(out);
        CHECK(out[2] == XBOX_GAMEPAD_START);
        CHECK(out[5] == (mm3 ? 255 : 0));
        key(window, VK_ESCAPE, FALSE);
        report(out); CHECK(out[2] == XBOX_GAMEPAD_START && out[5] == 0);
        key(window, VK_RETURN, FALSE);
        report(out); CHECK(out[2] == 0);
        key(window, VK_ESCAPE, TRUE);
        report(out); CHECK(out[2] == 0 && out[5] == (mm3 ? 255 : 0));
        key(window, VK_ESCAPE, FALSE);
        report(out); CHECK(out[2] == 0 && out[5] == 0);
        key(window, 'X', TRUE); key(window, VK_ESCAPE, TRUE);
        report(out); CHECK(out[5] == 255);
        key(window, VK_ESCAPE, FALSE);
        report(out); CHECK(out[5] == 255); /* X still holds the back action. */
        key(window, 'X', FALSE);
        report(out); CHECK(out[5] == 0);
        key(window, VK_ESCAPE, TRUE); key(window, VK_RETURN, TRUE);
        SendMessageA(window, WM_KILLFOCUS, 0, 0);
        report(out); CHECK(out[2] == 0 && out[5] == 0);
        CHECK(xbox_FramebufferKeyDown(VK_ESCAPE) == 0);
        CHECK(xbox_FramebufferKeyDown(VK_RETURN) == 0);
    }
}

int main(int argc, char **argv)
{
    WNDCLASSA wc = {0};
    HWND window;
    BOOL mm3;
    if (argc != 2) { fprintf(stderr, "Usage: mm3_input_contract mm3|legacy\n"); return 2; }
    mm3 = _stricmp(argv[1], "mm3") == 0;
    _putenv_s("RECOMP_KEYBOARD", "1");
    _putenv_s("RECOMP_KEYBOARD_LAYOUT", argv[1]);
    _putenv_s("RECOMP_PAD_SCRIPT", ""); _putenv_s("RECOMP_PAD_PRESS", "");
    _putenv_s("RECOMP_PAD_LIVE", ""); _putenv_s("RECOMP_INPUT_DIAG", "");
    _putenv_s("RECOMP_KEY_TRACE", "");
    wc.lpfnWndProc = fb_wndproc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "MM3InputContract";
    if (!RegisterClassA(&wc)) return 2;
    /* Hidden window: SendMessage exercises the production window handler. */
    window = CreateWindowA(wc.lpszClassName, "Offline input test", WS_OVERLAPPED,
        0, 0, 64, 64, NULL, NULL, wc.hInstance, NULL);
    if (!window) return 2;
    check_host_report();
    check_context(window, mm3);
    check_keyboard(window, mm3);
    check_gears(window, mm3);
    check_camera(window, mm3);
    check_handbrake(window, mm3);
    check_escape(window, mm3);
    DestroyWindow(window);
    UnregisterClassA(wc.lpszClassName, wc.hInstance);
    printf("%s input contract: %u checks, %u failures\n", argv[1], checks, failures);
    return failures ? 1 : 0;
}
