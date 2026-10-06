#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "recomp_types.h"
#include "mm3_debug_teleport.h"

/* PAL XBE: player update 2203E5; primary player manager 3C5CDC+54+D8.
 * Player+19C is the vehicle interface (vtable 383768), whose +8 is the car.
 * Car+3C points at simulation state; its +4 points at the rigid body.
 * Body+A4 is the native 3x4 transform; translation is Body+C8 (X/Y/Z).
 * Native reset 21F6DE uses vehicle vtable+AC (1D5F90 -> 1CDDA3), then
 * vtable+28 (1D65BC), copies the transform to Player+48C, and sets +218.
 * These calls belong on the game thread, never on the pipe/UI thread. */
extern void sub_002203E5_original(void);
extern void sub_001CDDA3(void);
extern void sub_001D65BC(void);

static int enabled;
static SRWLOCK lock = SRWLOCK_INIT;
static SRWLOCK request_lock = SRWLOCK_INIT;
static HANDLE finished;
static float position[3], destination[3];
static DWORD sampled;
static unsigned serial, pending, completed;
static int available;
static char result[128] = "not ready";

static int mapped(uint32_t va, size_t size)
{
    MEMORY_BASIC_INFORMATION info;
    uintptr_t p = XBOX_PTR(va);
    if (va < 0x10000u || va > 0x07FFFFFFu || size > 0x08000000u - va)
        return 0;
    if (!VirtualQuery((void *)p, &info, sizeof(info)) || info.State != MEM_COMMIT ||
        (info.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return 0;
    return p + size <= (uintptr_t)info.BaseAddress + info.RegionSize;
}

static uint32_t primary_player(void)
{
    uint32_t manager = MEM32(0x003C5CDCu), race;
    if (!mapped(manager, 0x58)) return 0;
    race = MEM32(manager + 0x54);
    if (!mapped(race, 0xDC)) return 0;
    return MEM32(race + 0xD8);
}

static uint32_t body_for(uint32_t player, uint32_t *car_out, uint32_t *vehicle_out)
{
    uint32_t vehicle, car, simulation, body;
    if (!mapped(player, 0x7A0)) return 0;
    vehicle = MEM32(player + 0x19C);
    if (!mapped(vehicle, 0x2C) || MEM32(vehicle) != 0x00383768u) return 0;
    car = MEM32(vehicle + 8);
    if (!mapped(car, 0x48)) return 0;
    simulation = MEM32(car + 0x3C);
    if (!mapped(simulation, 8)) return 0;
    body = MEM32(simulation + 4);
    if (!mapped(body, 0x1C4)) return 0;
    *car_out = car;
    *vehicle_out = vehicle;
    return body;
}

static void teleport(uint32_t player, uint32_t vehicle, uint32_t car,
                     uint32_t body, const float xyz[3])
{
    /* Inject below the incoming stack without changing its return/argument
     * slots. Restore architectural registers before entering the real update.
     * The two native paths use GPRs/x87 only (verified from the PAL XBE). */
    uint32_t regs[] = {g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi,
                       g_seh_ebp, g_ebp};
    double fp[8];
    int top = g_fp_top, cmp = g_fp_cmp, df = g_df;
    uint16_t cc = g_fp_cc, control = g_fp_control_word;
    uint32_t matrix = g_esp - 0x100u;
    memcpy(fp, g_fp_stack, sizeof(fp));
    memcpy((void *)XBOX_PTR(matrix), (const void *)XBOX_PTR(body + 0xA4), 48);
    for (unsigned i = 0; i < 3; ++i) MEMF(matrix + 0x24 + i * 4) = xyz[i];

    g_esp = matrix - 16;
    g_eax = car;
    PUSH32(g_esp, 0); /* synthetic return; the native routine discards it */
    sub_001CDDA3();
    g_ecx = vehicle;
    PUSH32(g_esp, matrix);
    PUSH32(g_esp, 0);
    sub_001D65BC();
    memcpy((void *)XBOX_PTR(player + 0x48C), (const void *)XBOX_PTR(matrix), 48);
    MEMF(player + 0x1B0) = 0;
    MEM8(player + 0x218) = 1;

    g_eax = regs[0]; g_ecx = regs[1]; g_edx = regs[2]; g_esp = regs[3];
    g_ebx = regs[4]; g_esi = regs[5]; g_edi = regs[6]; g_seh_ebp = regs[7];
    g_ebp = regs[8];
    memcpy(g_fp_stack, fp, sizeof(fp));
    g_fp_top = top; g_fp_cmp = cmp; g_fp_cc = cc;
    g_fp_control_word = control; g_df = df;
}

void sub_002203E5(void)
{
    if (enabled && g_ecx == primary_player()) {
        uint32_t car, vehicle, player = g_ecx;
        uint32_t body = body_for(player, &car, &vehicle);
        AcquireSRWLockExclusive(&lock);
        available = body != 0;
        if (body) {
            float current[3];
            for (unsigned i = 0; i < 3; ++i) current[i] = MEMF(body + 0xC8 + i * 4);
            available = isfinite(current[0]) && isfinite(current[1]) && isfinite(current[2]);
            if (available) memcpy(position, current, sizeof(current));
        }
        sampled = GetTickCount();
        if (pending) {
            if (available && mapped(g_esp - 0x400, 0x400)) {
                teleport(player, vehicle, car, body, destination);
                for (unsigned i = 0; i < 3; ++i) position[i] = MEMF(body + 0xC8 + i * 4);
                strcpy_s(result, sizeof(result), "applied");
            } else strcpy_s(result, sizeof(result), "no active car");
            completed = pending;
            pending = 0;
            SetEvent(finished);
        }
        ReleaseSRWLockExclusive(&lock);
    }
    sub_002203E5_original();
}

static void request_teleport(const float xyz[3], char *reply, size_t size)
{
    AcquireSRWLockExclusive(&request_lock);
    AcquireSRWLockExclusive(&lock);
    if (available && GetTickCount() - sampled < 500) {
        unsigned request = ++serial;
        memcpy(destination, xyz, sizeof(destination));
        ResetEvent(finished);
        pending = request;
        ReleaseSRWLockExclusive(&lock);
        WaitForSingleObject(finished, 2000);
        AcquireSRWLockExclusive(&lock);
        if (completed != request) {
            pending = 0; /* cancel; a timed-out request cannot run later */
            strcpy_s(result, sizeof(result), "game update timeout");
        }
    } else strcpy_s(result, sizeof(result), "no active car");
    snprintf(reply, size, "{\"result\":\"%s\",\"x\":%.9g,\"y\":%.9g,\"z\":%.9g}",
             result, position[0], position[1], position[2]);
    ReleaseSRWLockExclusive(&lock);
    ReleaseSRWLockExclusive(&request_lock);
}

static DWORD WINAPI pipe_thread(void *unused)
{
    char name[80];
    (void)unused;
    snprintf(name, sizeof(name), "\\\\.\\pipe\\MM3Teleport-%lu", GetCurrentProcessId());
    for (;;) {
        HANDLE pipe = CreateNamedPipeA(name, PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
            1, 512, 512, 0, NULL);
        if (pipe == INVALID_HANDLE_VALUE) return 1;
        if (ConnectNamedPipe(pipe, NULL) || GetLastError() == ERROR_PIPE_CONNECTED) {
            char command[128] = {0}, reply[256], extra;
            DWORD bytes;
            float xyz[3];
            if (ReadFile(pipe, command, sizeof(command) - 1, &bytes, NULL)) {
                command[bytes] = 0;
                if (sscanf(command, "teleport %f %f %f %c", &xyz[0], &xyz[1], &xyz[2], &extra) == 3 &&
                    isfinite(xyz[0]) && isfinite(xyz[1]) && isfinite(xyz[2])) {
                    request_teleport(xyz, reply, sizeof(reply));
                } else if (strcmp(command, "position") == 0) {
                    AcquireSRWLockShared(&lock);
                    snprintf(reply, sizeof(reply), "{\"available\":%s,\"x\":%.9g,\"y\":%.9g,\"z\":%.9g}",
                        available && GetTickCount() - sampled < 500 ? "true" : "false",
                        position[0], position[1], position[2]);
                    ReleaseSRWLockShared(&lock);
                } else strcpy_s(reply, sizeof(reply), "{\"error\":\"use position or teleport X Y Z; coordinates must be finite\"}");
                if (WriteFile(pipe, reply, (DWORD)strlen(reply), &bytes, NULL))
                    FlushFileBuffers(pipe); /* do not discard an unread reply on disconnect */
            }
        }
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
    }
}

static HWND edits[3], coordinates, feedback;

static LRESULT CALLBACK panel_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_CREATE) {
        const char *axes[] = {"X", "Y", "Z"};
        coordinates = CreateWindowA("STATIC", "Waiting for active car", WS_CHILD | WS_VISIBLE,
            10, 10, 325, 35, window, NULL, NULL, NULL);
        for (unsigned i = 0; i < 3; ++i) {
            CreateWindowA("STATIC", axes[i], WS_CHILD | WS_VISIBLE,
                10 + i * 108, 48, 20, 20, window, NULL, NULL, NULL);
            edits[i] = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "0",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                10 + i * 108, 68, 100, 24, window, NULL, NULL, NULL);
        }
        CreateWindowA("BUTTON", "Teleport", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            10, 100, 100, 26, window, (HMENU)1, NULL, NULL);
        feedback = CreateWindowA("STATIC", "Native X/Y/Z; orientation retained, motion reset.",
            WS_CHILD | WS_VISIBLE, 10, 135, 325, 35, window, NULL, NULL, NULL);
        SetTimer(window, 1, 200, NULL);
        return 0;
    }
    if (message == WM_TIMER) {
        char text[160];
        AcquireSRWLockShared(&lock);
        if (available && GetTickCount() - sampled < 500)
            snprintf(text, sizeof(text), "Native position\r\nX %.3f   Y %.3f   Z %.3f",
                position[0], position[1], position[2]);
        else strcpy_s(text, sizeof(text), "Waiting for active car");
        ReleaseSRWLockShared(&lock);
        SetWindowTextA(coordinates, text);
        return 0;
    }
    if (message == WM_COMMAND && LOWORD(wparam) == 1 && HIWORD(wparam) == BN_CLICKED) {
        float xyz[3];
        char text[96], reply[256], extra;
        for (unsigned i = 0; i < 3; ++i) {
            GetWindowTextA(edits[i], text, sizeof(text));
            if (sscanf(text, "%f %c", &xyz[i], &extra) != 1 || !isfinite(xyz[i])) {
                SetWindowTextA(feedback, "Enter three finite numeric coordinates.");
                return 0;
            }
        }
        request_teleport(xyz, reply, sizeof(reply));
        SetWindowTextA(feedback, reply);
        return 0;
    }
    if (message == WM_CLOSE) { ShowWindow(window, SW_HIDE); return 0; }
    return DefWindowProcA(window, message, wparam, lparam);
}

