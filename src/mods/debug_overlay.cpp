#if defined(XBOXRECOMP_SDL3)
#include <SDL3/SDL.h>
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlgpu3.h"
#else
#include <windows.h>
#include <Xinput.h>
#include <d3d11.h>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#endif
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <atomic>
#include <mutex>
extern "C" {
#include "fb_present.h"
#include "teleport.h"
#include "dev_tools.h"
#include "render_policy.h"
}
#if !defined(XBOXRECOMP_SDL3)
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
#endif

namespace {
/* MM3_OVERLAY_KEY / MM3_OVERLAY_MOUSE / the default toggle are Windows virtual-key
 * codes on every build; the SDL path maps them to SDL keys and buttons. */
const int kVkF4 = 0x73, kVkMButton = 0x04, kVkXButton1 = 0x05, kVkXButton2 = 0x06;
/* XINPUT button bits (BACK | LEFT_THUMB); the pad mask is in these bits on every build. */
const unsigned short kPadBackThumb = 0x0020 | 0x0040;

std::atomic<long> visible;
std::atomic<long> pad_block;
int toggle_key = kVkF4, toggle_mouse;
unsigned short toggle_pad = kPadBackThumb;
// Backend calls (capture, key modifiers and cursor) belong on the window
// thread. Serialize them with render-thread ImGui calls; recursive
// locking permits synchronous focus callbacks during capture changes.
std::recursive_mutex imgui_lock;
bool backend_ready;
bool debug_enabled;
Mm3TeleportState bookmark = {};
bool bookmark_saved;

unsigned setting(const char *name, unsigned fallback, unsigned maximum)
{
    const char *s = getenv(name);
    char *end;
    if (!s || !*s) return fallback;
    unsigned long value = strtoul(s, &end, 16);
    return *end || value > maximum ? fallback : (unsigned)value;
}

void command(unsigned probe)
{
    mm3_dev_tools_request(probe);
}

void draw_tools(const Mm3TeleportState &state)
{
    ImGui::SetNextWindowSize(ImVec2(550, 430), ImGuiCond_FirstUseEver);
    bool open = visible != 0;
    if (ImGui::Begin("Midtown Madness 3 | Debug tools", &open)) {
        ImGui::TextDisabled("Session tools | simulation keeps running");
        if (!state.available)
            ImGui::TextWrapped("Actions unavailable: enter gameplay with an active car.");
        if (ImGui::BeginTabBar("tools")) {
            if (ImGui::BeginTabItem("Developer")) {
                ImGui::TextWrapped("Uses the game's existing debug commands. Prepared scripts are required.");
                ImGui::BeginDisabled(!state.available);
                if (ImGui::Button("Show debug HUD")) command(1);
                ImGui::SameLine();
                if (ImGui::Button("Hide debug HUD")) command(10);
                if (ImGui::Button("Open native debug menu")) {
                    if (mm3_dev_tools_request(2)) open = false;
                }
                ImGui::TextWrapped("The overlay closes so the native menu can receive the D-pad.");
                if (ImGui::Button("Close native debug menu")) command(11);
                ImGui::EndDisabled();
                char status[128]; mm3_dev_tools_status(status, sizeof status);
                ImGui::Separator(); ImGui::TextWrapped("%s", status);
                ImGui::TextDisabled("No save unlocks or unverified cheat writes.");
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Teleport")) {
                static float x, z, heading;
                static bool set_heading;
                ImGui::Text("Position: %.3f / %.3f / %.3f", state.position[0], state.position[1], state.position[2]);
                ImGui::Text("Heading: %.1f degrees", state.heading * 57.2957795f);
                ImGui::BeginDisabled(!state.available || state.pending);
                if (ImGui::Button("Use current position")) {
                    x = state.position[0]; z = state.position[2]; heading = state.heading * 57.2957795f;
                }
                ImGui::InputFloat("X", &x, 1, 10, "%.3f");
                ImGui::InputFloat("Z", &z, 1, 10, "%.3f");
                ImGui::Checkbox("Set heading", &set_heading);
                if (set_heading) ImGui::InputFloat("Heading (degrees)", &heading);
                ImGui::BeginDisabled(!isfinite(x) || !isfinite(z) || (set_heading && !isfinite(heading)));
                if (ImGui::Button("Teleport to ground"))
                    mm3_teleport_queue(x, z, set_heading ? heading * 0.0174532925f : NAN);
                ImGui::EndDisabled();
                if (ImGui::Button("Remember this position")) { bookmark = state; bookmark_saved = true; }
                ImGui::SameLine(); ImGui::BeginDisabled(!bookmark_saved);
                if (ImGui::Button("Return to remembered position"))
                    mm3_teleport_queue(bookmark.position[0], bookmark.position[2], bookmark.heading);
                ImGui::EndDisabled();
                ImGui::EndDisabled();
                ImGui::TextWrapped("Height and car placement use native ground reset. A ground miss leaves the car unchanged. Remembered position clears when gameplay becomes unavailable or the world changes.");
                ImGui::TextWrapped("%s", state.result);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Vehicle")) {
                ImGui::TextWrapped("Uses the game's native repair and ground-reset functions.");
                ImGui::BeginDisabled(!state.available || state.pending);
                if (ImGui::Button("Repair current vehicle")) command(MM3_DEV_REPAIR);
                if (ImGui::Button("Recover at current position"))
                    mm3_teleport_queue(state.position[0], state.position[2], state.heading);
                ImGui::EndDisabled();
                ImGui::TextWrapped("Recovery places the vehicle on usable ground and clears motion. It does not search for a road or move to a known safe destination.");
                char status[128]; mm3_dev_tools_status(status, sizeof status);
                ImGui::TextWrapped("%s", status);
                ImGui::TextWrapped("%s", state.result);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Diagnostics")) {
                ImGui::Text("Active car: %s", state.available ? "available" : "unavailable");
                ImGui::Text("Player: %08X | car: %08X | world: %08X", state.player, state.car, state.world);
                ImGui::Text("Teleport: %s", state.pending ? "pending" : "idle");
                ImGui::Text("Position: %.3f, %.3f, %.3f", state.position[0], state.position[1], state.position[2]);
                ImGui::TextWrapped("Keyboard: arrows / Tab, Enter or Space. Controller: D-pad / stick, A to activate, shoulders to change tabs. Use your configured binding to close.");
                ImGui::Text("Toggle key VK: %02X | pad mask: %04X | mouse VK: %02X", toggle_key, toggle_pad, toggle_mouse);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
    if (!open) visible.exchange(0);
}

/* Shared frame steps: the Win32 and SDL paths both call these. */
void create_context()
{
    IMGUI_CHECKVERSION(); ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr; // Session layout, no extra settings file.
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    ImGui::StyleColorsDark();
    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowRounding = 8; style.FrameRounding = 4;
    style.WindowPadding = ImVec2(18,18); style.ItemSpacing = ImVec2(10,10);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.91f,0.64f,0.24f,1);
}

/* Pad chord edge: pressing it toggles the overlay once per press. */
void pad_edge(bool focused, bool down)
{
    static bool pad_held;
    if (down && !pad_held) {
        pad_block.exchange(toggle_pad);
        visible.exchange(!visible);
    }
    pad_held = down;
    if (!focused) visible.exchange(0);
}

void build_frame()
{
    ImGui::NewFrame();
    Mm3TeleportState state = {};
    mm3_teleport_state(&state);
    if (!state.available || state.world != bookmark.world) bookmark_saved = false;
    if (visible) draw_tools(state);
}

#if !defined(XBOXRECOMP_SDL3)
/* ---- Win32 / D3D11 platform ---- */

void *overlay_window()
{
    return (void *)xbox_FramebufferWindowHandle();
}

bool window_focused(void *window)
{
    return GetForegroundWindow() == (HWND)window;
}

unsigned short pad_buttons()
{
    XINPUT_STATE pad = {};
    return XInputGetState(0, &pad) == ERROR_SUCCESS ? pad.Gamepad.wButtons : 0;
}

bool backend_init(void *window, void *device, void *context)
{
    bool platform = ImGui_ImplWin32_Init(window);
    if (platform && ImGui_ImplDX11_Init((ID3D11Device *)device, (ID3D11DeviceContext *)context)) return true;
    if (platform) ImGui_ImplWin32_Shutdown();
    return false;
}

void backend_new_frame()
{
    ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame();
}

void backend_draw(void *context, void *target)
{
    auto *ctx = (ID3D11DeviceContext *)context;
    auto *rtv = (ID3D11RenderTargetView *)target;
    ctx->OMSetRenderTargets(1, &rtv, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

int message(HWND h, UINT m, WPARAM w, LPARAM l)
{
    std::lock_guard<std::recursive_mutex> guard(imgui_lock);
    if (m == WM_KILLFOCUS || (m == WM_ACTIVATEAPP && !w))
        visible.exchange(0);
    bool key_toggle = (m == WM_KEYDOWN || m == WM_SYSKEYDOWN) &&
                      w == (WPARAM)toggle_key && !(l & (1LL << 30));
    int mouse = m == WM_MBUTTONDOWN ? VK_MBUTTON :
                m == WM_XBUTTONDOWN ? (GET_XBUTTON_WPARAM(w) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2) : 0;
    if (key_toggle || (toggle_mouse && mouse == toggle_mouse))
        visible.exchange(!visible);
    LRESULT handled = backend_ready ? ImGui_ImplWin32_WndProcHandler(h, m, w, l) : 0;
    bool input = (m >= WM_KEYFIRST && m <= WM_KEYLAST) ||
                 (m >= WM_MOUSEFIRST && m <= WM_MOUSELAST);
    return (visible && handled) ||
           (input && (visible || key_toggle || (toggle_mouse && mouse == toggle_mouse)));
}
#else
/* ---- SDL3 SDLGPU platform ---- */

SDL_Scancode toggle_scancode = SDL_SCANCODE_UNKNOWN;
SDL_Gamepad *gamepad;

/* Config VK code -> physical key. Covers letters, digits, F1-F24, Tab, Enter, Esc, Space;
 * other VKs get no keyboard toggle (ceiling: add a row when a binding needs one). */
SDL_Scancode vk_to_scancode(int vk)
{
    if (vk >= 'A' && vk <= 'Z') return (SDL_Scancode)(SDL_SCANCODE_A + vk - 'A');
    if (vk >= '1' && vk <= '9') return (SDL_Scancode)(SDL_SCANCODE_1 + vk - '1');
    if (vk == '0') return SDL_SCANCODE_0;
    if (vk >= 0x70 && vk <= 0x7B) return (SDL_Scancode)(SDL_SCANCODE_F1 + vk - 0x70);
    if (vk >= 0x7C && vk <= 0x87) return (SDL_Scancode)(SDL_SCANCODE_F13 + vk - 0x7C);
    switch (vk) {
    case 0x09: return SDL_SCANCODE_TAB;
    case 0x0D: return SDL_SCANCODE_RETURN;
    case 0x1B: return SDL_SCANCODE_ESCAPE;
    case 0x20: return SDL_SCANCODE_SPACE;
    }
    return SDL_SCANCODE_UNKNOWN;
}

int sdl_button_vk(Uint8 button)
{
    switch (button) {
    case SDL_BUTTON_MIDDLE: return kVkMButton;
    case SDL_BUTTON_X1: return kVkXButton1;
    case SDL_BUTTON_X2: return kVkXButton2;
    }
    return 0;
}

void *overlay_window()
{
    return (void *)xbox_FramebufferSdlWindow();
}

bool window_focused(void *window)
{
    return (SDL_GetWindowFlags((SDL_Window *)window) & SDL_WINDOW_INPUT_FOCUS) != 0;
}

/* Pad bits in the XINPUT layout the mask uses, mapped to the first opened gamepad. */
struct PadBit { unsigned short xinput; SDL_GamepadButton button; };
const PadBit pad_map[] = {
    {0x0001, SDL_GAMEPAD_BUTTON_DPAD_UP},     {0x0002, SDL_GAMEPAD_BUTTON_DPAD_DOWN},
    {0x0004, SDL_GAMEPAD_BUTTON_DPAD_LEFT},   {0x0008, SDL_GAMEPAD_BUTTON_DPAD_RIGHT},
    {0x0010, SDL_GAMEPAD_BUTTON_START},       {0x0020, SDL_GAMEPAD_BUTTON_BACK},
    {0x0040, SDL_GAMEPAD_BUTTON_LEFT_STICK},  {0x0080, SDL_GAMEPAD_BUTTON_RIGHT_STICK},
    {0x0100, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER}, {0x0200, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER},
    {0x1000, SDL_GAMEPAD_BUTTON_SOUTH},       {0x2000, SDL_GAMEPAD_BUTTON_EAST},
    {0x4000, SDL_GAMEPAD_BUTTON_WEST},        {0x8000, SDL_GAMEPAD_BUTTON_NORTH},
};

/* Called from capture() and render(), which are not under one lock: pad_lock guards the gamepad handle. */
unsigned short pad_buttons()
{
    static std::mutex pad_lock;
    static bool tried;
    std::lock_guard<std::mutex> guard(pad_lock);
    if (!tried) { tried = true; SDL_InitSubSystem(SDL_INIT_GAMEPAD); }
    if (gamepad && !SDL_GamepadConnected(gamepad)) { SDL_CloseGamepad(gamepad); gamepad = nullptr; }
    if (!gamepad) {
        int count = 0;
        SDL_JoystickID *ids = SDL_GetGamepads(&count);
        if (ids && count > 0) gamepad = SDL_OpenGamepad(ids[0]);
        SDL_free(ids);
    }
    if (!gamepad) return 0;
    unsigned short bits = 0;
    for (const PadBit &b : pad_map)
        if (SDL_GetGamepadButton(gamepad, b.button)) bits |= b.xinput;
    return bits;
}

bool backend_init(void *window, void *device, void *)
{
    SDL_Window *sdl_window = (SDL_Window *)window;
    SDL_GPUDevice *gpu = (SDL_GPUDevice *)device;
    bool platform = ImGui_ImplSDL3_InitForSDLGPU(sdl_window);
    ImGui_ImplSDLGPU3_InitInfo info = {};
    info.Device = gpu;
    info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(gpu, sdl_window);
    info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
    if (platform && ImGui_ImplSDLGPU3_Init(&info)) return true;
    if (platform) ImGui_ImplSDL3_Shutdown();
    return false;
}

void backend_new_frame()
{
    ImGui_ImplSDLGPU3_NewFrame(); ImGui_ImplSDL3_NewFrame();
}

/* Records its own pass on the swapchain; the game image is already there. Not submitted here. */
void backend_draw(void *context, void *target)
{
    SDL_GPUCommandBuffer *cmd = (SDL_GPUCommandBuffer *)context;
    ImDrawData *data = ImGui::GetDrawData();
    Imgui_ImplSDLGPU3_PrepareDrawData(data, cmd); // must precede the render pass
    SDL_GPUColorTargetInfo color = {};
    color.texture = (SDL_GPUTexture *)target;
    color.load_op = SDL_GPU_LOADOP_LOAD;
    color.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &color, 1, nullptr);
    if (!pass) return;
    ImGui_ImplSDLGPU3_RenderDrawData(data, cmd, pass);
    SDL_EndGPURenderPass(pass);
}

/* Window-thread SDL event hook. Nonzero consumes the event (the game does not see it). */
int event(const SDL_Event *e)
{
    std::lock_guard<std::recursive_mutex> guard(imgui_lock);
    if (e->type == SDL_EVENT_WINDOW_FOCUS_LOST) visible.exchange(0);
    bool key = e->type == SDL_EVENT_KEY_DOWN || e->type == SDL_EVENT_KEY_UP ||
               e->type == SDL_EVENT_TEXT_INPUT || e->type == SDL_EVENT_TEXT_EDITING;
    bool mouse = e->type == SDL_EVENT_MOUSE_MOTION || e->type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
                 e->type == SDL_EVENT_MOUSE_BUTTON_UP || e->type == SDL_EVENT_MOUSE_WHEEL;
    bool key_toggle = e->type == SDL_EVENT_KEY_DOWN && !e->key.repeat &&
                      toggle_scancode != SDL_SCANCODE_UNKNOWN && e->key.scancode == toggle_scancode;
    int button = e->type == SDL_EVENT_MOUSE_BUTTON_DOWN ? sdl_button_vk(e->button.button) : 0;
    bool mouse_toggle = toggle_mouse && button == toggle_mouse;
    if (key_toggle || mouse_toggle) visible.exchange(!visible);
    bool wanted = false;
    if (backend_ready) {
        ImGui_ImplSDL3_ProcessEvent(e);
        ImGuiIO &io = ImGui::GetIO();
        wanted = (key && io.WantCaptureKeyboard) || (mouse && io.WantCaptureMouse);
    }
    return (visible && wanted) ||
           ((key || mouse) && (visible || key_toggle || mouse_toggle));
}
#endif

int capture()
{
    if (visible) return 1;
    /* Block the physical toggle chord before the next render samples it. */
    void *window = overlay_window();
    if (window && window_focused(window) && toggle_pad) {
        unsigned short buttons = pad_buttons();
        if ((buttons & toggle_pad) == toggle_pad)
            pad_block.exchange(toggle_pad);
        if (buttons & (unsigned short)pad_block) return 1;
    }
    pad_block.exchange(0);
    return 0;
}

void render(void *h, void *device, void *context, void *target)
{
    std::lock_guard<std::recursive_mutex> guard(imgui_lock);
    static bool failed;
    if (!h || !device || !context || !target) return;
    if (!debug_enabled) return;
    if (failed) return;
    if (!backend_ready) {
        create_context();
        if (!backend_init(h, device, context)) {
            ImGui::DestroyContext(); failed = true;
            visible.exchange(0); return;
        }
        backend_ready = true;
    }
    bool focused = window_focused(h);
    bool down = debug_enabled && focused && toggle_pad && (pad_buttons() & toggle_pad) == toggle_pad;
    pad_edge(focused, down);
    backend_new_frame();
    build_frame();
    ImGui::Render();
    backend_draw(context, target);
}
}

extern "C" void mm3_debug_overlay_init()
{
    const char *enabled = getenv("MM3_DEBUG_OVERLAY");
    debug_enabled = enabled && *enabled && strcmp(enabled, "0");
    if (!debug_enabled) return;
    toggle_key = setting("MM3_OVERLAY_KEY", kVkF4, 255);
    toggle_mouse = setting("MM3_OVERLAY_MOUSE", 0, 6);
    if (toggle_mouse != kVkMButton && toggle_mouse != kVkXButton1 && toggle_mouse != kVkXButton2) toggle_mouse = 0;
    toggle_pad = (unsigned short)setting("MM3_OVERLAY_PAD", kPadBackThumb, 0xFFFF);
#if defined(XBOXRECOMP_SDL3)
    toggle_scancode = vk_to_scancode(toggle_key);
    XboxHostOverlay hooks = {render, debug_enabled ? event : nullptr, debug_enabled ? capture : nullptr};
#else
    XboxHostOverlay hooks = {render, debug_enabled ? message : nullptr, debug_enabled ? capture : nullptr};
#endif
    xbox_FramebufferSetHostOverlay(&hooks);
}
