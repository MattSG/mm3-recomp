#include "rmlui_host.h"
#include <RmlUi_Backend.h>
#include <stdio.h>

#if defined(XBOXRECOMP_SDL3)
#include <SDL3/SDL.h>
extern "C" int xbox_FramebufferStartHidden(void);
extern "C" void xbox_FramebufferSdlHints(void);
#else
#include <RmlUi_Renderer_DX11.h>
#include <windows.h>
#include <dwmapi.h>
#endif

#if !defined(XBOXRECOMP_SDL3)
/* A dark caption in the client's colour instead of the light default. */
static void dark_title_bar(int icon_resource)
{
    HWND hwnd = NULL;
    EnumThreadWindows(GetCurrentThreadId(), [](HWND h, LPARAM p) -> BOOL {
        if (!IsWindowVisible(h))
            return TRUE;
        *(HWND *)p = h;
        return FALSE;
    }, (LPARAM)&hwnd);
    if (!hwnd)
        return;
    const BOOL dark = TRUE;
    const COLORREF caption = RGB(0x12, 0x14, 0x18);
    DwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof dark);
    DwmSetWindowAttribute(hwnd, 35 /* DWMWA_CAPTION_COLOR, Windows 11 */, &caption, sizeof caption);
    if (icon_resource) {
        if (HICON icon = LoadIconA(GetModuleHandleA(NULL), MAKEINTRESOURCEA(icon_resource))) {
            SendMessageA(hwnd, WM_SETICON, ICON_BIG, (LPARAM)icon);
            SendMessageA(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)icon);
        }
    }
}
#endif

int rmlui_host_run(const RmlUiHostOptions *options, const RmlUiHostClient *client)
{
    int width, height;
#if defined(XBOXRECOMP_SDL3)
    /* 10/9 of the window size, so the usable display bounds can shrink it. */
    SDL_Rect work = {0, 0, options->max_width * 10 / 9, options->max_height * 10 / 9};
    xbox_FramebufferSdlHints();   /* the same video driver choice as the game */
    if (SDL_InitSubSystem(SDL_INIT_VIDEO))
        SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay(), &work);
    width = Rml::Math::Min(options->max_width, work.w * 9 / 10);
    height = Rml::Math::Min(options->max_height, work.h * 9 / 10);
    /* The SDL GPU backend ships SPIR-V shaders only, which only Vulkan takes on Windows. */
    SDL_SetHint(SDL_HINT_GPU_DRIVER, "vulkan");
#else
    /* Not DPI-aware yet, so the work area is in the backend's 96-dpi units. */
    RECT work = {0, 0, options->max_width, options->max_height};
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &work, 0);
    width = Rml::Math::Min(options->max_width, (int)(work.right - work.left) * 9 / 10);
    height = Rml::Math::Min(options->max_height, (int)(work.bottom - work.top) * 9 / 10);
#endif
    if (!Backend::Initialize(options->title, width, height, false)) {
        if (client->unavailable)
            client->unavailable(client->user);
        return 0;
    }
#if !defined(XBOXRECOMP_SDL3)
    if (options->dark_title_bar)
        dark_title_bar(options->icon_resource);
#else
    /* A hidden start (scripts, tests), which the Win32 backend's first
     * ShowWindow honours and SDL's window does not. */
    if (xbox_FramebufferStartHidden()) {
        int count = 0;
        SDL_Window **windows = SDL_GetWindows(&count);
        for (int i = 0; i < count; i++)
            SDL_HideWindow(windows[i]);
        SDL_free(windows);
    }
#endif
    client->bind_system(client->user);
    Rml::SetSystemInterface(client->system);
    Rml::SetRenderInterface(Backend::GetRenderInterface());
    Rml::Initialise();
    if (client->prepare)
        client->prepare(client->user);

    Rml::Context *ctx = Rml::CreateContext(options->log_tag, Rml::Vector2i(width, height));
    if (ctx && client->load(ctx, client->user)) {
        bool rendered = false;
        /* Waits for messages only in the background: in front, the controller
         * is polled every frame (the backend presents with v-sync). */
        while (client->keep_running(client->user) &&
               Backend::ProcessEvents(ctx, client->on_key, !
#if defined(XBOXRECOMP_SDL3)
                   SDL_GetKeyboardFocus()
#else
                   GetActiveWindow()
#endif
               )) {
            const double now = Rml::GetSystemInterface()->GetElapsedTime();
            client->before_update(ctx, now, client->user);
            ctx->Update();
            client->after_update(ctx, client->user);
            /* The first WM_SIZE can precede creation of the backend renderer.
             * A hidden startup need not send another one: seed its viewport
             * from the context's actual pixel size before drawing. */
            const Rml::Vector2i dimensions = ctx->GetDimensions();
#if !defined(XBOXRECOMP_SDL3)
            static_cast<RenderInterface_DX11 *>(Backend::GetRenderInterface())->SetViewport(
                dimensions.x, dimensions.y);
#endif
#if defined(XBOXRECOMP_SDL3)
            /* No swapchain image while hidden or minimized, and RmlUi's SDL GPU
             * backend then submits a cancelled command buffer: skip the frame. */
            {
                int count = 0;
                SDL_Window **windows = SDL_GetWindows(&count);
                bool drawable = count > 0 && !(SDL_GetWindowFlags(windows[0]) &
                                               (SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED));
                SDL_free(windows);
                if (!drawable) {
                    SDL_Delay(16);
                    continue;
                }
            }
#endif
            /* The SDL GPU renderer takes its viewport from each swapchain image. */
            Backend::BeginFrame();
            ctx->Render();
            Backend::PresentFrame();
            if (!rendered && (!client->frame_ok || client->frame_ok(client->user))) {
                fprintf(stderr, "[%s] first frame rendered: %dx%d\n", options->log_tag,
                        dimensions.x, dimensions.y);
                fflush(stderr);
                rendered = true;
            }
        }
    }
    Rml::Shutdown();
    Backend::Shutdown();
    return 1;
}

/* Candidate sans-serif pairs, regular then bold, in the order a host tries them. */
int rmlui_host_load_sans_fonts(const char *family)
{
    static const char *const faces[][2] = {
        {"C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/segoeuib.ttf"},
        {"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
         "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"},
        {"/usr/share/fonts/TTF/DejaVuSans.ttf", "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf"},
        {"/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
         "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf"},
        {"/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
         "/usr/share/fonts/truetype/noto/NotoSans-Bold.ttf"},
        {"/System/Library/Fonts/Supplemental/Arial.ttf",
         "/System/Library/Fonts/Supplemental/Arial Bold.ttf"},
    };
    for (const auto &pair : faces) {
        FILE *regular = fopen(pair[0], "rb");
        FILE *bold;
        if (!regular)
            continue;
        fclose(regular);
        Rml::LoadFontFace(pair[0], family, Rml::Style::FontStyle::Normal,
                          Rml::Style::FontWeight::Normal, true);
        bold = fopen(pair[1], "rb");
        if (bold) {
            fclose(bold);
            Rml::LoadFontFace(pair[1], family, Rml::Style::FontStyle::Normal,
                              Rml::Style::FontWeight::Bold);
        }
        return 1;
    }
    return 0;
}
