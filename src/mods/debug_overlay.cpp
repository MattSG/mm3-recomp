#include <windows.h>
#include <Xinput.h>
#include <d3d11.h>
#include <stdlib.h>
#include <math.h>
#include <mutex>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
extern "C" {
#include "fb_present.h"
#include "teleport.h"
#include "dev_tools.h"
}
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace {
volatile LONG visible;
volatile LONG pad_block;
int toggle_key = VK_F4, toggle_mouse;
WORD toggle_pad = XINPUT_GAMEPAD_BACK | XINPUT_GAMEPAD_LEFT_THUMB;
// Win32 backend calls (capture, key modifiers and cursor) belong on the
// window thread. Serialize them with render-thread ImGui calls; recursive
// locking permits synchronous WM_CAPTURECHANGED during ReleaseCapture.
std::recursive_mutex imgui_lock;
bool backend_ready;
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

bool input_message(UINT m)
{
    return (m >= WM_KEYFIRST && m <= WM_KEYLAST) ||
           (m >= WM_MOUSEFIRST && m <= WM_MOUSELAST);
}

int message(HWND h, UINT m, WPARAM w, LPARAM l)
{
    std::lock_guard<std::recursive_mutex> guard(imgui_lock);
    if (m == WM_KILLFOCUS || (m == WM_ACTIVATEAPP && !w))
        InterlockedExchange(&visible, 0);
    bool key_toggle = (m == WM_KEYDOWN || m == WM_SYSKEYDOWN) &&
                      w == (WPARAM)toggle_key && !(l & (1LL << 30));
    int mouse = m == WM_MBUTTONDOWN ? VK_MBUTTON :
                m == WM_XBUTTONDOWN ? (GET_XBUTTON_WPARAM(w) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2) : 0;
    if (key_toggle || (toggle_mouse && mouse == toggle_mouse))
        InterlockedExchange(&visible, !visible);
    LRESULT handled = backend_ready ? ImGui_ImplWin32_WndProcHandler(h, m, w, l) : 0;
    return (visible && handled) ||
           (input_message(m) && (visible || key_toggle || (toggle_mouse && mouse == toggle_mouse)));
}

int capture()
{
    if (visible) return 1;
    /* Block the physical toggle chord before the next render samples it. */
    HWND window = xbox_FramebufferWindowHandle();
    if (window && GetForegroundWindow() == window && toggle_pad) {
        XINPUT_STATE pad = {};
        if (XInputGetState(0, &pad) == ERROR_SUCCESS) {
            if ((pad.Gamepad.wButtons & toggle_pad) == toggle_pad)
                InterlockedExchange(&pad_block, toggle_pad);
            if (pad.Gamepad.wButtons & (WORD)pad_block) return 1;
        }
    }
    InterlockedExchange(&pad_block, 0);
    return 0;
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
    if (!open) InterlockedExchange(&visible, 0);
}

void render(void *h, void *device, void *context, void *target)
{
    std::lock_guard<std::recursive_mutex> guard(imgui_lock);
    static bool failed, pad_held;
    if (!h || !device || !context || !target) return;
    if (failed) return;
    HWND window = (HWND)h;
    if (!backend_ready) {
        IMGUI_CHECKVERSION(); ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        io.IniFilename = nullptr; // Session layout, no extra settings file.
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
        ImGui::StyleColorsDark();
        ImGuiStyle &style = ImGui::GetStyle();
        style.WindowRounding = 8; style.FrameRounding = 4;
        style.WindowPadding = ImVec2(18,18); style.ItemSpacing = ImVec2(10,10);
        style.Colors[ImGuiCol_CheckMark] = ImVec4(0.91f,0.64f,0.24f,1);
        bool platform = ImGui_ImplWin32_Init(h);
        if (!platform || !ImGui_ImplDX11_Init((ID3D11Device *)device, (ID3D11DeviceContext *)context)) {
            if (platform) ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext(); failed = true;
            InterlockedExchange(&visible, 0); return;
        }
        backend_ready = true;
    }
    XINPUT_STATE pad = {};
    bool focused = GetForegroundWindow() == window;
    bool down = focused && toggle_pad && XInputGetState(0, &pad) == ERROR_SUCCESS &&
                (pad.Gamepad.wButtons & toggle_pad) == toggle_pad;
    if (down && !pad_held) {
        InterlockedExchange(&pad_block, toggle_pad);
        InterlockedExchange(&visible, !visible);
    }
    pad_held = down;
    if (!focused) InterlockedExchange(&visible, 0);
    ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();
    Mm3TeleportState state = {};
    mm3_teleport_state(&state);
    if (!state.available || state.world != bookmark.world) bookmark_saved = false;
    if (visible) draw_tools(state);
    ImGui::Render();
    auto *ctx = (ID3D11DeviceContext *)context;
    auto *rtv = (ID3D11RenderTargetView *)target;
    ctx->OMSetRenderTargets(1, &rtv, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}
}

extern "C" void mm3_debug_overlay_init()
{
    const char *enabled = getenv("MM3_DEBUG_OVERLAY");
    if (!enabled || !*enabled || !strcmp(enabled, "0")) return;  /* as mm3_mod_on */
    toggle_key = setting("MM3_OVERLAY_KEY", VK_F4, 255);
    toggle_mouse = setting("MM3_OVERLAY_MOUSE", 0, 6);
    if (toggle_mouse != VK_MBUTTON && toggle_mouse != VK_XBUTTON1 && toggle_mouse != VK_XBUTTON2) toggle_mouse = 0;
    toggle_pad = (WORD)setting("MM3_OVERLAY_PAD", 0x60, 0xFFFF);
    XboxHostOverlay hooks = {render, message, capture};
    xbox_FramebufferSetHostOverlay(&hooks);
}
