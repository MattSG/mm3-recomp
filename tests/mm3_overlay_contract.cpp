#include "../src/mods/debug_overlay.cpp"
#include <assert.h>
#include <stdio.h>
extern "C" HWND xbox_FramebufferWindowHandle(void) { return nullptr; }
extern "C" void xbox_FramebufferSetHostOverlay(const XboxHostOverlay *) {}
extern "C" void mm3_teleport_state(Mm3TeleportState *s) { memset(s, 0, sizeof *s); }
extern "C" int mm3_teleport_queue(float, float, float) { return 0; }
// The runtime's host pad read: the test decides what the first pad holds.
static uint16_t fake_pad;
static bool fake_pad_connected;
extern "C" uint32_t xbox_InputHostPad(uint32_t port, uint16_t *buttons, uint8_t *, uint8_t *, int16_t *, int16_t *,
                                      int16_t *, int16_t *)
{
    if (port != 0 || !fake_pad_connected) return 1167; // ERROR_DEVICE_NOT_CONNECTED
    if (buttons) *buttons = fake_pad;
    return 0;
}
extern "C" int mm3_dev_tools_request(unsigned) { return 0; }
extern "C" void mm3_dev_tools_status(char *s, unsigned n) { if (n) *s = 0; }
int main()
{
    toggle_key = VK_F4; toggle_mouse = VK_XBUTTON1;
    assert(message(nullptr, WM_KEYDOWN, VK_F4, 0) && visible);
    assert(capture());
    message(nullptr, WM_KEYDOWN, VK_F4, 1LL << 30); assert(visible); // repeat is not a toggle
    assert(message(nullptr, WM_KEYDOWN, 'Z', 0));
    message(nullptr, WM_KEYDOWN, VK_F4, 0); assert(!visible);
    assert(!message(nullptr, WM_KEYDOWN, 'Z', 0));
    message(nullptr, WM_XBUTTONDOWN, MAKEWPARAM(0, XBUTTON1), 0); assert(visible);
    message(nullptr, WM_KILLFOCUS, 0, 0); assert(!visible && !capture());
    message(nullptr, WM_KEYDOWN, VK_F4, 0); assert(visible);
    message(nullptr, WM_ACTIVATEAPP, FALSE, 0); assert(!visible);
    _putenv_s("MM3_TEST_KEY", "77"); assert(setting("MM3_TEST_KEY", 0, 255) == VK_F8);
    _putenv_s("MM3_TEST_KEY", "100"); assert(setting("MM3_TEST_KEY", VK_F4, 255) == VK_F4);
    _putenv_s("MM3_TEST_KEY", "junk"); assert(setting("MM3_TEST_KEY", VK_F4, 255) == VK_F4);
    _putenv_s("MM3_TEST_KEY", "");
    assert(pad_buttons() == 0);                    // no pad: nothing held
    fake_pad_connected = true; fake_pad = 0x0020 | 0x0080; // back + right thumb
    assert(pad_buttons() == (0x0020 | 0x0080));
    puts("PASS: keyboard/mouse toggle, repeat, input ownership, focus loss and binding validation (offline)");
}
