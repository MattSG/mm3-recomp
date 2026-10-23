# Historical host-video capture policy

Moved from fork 365656b. Fixed evidence-frame choices belong to the port.
The BMP writer copies decoded CPU memory; it never proves a successful texture
upload, draw or window presentation. Original source commentary follows.

```c
        /* A couple of frames to disc as evidence that real pixels were
         * drawn -- two of them, far enough apart to show the picture moving
         * rather than one still held for the duration. */
        if ((frames == 60 || frames == 80) && getenv("RECOMP_FMV_DUMP")) {
            char out[MAX_PATH];
            _snprintf_s(out, sizeof(out), _TRUNCATE, "%s.%u.bmp",
                        getenv("RECOMP_FMV_DUMP"), frames);
            video_dump_frame_bmp(out);
        }

```

```c
/* Write the frame currently decoded to a 24-bit BMP.
 *
 * The point is evidence rather than debugging: "the video played" is otherwise
 * only a log line, and a log line cannot distinguish a decoded frame that got
 * drawn from a black screen that ran for the right number of seconds. This
 * writes what was uploaded to the texture, so a non-black file means real
 * decoded pixels reached the renderer. */

```

```c
/**
 * Shared FMV player: Media Foundation decode onto a D3D8 texture.
 *
 * Titles whose video is an ordinary container Windows can already decode do not
 * need their in-game decoder emulated to be watchable. The recompiled title
 * still drives everything -- it opens the file when its own logic says to, and
 * the runtime plays that file -- so the video appears where the game puts it
 * rather than being replayed on a timer.
 *
 * Lifted from burnout3's player, with its boot-sequence state machine and menu
 * dependencies removed: those are title logic, and this is the decoder.
 *
 * The pump in xbox_VideoPlayRun() owns its window, its D3D8 device and its
 * Media Foundation session on one thread, so nothing here touches the guest's
 * execution or its memory.
 */

```

## Migration and proof

The shared pump now exposes xbox_VideoSetPresentHandler. Register only while
playback is stopped/quiesced; notification is on the playback thread after
Present returns, with a 1-based counter reset for each playback. It does not
prove Present succeeded or that a new frame decoded. MM3's existing
src/mm3_host_video.c owns the environment option and frame-60/80 selection.
The generic BMP function now has the same explicit unsupported return on Linux
as the other decoder APIs. It copies decoded CPU memory, not the final window.

Windows tests/video_pump exercises the actual pump using mocked decoder,
window creation and D3D8 device operations: no automatic captures with legacy
environment alone, callback order/counts, caller selection and clearing. Linux
builds/executes the actual unsupported APIs. Each CTest project passes 1/1.
The MM3 fixture passes on Windows and Linux and checks registration and exact
frame selection. Both final game builds pass. Logs: conformance_tmp/ownership-
video-*.log and ownership-mm3-video-*.log. No real movie playback, decoding,
GPU output or gameplay acceptance is claimed by these synthetic checks.

From the MM3 checkout, run the generic project with cmake -S
tools/xboxrecomp/tests/video_pump -B build/video-pump, build it, then ctest
--test-dir build/video-pump (add -C Release on Windows). On Linux the MM3
fixture is runnable with:

```sh
cc -Itools/xboxrecomp/src -Itools/xboxrecomp/src/kernel tests/mm3_video_capture_contract.c -o build/mm3-video-contract
build/mm3-video-contract
```

Windows uses the same fixture with MSVC include paths and the normal Windows
SDK libraries. Decoder allocation/failure cleanup, real playback pacing and
resource lifetime remain separate audit work.
