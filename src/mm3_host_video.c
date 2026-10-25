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

void mm3_host_video_init(void)
{
    xbox_VideoSetPresentHandler(mm3_video_capture_present);
}
