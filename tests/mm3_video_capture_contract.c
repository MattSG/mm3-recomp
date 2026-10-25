#include "../src/mm3_host_video.c"
#include <string.h>
static void (*present_callback)(unsigned);
void xbox_VideoSetPresentHandler(void (*handler)(unsigned)) { present_callback=handler; }
#ifndef _WIN32
#include <strings.h>
int _stricmp(const char *a, const char *b) { return strcasecmp(a,b); }
void Sleep(DWORD milliseconds) { (void)milliseconds; }
#endif
static unsigned captures;
static char last[260];
int video_dump_frame_bmp(const char *path)
{
    captures++;
    snprintf(last, sizeof(last), "%s", path);
    return 0;
}
int main(void)
{
    mm3_host_video_init();
    if (present_callback != mm3_video_capture_present) return 3;
#ifdef _WIN32
    _putenv_s("RECOMP_FMV_DUMP", "chosen");
#else
    setenv("RECOMP_FMV_DUMP", "chosen", 1);
#endif
    for (unsigned i=1; i<=82; i++) mm3_video_capture_present(i);
    if (captures!=2 || strcmp(last,"chosen.80.bmp")) return 1;
#ifdef _WIN32
    _putenv_s("RECOMP_FMV_DUMP", "");
#else
    unsetenv("RECOMP_FMV_DUMP");
#endif
    mm3_video_capture_present(60);
    if (captures!=2) return 2;
    puts("PASS: MM3 selects only frames 60/80 when requested");
    return 0;
}
