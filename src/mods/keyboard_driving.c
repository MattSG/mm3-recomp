/* Keyboard driving: on the runtime's keyboard pad (RECOMP_KEYBOARD=1) the
 * arrows drive while the player's car is being driven -- up/down are the
 * right/left triggers and left/right steer -- and are only menu d-pad
 * buttons otherwise. The numbered trigger and numpad stick keys still work
 * in either context. While driving, R/F are Xbox B/X (the default manual
 * gear up/down), Space is A (handbrake) and C is Black (change camera
 * view). Escape is B everywhere, as X's menu back. Enter remains Start. */
#include <windows.h>
#include <input/xinput_xbox.h>

extern int xbox_FramebufferKeyDown(int vk);

static int (*s_driving)(void);

static void keyboard_driving(XBOX_INPUT_STATE *state, BOOL menu)
{
    XBOX_GAMEPAD *g = &state->Gamepad;
    BOOL left, right;
    if (xbox_FramebufferKeyDown(VK_ESCAPE))
        g->bAnalogButtons[XBOX_BUTTON_B] = 255;
    if (menu || !s_driving || !s_driving())
        return;
    /* Driving, nothing but the arrows sets the keyboard's d-pad. */
    g->wButtons &= ~(XBOX_GAMEPAD_DPAD_UP | XBOX_GAMEPAD_DPAD_DOWN |
                     XBOX_GAMEPAD_DPAD_LEFT | XBOX_GAMEPAD_DPAD_RIGHT);
    if (xbox_FramebufferKeyDown(VK_SPACE)) g->bAnalogButtons[XBOX_BUTTON_A] = 255;
    if (xbox_FramebufferKeyDown('R')) g->bAnalogButtons[XBOX_BUTTON_B] = 255;
    if (xbox_FramebufferKeyDown('F')) g->bAnalogButtons[XBOX_BUTTON_X] = 255;
    if (xbox_FramebufferKeyDown('C')) g->bAnalogButtons[XBOX_BUTTON_BLACK] = 255;
    if (xbox_FramebufferKeyDown(VK_DOWN)) g->bAnalogButtons[XBOX_BUTTON_LTRIGGER] = 255;
    if (xbox_FramebufferKeyDown(VK_UP)) g->bAnalogButtons[XBOX_BUTTON_RTRIGGER] = 255;
    left = g->sThumbLX < 0 || xbox_FramebufferKeyDown(VK_LEFT);
    right = g->sThumbLX > 0 || xbox_FramebufferKeyDown(VK_RIGHT);
    g->sThumbLX = (SHORT)((right - left) * 32767);
}

/* driving() says whether the player is driving (NULL: never); it runs on the
 * input thread. */
void mm3_keyboard_driving_init(int (*driving)(void))
{
    s_driving = driving;
    xbox_InputSetKeyboardHook(keyboard_driving);
}
