#include <windows.h>
#include <Xinput.h>
#include <assert.h>
#include <stdio.h>
static XINPUT_STATE physical;
static int connected, captured, keys_enabled = 1;
static DWORD WINAPI fixture_get_state(DWORD port, XINPUT_STATE *out)
{
    if (port || !connected) return ERROR_DEVICE_NOT_CONNECTED;
    *out = physical; return ERROR_SUCCESS;
}
#define XInputGetState fixture_get_state
#include "../tools/xboxrecomp/src/input/xinput_device.c"
int xbox_FramebufferHostInputCaptured(void) { return captured; }
int xbox_FramebufferKeyDown(int vk) { return keys_enabled && (vk == 'Z' || vk == '3' || vk == VK_NUMPAD8); }
int xbox_FramebufferMouse(float *x, float *y) { *x = *y = 0; return 0; }
void nv2a_d3d_collect_menu_rects(int on) { (void)on; }
void xbox_FramebufferSetMousePressHandler(int (*handler)(float, float)) { (void)handler; }
int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "--pad-only")) {
        _putenv_s("RECOMP_KEYBOARD", "0"); connected = 1;
        physical.dwPacketNumber = 100; physical.Gamepad.wButtons = XINPUT_GAMEPAD_A;
        XBOX_INPUT_STATE state;
        assert(xbox_InputGetState(0, &state) == ERROR_SUCCESS && state.dwPacketNumber == 100);
        captured = 1; assert(xbox_InputGetState(0, &state) == ERROR_SUCCESS);
        DWORD neutral_packet = state.dwPacketNumber; assert(neutral_packet != 100);
        captured = 0; assert(xbox_InputGetState(0, &state) == ERROR_SUCCESS);
        DWORD restored_packet = state.dwPacketNumber;
        assert(restored_packet != neutral_packet && state.Gamepad.bAnalogButtons[XBOX_BUTTON_A] == 255);
        assert(xbox_InputGetState(0, &state) == ERROR_SUCCESS && state.dwPacketNumber == restored_packet);
        ++physical.dwPacketNumber; physical.Gamepad.wButtons = XINPUT_GAMEPAD_B;
        assert(xbox_InputGetState(0, &state) == ERROR_SUCCESS && state.dwPacketNumber != restored_packet);
        assert(state.Gamepad.bAnalogButtons[XBOX_BUTTON_B] == 255 && !state.Gamepad.bAnalogButtons[XBOX_BUTTON_A]);
        puts("PASS: physical-only capture/release packet collisions and subsequent button transitions");
        return 0;
    }
    _putenv_s("RECOMP_KEYBOARD", "1"); _putenv_s("RECOMP_MENU_KEYS", "0");
    connected = 1;
    physical.dwPacketNumber = 100;
    physical.Gamepad.wButtons = XINPUT_GAMEPAD_START | XINPUT_GAMEPAD_DPAD_UP;
    physical.Gamepad.bLeftTrigger = 255; physical.Gamepad.sThumbRX = 1000;
    XBOX_INPUT_STATE state;
    assert(xbox_InputGetState(0, &state) == ERROR_SUCCESS);
    assert(state.Gamepad.wButtons && state.Gamepad.bAnalogButtons[XBOX_BUTTON_A] == 255);
    assert(state.Gamepad.sThumbLY && state.Gamepad.sThumbRX == 1000);
    DWORD before_capture = state.dwPacketNumber;
    captured = 1;
    assert(xbox_InputGetState(0, &state) == ERROR_SUCCESS);
    assert(state.dwPacketNumber != before_capture);
    XBOX_GAMEPAD neutral = {0}; assert(memcmp(&state.Gamepad, &neutral, sizeof neutral) == 0);
    DWORD packet = state.dwPacketNumber;
    assert(xbox_InputGetState(0, &state) == ERROR_SUCCESS && state.dwPacketNumber != packet);
    assert(xbox_InputGetState(1, &state) == ERROR_DEVICE_NOT_CONNECTED); // No phantom pads.
    captured = 0;
    assert(xbox_InputGetState(0, &state) == ERROR_SUCCESS && state.Gamepad.sThumbRX == 1000);
    connected = 0; captured = 1;
    assert(xbox_InputGetState(0, &state) == ERROR_SUCCESS);
    assert(memcmp(&state.Gamepad, &neutral, sizeof neutral) == 0);
    captured = 0;
    assert(xbox_InputGetState(0, &state) == ERROR_SUCCESS && state.Gamepad.bAnalogButtons[XBOX_BUTTON_A] == 255);
    puts("PASS: real runtime keyboard/pad merge, capture neutralization, packet updates, release and disconnected ports");
}
