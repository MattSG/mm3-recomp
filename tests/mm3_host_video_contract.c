#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "kernel.h"
static int playing, plays;
static unsigned sleeps;
static char last_path[MAX_PATH*4];
static void (*open_observer)(const xbox_host_char *);
static void (*firmware_hook)(ULONG);
int video_dump_frame_bmp(const char *path) { (void)path; return 0; }
void xbox_VideoSetPresentHandler(void (*handler)(unsigned frame)) { (void)handler; }
static void test_sleep(DWORD ms) { assert(ms==50); sleeps++; if (playing>0) playing--; }
#define Sleep test_sleep
#include "../tools/xboxrecomp/src/video/video_host_policy.c"
#include "../src/mm3_host_video.c"
#undef Sleep
void xbox_SetFileOpenObserver(void (*observer)(const xbox_host_char *)) { open_observer=observer; }
void xbox_SetFirmwareReturnHook(void (*hook)(ULONG)) { firmware_hook=hook; }
int xbox_VideoIsPlaying(void) { return playing; }
int xbox_VideoPlayFile(const char *path) { strcpy(last_path,path); plays++; playing=1; return 0; }
int main(void)
{
#ifdef _WIN32
    _putenv_s("RECOMP_FMV_HOST","");
#else
    unsetenv("RECOMP_FMV_HOST");
#endif
    mm3_host_video_init(); assert(!open_observer && firmware_hook);
#ifdef _WIN32
    _putenv_s("RECOMP_FMV_HOST","1");
#else
    setenv("RECOMP_FMV_HOST","1",1);
#endif
    mm3_host_video_init(); assert(open_observer);
#ifdef _WIN32
    open_observer(L"C:/movies/notes.txt"); assert(!plays);
    open_observer(L"C:/movies/\u00FC.WMV");
#else
    open_observer("C:/movies/notes.txt"); assert(!plays);
    open_observer("C:/movies/\xc3\xbc.WMV");
#endif
    assert(plays==1 && !strcmp(last_path,"C:/movies/\xc3\xbc.WMV"));
#ifdef _WIN32
    open_observer(L"C:/movies/second.wmv");
#else
    open_observer("C:/movies/second.wmv");
#endif
    assert(plays==1);
    playing=2; firmware_hook(37); assert(!playing && sleeps==2);
    playing=-1; sleeps=0; firmware_hook(37); assert(sleeps==1200);
    puts("PASS: optional host-video selection, UTF-8 paths, active-playback guard and bounded exit wait");
}
