/* Host loop for a RmlUi window: backend start, window sizing, the frame loop
 * and shutdown.
 *
 * The client is the title. It binds its system interface, loads the fonts and
 * data model it shows, handles keys, and does the per-frame work around the
 * context update. The host does not know what the window shows. RmlUi and its
 * backends (RmlUi_Backend.h, RmlUi_Renderer_DX11.h or the SDL GPU backend) are
 * the consumer's to fetch and compile. */
#ifndef UI_RMLUI_HOST_H
#define UI_RMLUI_HOST_H

#include <RmlUi/Core.h>
#include <stdio.h>

struct RmlUiHostOptions {
    const char *title;
    int max_width, max_height;   /* the window is at most this, and within 90% of the work area */
    int dark_title_bar;          /* Win32 only: dark caption; 0 keeps the light default */
    int icon_resource;           /* Win32 only: icon resource id for the window; 0 keeps the class icon */
    const char *log_tag;         /* prefix for the host's own diagnostics */
};

struct RmlUiHostClient {
    void *user;
    void (*bind_system)(void *user);  /* after the backend starts: set the client's system interface */
    Rml::SystemInterface *system;     /* then installed with Rml::SetSystemInterface */
    void (*prepare)(void *user);      /* after RmlUi is initialised, before the context: fonts */
    int (*load)(Rml::Context *ctx, void *user);  /* data model and document; 0 = no window */
    bool (*on_key)(Rml::Context *ctx, Rml::Input::KeyIdentifier key, int mods, float, bool priority);
    int (*keep_running)(void *user);  /* 0 ends the loop */
    void (*before_update)(Rml::Context *ctx, double now, void *user);
    void (*after_update)(Rml::Context *ctx, void *user);  /* after the first ctx->Update() */
    int (*frame_ok)(void *user);      /* 0 while the client has logged errors; suppresses the first-frame line */
    void (*unavailable)(void *user);  /* the backend could not start */
};

/* Returns 1 when the backend started and the loop ran, 0 when it could not start. */
int rmlui_host_run(const RmlUiHostOptions *options, const RmlUiHostClient *client);

/* Registers the first installed sans-serif pair (regular, then bold) under family, and as the
 * fallback face for glyphs the family lacks. Returns 0 when no candidate is installed. */
int rmlui_host_load_sans_fonts(const char *family);

/* A system interface that forwards to base and writes RmlUi's messages to
 * stderr as "[tag] message", counting the errors. Set base before the host
 * starts; the errors count feeds the client's first-frame check. */
struct RmlUiHostLogSystem : Rml::SystemInterface {
    Rml::SystemInterface *base = nullptr;
    const char *tag = "rmlui";
    unsigned errors = 0;
    double GetElapsedTime() override { return base->GetElapsedTime(); }
    void SetMouseCursor(const Rml::String &name) override { base->SetMouseCursor(name); }
    void SetClipboardText(const Rml::String &text) override { base->SetClipboardText(text); }
    void GetClipboardText(Rml::String &text) override { base->GetClipboardText(text); }
    bool LogMessage(Rml::Log::Type type, const Rml::String &message) override
    {
        if (type == Rml::Log::LT_ERROR || type == Rml::Log::LT_ASSERT)
            ++errors;
        fprintf(stderr, "[%s] %s\n", tag, message.c_str());
        fflush(stderr);
        return true;
    }
};

#endif
