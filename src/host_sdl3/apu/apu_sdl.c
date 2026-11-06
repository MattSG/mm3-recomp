/**
 * SDL3 Audio Output Backend
 *
 * The xa2_* contract of apu_xaudio2.h on every host: the APU frame
 * thread hands over one EP period (256 stereo frames) at a time, and the
 * device queue paces it (mcpx_apu_pace_step reads xa2_queued_samples).
 * An SDL audio stream bound to the default device is the queue; its device
 * thread pulls one device buffer at a time and pads a short pull with
 * silence, which is why the pacing marks keep at least one device buffer
 * queued.
 *
 * Headless runs (RECOMP_HEADLESS=1) use SDL's "dummy" driver unless
 * SDL_AUDIO_DRIVER is set (the environment wins over the hint): it drains
 * the queue in real time without a device, so pacing behaves as on a real
 * device and nothing reaches the speakers. SDL_AUDIO_DRIVER=coreaudio (or
 * pulseaudio, pipewire) with RECOMP_HEADLESS=1 is the way to hear a
 * headless run. RECOMP_AUDIO_MUTE=1 plays at volume 0, paced as usual.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "apu_xaudio2.h"


#include <SDL3/SDL.h>

#define SDLA_SAMPLE_RATE   48000
#define SDLA_CHANNELS      2
/* Device buffer size and count: the same defaults and knobs as the XAudio2
 * backend (1024 x 8: 21.3 ms per buffer, 171 ms queued at the high mark). The
 * count is a pacing mark here, not a slot limit: SDL's queue is unbounded. */
#define SDLA_BUF_SAMPLES_DEFAULT 1024
#define SDLA_NUM_BUFS_DEFAULT    8
#define SDLA_BUF_SAMPLES_MIN     128
#define SDLA_BUF_SAMPLES_MAX     4096
#define SDLA_NUM_BUFS_MIN        2
#define SDLA_NUM_BUFS_MAX        8

static SDL_AudioStream *g_dev;
static float     g_gain = 1.0f;     /* 0 under RECOMP_AUDIO_MUTE */
static int       g_muted;           /* pause state last applied */
static int       g_buf_samples = SDLA_BUF_SAMPLES_DEFAULT;
static int       g_num_bufs    = SDLA_NUM_BUFS_DEFAULT;
static int       g_initialized;
static int       g_audio_opened;    /* this backend brought SDL audio up */
static int       g_frames_written;  /* periods handed to the queue */
static unsigned  g_underruns;
static uint64_t  g_samples_queued;  /* samples handed to the queue, ever */
static int       g_dry;
static int64_t   g_underrun_logged_ms;

static int env_int(const char *name, int def, int lo, int hi)
{
    const char *e = getenv(name);
    int v;
    if (!e || !*e) return def;
    v = atoi(e);
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    return v;
}

static int64_t now_ms(void)
{
    return (int64_t)SDL_GetTicks();
}

int xa2_init(void)
{
    const SDL_AudioSpec spec = { SDL_AUDIO_S16, SDLA_CHANNELS, SDLA_SAMPLE_RATE };
    const char *e, *driver;
    char frames[16];
    int have = 0;

    if (g_initialized) return 1;

    g_buf_samples = env_int("RECOMP_AUDIO_BUF_SAMPLES", SDLA_BUF_SAMPLES_DEFAULT,
                            SDLA_BUF_SAMPLES_MIN, SDLA_BUF_SAMPLES_MAX);
    g_num_bufs = env_int("RECOMP_AUDIO_BUF_COUNT", SDLA_NUM_BUFS_DEFAULT,
                         SDLA_NUM_BUFS_MIN, SDLA_NUM_BUFS_MAX);

    /* SDL installs SIGINT/SIGTERM handlers that only post SDL_QUIT, which
     * nothing reads in a headless run; keep the default handlers so Ctrl-C
     * still ends the process. */
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");

    /* Headless: no speakers unless the caller names a driver. Must be set
     * before the audio subsystem starts; 0 keeps an existing value. */
    e = getenv("RECOMP_HEADLESS");
    if (e && *e && *e != '0')
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    snprintf(frames, sizeof frames, "%d", g_buf_samples);
    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, frames);

    if (!SDL_WasInit(SDL_INIT_AUDIO)) {
        if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
            fprintf(stderr, "[SDL] audio init failed: %s\n", SDL_GetError());
            return 0;
        }
        g_audio_opened = 1;
    }

    /* The mixer's 48 kHz s16 stereo is what the stream takes; SDL converts
     * for the device if it has to. */
    g_dev = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (!g_dev) {
        fprintf(stderr, "[SDL] SDL_OpenAudioDeviceStream failed: %s\n", SDL_GetError());
        if (g_audio_opened) { SDL_QuitSubSystem(SDL_INIT_AUDIO); g_audio_opened = 0; }
        return 0;
    }
    SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(g_dev), NULL, &have);
    if (have > 0 && have != g_buf_samples) {
        fprintf(stderr, "[SDL] device buffer is %d samples (asked %d)\n",
                have, g_buf_samples);
        g_buf_samples = have;
        /* The queue marks and the pacer's stall window assume buffers no
         * larger than the env limit. A driver that grants more is still
         * fed, against marks sized for the limit, and may underrun. */
        if (g_buf_samples > SDLA_BUF_SAMPLES_MAX)
            g_buf_samples = SDLA_BUF_SAMPLES_MAX;
    }

    g_frames_written = 0;
    g_samples_queued = 0;
    g_dry = 0;
    e = getenv("RECOMP_AUDIO_MUTE");
    g_gain = (e && *e && *e != '0') ? 0.0f : 1.0f;
    g_muted = 0;
    SDL_SetAudioStreamGain(g_dev, g_gain);
    g_initialized = 1;
    SDL_ResumeAudioStreamDevice(g_dev);

    driver = SDL_GetCurrentAudioDriver();
    fprintf(stderr, "[SDL] audio initialized: driver %s (%d Hz stereo 16-bit, %d x %d-sample buffers, %.1f ms queued max)%s\n",
            driver ? driver : "?", SDLA_SAMPLE_RATE, g_num_bufs, g_buf_samples,
            1000.0 * g_num_bufs * g_buf_samples / SDLA_SAMPLE_RATE,
            driver && !strcmp(driver, "dummy") ? " [silent: no device output]" : "");
    return 1;
}

