/* Optional bring-up presentation policy, owned by the game port. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kernel.h"
#include "video/video_player.h"

/* Preserve the port's optional two-frame host-player diagnostic. */
static void mm3_video_capture_present(unsigned frame)
{
    const char *prefix = getenv("RECOMP_FMV_DUMP");
    if (prefix && (frame == 60 || frame == 80)) {
        char path[260];
        snprintf(path, sizeof(path), "%s.%u.bmp", prefix, frame);
        video_dump_frame_bmp(path);
    }
}

static void opened_video(const xbox_host_char *path)
{
#if defined(_WIN32)
    char host[MAX_PATH * 4];
    if (!WideCharToMultiByte(CP_UTF8,0,path,-1,host,sizeof host,NULL,NULL)) return;
#else
    const char *host = path;
#endif
    size_t n = strlen(host);
    if (n > 4 && !_stricmp(host+n-4,".wmv") && !xbox_VideoIsPlaying())
        xbox_VideoPlayFile(host);
}

static void finish_video(ULONG routine)
{
    unsigned waited = 0;
    (void)routine;
    while (xbox_VideoIsPlaying() && waited < 60000) {
        Sleep(50);
        waited += 50;
    }
    if (waited) fprintf(stderr,"[MM3_VIDEO] waited %u ms before firmware return\n",waited);
}

void mm3_host_video_init(void)
{
    xbox_VideoSetPresentHandler(mm3_video_capture_present);
    xbox_SetFirmwareReturnHook(finish_video);
    xbox_SetFileOpenObserver(getenv("RECOMP_FMV_HOST") ? opened_video : NULL);
}