static BOOL CALLBACK find_framebuffer(HWND window, LPARAM param)
{
    DWORD pid;
    char name[80];
    GetWindowThreadProcessId(window, &pid);
    GetClassNameA(window, name, sizeof(name));
    if (pid == GetCurrentProcessId() && strcmp(name, "XboxRecompFramebuffer") == 0) {
        *(HWND *)param = window;
        return FALSE;
    }
    return TRUE;
}

static DWORD WINAPI panel_thread(void *unused)
{
    HWND owner = NULL, panel;
    WNDCLASSA wc = {0};
    RECT rect;
    MSG msg;
    (void)unused;
    while (!owner) { EnumWindows(find_framebuffer, (LPARAM)&owner); if (!owner) Sleep(200); }
    wc.lpfnWndProc = panel_proc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "MM3DebugCoordinates";
    if (!RegisterClassA(&wc)) return 1;
    GetWindowRect(owner, &rect);
    panel = CreateWindowExA(WS_EX_TOOLWINDOW, wc.lpszClassName, "MM3 debug coordinates",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        rect.right - 360, rect.top + 40, 355, 210, owner, NULL, wc.hInstance, NULL);
    if (!panel) return 1;
    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        if (!IsDialogMessageA(panel, &msg)) { TranslateMessage(&msg); DispatchMessageA(&msg); }
    }
    return 0;
}

void mm3_debug_teleport_init(void)
{
    const char *opt = getenv("MM3_DEBUG_TELEPORT");
    HANDLE thread;
    if (!opt || strcmp(opt, "1") != 0) return;
    finished = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (!finished) return;
    enabled = 1;
    thread = CreateThread(NULL, 0, pipe_thread, NULL, 0, NULL);
    if (!thread) { enabled = 0; CloseHandle(finished); return; }
    CloseHandle(thread);
    thread = CreateThread(NULL, 0, panel_thread, NULL, 0, NULL);
    if (thread) CloseHandle(thread);
}