void xa2_shutdown(void)
{
    if (g_dev) {
        SDL_PauseAudioStreamDevice(g_dev);
        SDL_ClearAudioStream(g_dev);
        SDL_DestroyAudioStream(g_dev);
        g_dev = NULL;
    }
    if (g_audio_opened) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        g_audio_opened = 0;
    }
    if (g_initialized)
        fprintf(stderr, "[SDL] shut down (%d periods written, %u underruns)\n",
                g_frames_written, g_underruns);
    g_initialized = 0;
}

int xa2_is_active(void)
{
    return g_initialized;
}

/* Hand one EP period to the queue. SDL copies the samples, so the caller's
 * buffer is free on return. Nothing is ever refused by depth: the queue is
 * unbounded and pacing by queue depth holds it at the marks. */
int xa2_submit_samples(const int16_t *samples, int num_samples)
{
    if (!g_initialized || !g_dev || num_samples <= 0) return 0;
    {   /* Paused in the background (fb_present.c): quiet, not a held note. */
        extern int xbox_FramebufferPaused(void);
        int paused = xbox_FramebufferPaused();
        if (paused != g_muted) {
            SDL_SetAudioStreamGain(g_dev, paused ? 0.0f : g_gain);
            g_muted = paused;
        }
    }
    if (!SDL_PutAudioStreamData(g_dev, samples,
                                num_samples * SDLA_CHANNELS * (int)sizeof(int16_t))) {
        fprintf(stderr, "[SDL] SDL_PutAudioStreamData failed: %s\n", SDL_GetError());
        return 0;
    }
    g_frames_written++;
    g_samples_queued += (uint64_t)num_samples;
    return 1;
}

int xa2_get_buffer_size(void)
{
    return g_buf_samples;
}

int xa2_get_buffer_count(void)
{
    return g_num_bufs;
}

/* Samples queued and not yet pulled by the device thread. The device holds
 * up to one more buffer in flight, which this does not count, the same way
 * XAudio2's BuffersQueued does not count the one playing. Also the underrun
 * detector: an empty queue after audio has started means the device's last
 * pull was short and padded with silence; counted once per dry spell and
 * reported at most once a second. A drain announced by xa2_expect_drain
 * (the frame thread pausing) is not one. */
int xa2_queued_samples(void)
{
    int bytes;

    if (!g_initialized || !g_dev) return 0;
    bytes = SDL_GetAudioStreamQueued(g_dev);
    if (bytes < 0) bytes = 0;
    if (bytes == 0 && g_frames_written > 0) {
        if (!g_dry) {
            int64_t now = now_ms();
            g_dry = 1;
            g_underruns++;
            if (now - g_underrun_logged_ms >= 1000) {
                g_underrun_logged_ms = now;
                fprintf(stderr, "[SDL] underrun x%u\n", g_underruns);
            }
        }
    } else {
        g_dry = 0;
    }
    return bytes / (SDLA_CHANNELS * (int)sizeof(int16_t));
}

void xa2_expect_drain(void)
{
    g_dry = 1;
}

unsigned xa2_underruns(void)
{
    return g_underruns;
}

/* What the device thread has pulled off the queue; it pulls one device
 * buffer per callback, so this leads the speaker by up to one buffer. */
uint64_t xa2_samples_played(void)
{
    int bytes;
    uint64_t queued;

    if (!g_initialized || !g_dev) return 0;
    bytes = SDL_GetAudioStreamQueued(g_dev);
    queued = bytes > 0 ? (uint64_t)bytes / (SDLA_CHANNELS * sizeof(int16_t)) : 0;
    return g_samples_queued > queued ? g_samples_queued - queued : 0;
}
