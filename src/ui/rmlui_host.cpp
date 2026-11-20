#include "rmlui_host.h"
#include <RmlUi_Backend.h>
#include <stdio.h>

#include <SDL3/SDL.h>
extern "C" int xbox_FramebufferStartHidden(void);
extern "C" void xbox_FramebufferSdlHints(void);
extern "C" void fbw_sdl_adopt(SDL_Window *window, void (*release)(void));
SDL_Window *rmlui_backend_hand_over();
extern bool rmlui_backend_fullscreen;
void rmlui_backend_release_window();


static SDL_Window *host_window(void)
{
    int count = 0;
    SDL_Window **windows = SDL_GetWindows(&count);
    SDL_Window *window = count > 0 ? windows[0] : NULL;
    SDL_free(windows);
    return window;
}

int rmlui_host_fullscreen(void)
{
    SDL_Window *window = host_window();
    return window && (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
}

/* Borderless fullscreen on the window's display, and back to the window it was. */
void rmlui_host_set_fullscreen(int on)
{
    if (!on == !rmlui_host_fullscreen())
        return;
    SDL_Window *window = host_window();
    if (window && !(SDL_GetWindowFlags(window) & SDL_WINDOW_HIDDEN) &&   /* a hidden start stays hidden, */
        !SDL_getenv("MM3_LAUNCHER_OFFSCREEN"))                           /* an off-screen one off screen */
        SDL_SetWindowFullscreen(window, on != 0);   /* no mode set: the desktop's */
}

/* The document is laid out for max_width x max_height dp. A larger window (or
 * fullscreen) scales it up to fit; a smaller one keeps the display's own scale
 * and scrolls or reflows instead of shrinking the text. */
static void fit_to_window(Rml::Context *ctx, const RmlUiHostOptions *options)
{
    SDL_Window *window = host_window();
    const float native = window ? SDL_GetWindowDisplayScale(window) : 1.0f;
    const Rml::Vector2i size = ctx->GetDimensions();
    const float fit = Rml::Math::Min((float)size.x / (float)options->max_width,
                                     (float)size.y / (float)options->max_height);
    const float ratio = Rml::Math::Max(native > 0 ? native : 1.0f, fit);
    if (Rml::Math::Absolute(ctx->GetDensityIndependentPixelRatio() - ratio) > 0.001f)
        ctx->SetDensityIndependentPixelRatio(ratio);
}

int rmlui_host_run(const RmlUiHostOptions *options, const RmlUiHostClient *client)
{
    int width, height;
    /* 10/9 of the window size, so the usable display bounds can shrink it. */
    SDL_Rect work = {0, 0, options->max_width * 10 / 9, options->max_height * 10 / 9};
    xbox_FramebufferSdlHints();   /* the same video driver choice as the game */
    if (SDL_InitSubSystem(SDL_INIT_VIDEO))
        SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay(), &work);
    width = Rml::Math::Min(options->max_width, work.w * 9 / 10);
    height = Rml::Math::Min(options->max_height, work.h * 9 / 10);
    rmlui_backend_fullscreen = options->fullscreen && !xbox_FramebufferStartHidden();
    /* The SDL GPU backend ships SPIR-V shaders only, which only Vulkan takes on Windows. */
    SDL_SetHint(SDL_HINT_GPU_DRIVER, "vulkan");
    if (!Backend::Initialize(options->title, width, height, true)) {
        if (client->unavailable)
            client->unavailable(client->user);
        return 0;
    }
    /* A hidden start (scripts, tests), which the Win32 backend's first
     * ShowWindow honours and SDL's window does not. */
    if (xbox_FramebufferStartHidden()) {
        int count = 0;
        SDL_Window **windows = SDL_GetWindows(&count);
        for (int i = 0; i < count; i++)
            SDL_HideWindow(windows[i]);
        SDL_free(windows);
    }
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
                   SDL_GetKeyboardFocus()
               )) {
            const double now = Rml::GetSystemInterface()->GetElapsedTime();
            client->before_update(ctx, now, client->user);
            fit_to_window(ctx, options);
            ctx->Update();
            client->after_update(ctx, client->user);
            /* The first WM_SIZE can precede creation of the backend renderer.
             * A hidden startup need not send another one: seed its viewport
             * from the context's actual pixel size before drawing. */
            const Rml::Vector2i dimensions = ctx->GetDimensions();
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
    if (client->hand_over && client->hand_over(client->user)) {
        fbw_sdl_adopt(rmlui_backend_hand_over(), rmlui_backend_release_window);
        return 1;
    }
    Backend::Shutdown();
    return 1;
}

/* Candidate sans-serif pairs, regular then bold, in the order a host tries them. */
int rmlui_host_load_sans_fonts(const char *family)
{
    /* Regular, bold, bold italic (the bold italic is optional: a wordmark, no other text). */
    static const char *const faces[][3] = {
        {"C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/segoeuib.ttf", "C:/Windows/Fonts/segoeuiz.ttf"},
        {"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
         "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
         "/usr/share/fonts/truetype/dejavu/DejaVuSans-BoldOblique.ttf"},
        {"/usr/share/fonts/TTF/DejaVuSans.ttf", "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",
         "/usr/share/fonts/TTF/DejaVuSans-BoldOblique.ttf"},
        {"/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
         "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
         "/usr/share/fonts/truetype/liberation/LiberationSans-BoldItalic.ttf"},
        {"/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
         "/usr/share/fonts/truetype/noto/NotoSans-Bold.ttf",
         "/usr/share/fonts/truetype/noto/NotoSans-BoldItalic.ttf"},
        {"/System/Library/Fonts/Supplemental/Arial.ttf",
         "/System/Library/Fonts/Supplemental/Arial Bold.ttf",
         "/System/Library/Fonts/Supplemental/Arial Bold Italic.ttf"},
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
        if ((bold = fopen(pair[2], "rb")) != NULL) {
            fclose(bold);
            Rml::LoadFontFace(pair[2], family, Rml::Style::FontStyle::Italic,
                              Rml::Style::FontWeight::Bold);
        }
        return 1;
    }
    return 0;
}
