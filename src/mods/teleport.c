#define _CRT_SECURE_NO_WARNINGS
#if defined(_WIN32)
#include <windows.h>
#else
#include "platform/xbox_winnt.h"
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "recomp_types.h"
#include "teleport.h"

#if !defined(_WIN32)
/* Bounds-checked strcpy_s is MSVC-only; the replies here are NUL-terminated. */
#define strcpy_s(dst, size, src) snprintf((dst), (size), "%s", (src))
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
#endif

/* PAL XBE: player update 2203E5; primary player manager 3C5CDC+54+D8.
 * Player+19C is the vehicle interface (vtable 383768), whose +8 is the car.
 * Car+3C points at simulation state; its +4 points at the rigid body.
 * Body+A4 is the native 3x4 transform; translation is Body+C8 (X/Y/Z).
 * Ground reset 21ADF3 queries collision through 21848C, then uses vehicle
 * vtable+AC (1D5F90 -> 1CDDA3) and +28 (1D65BC), caches transform, sets +218.
 * These calls belong on the game thread, never on the pipe/UI thread. */
extern void sub_002203E5_original(void);
extern void sub_0021ADF3(void);
extern void mm3_mods_player_update(void);

static int enabled;
static volatile LONG keyboard_sampled, keyboard_active;
static SRWLOCK lock = SRWLOCK_INIT;
#if defined(_WIN32)
static SRWLOCK request_lock = SRWLOCK_INIT;
#define REQUEST_LOCK() AcquireSRWLockExclusive(&request_lock)
#define REQUEST_TRY_LOCK() TryAcquireSRWLockExclusive(&request_lock)
#define REQUEST_UNLOCK() ReleaseSRWLockExclusive(&request_lock)
#else
/* The compat layer has no TryAcquire for SRWLOCK; a mutex gives the same
 * blocking and non-blocking entry points. */
static pthread_mutex_t request_lock = PTHREAD_MUTEX_INITIALIZER;
#define REQUEST_LOCK() pthread_mutex_lock(&request_lock)
#define REQUEST_TRY_LOCK() (pthread_mutex_trylock(&request_lock) == 0)
#define REQUEST_UNLOCK() pthread_mutex_unlock(&request_lock)
#endif
static HANDLE finished;
static float position[3], destination[3], heading_now;   /* radians, as the pipe takes it */
static float destination_heading = NAN;   /* radians; NAN keeps the car's */
static DWORD sampled;
static unsigned serial, pending, completed;
static DWORD pending_until;
static uint32_t sampled_player, sampled_car, sampled_world, requested_player, requested_car;
static uint32_t requested_world;
static int available;
static char result[128] = "not ready";
/* Every car the player update touches (AI included), for the AI-start check. */
static struct { uint32_t player, vtable; float x, z, fx, fz; DWORD tick; } cars[16];

int mm3_keyboard_driving(void)
{
    DWORD tick = (DWORD)InterlockedCompareExchange(&keyboard_sampled, 0, 0);
    if (!InterlockedCompareExchange(&keyboard_active, 0, 0) ||
        GetTickCount() - tick >= 500u) return 0;
    /* Pause singleton 1E25C1 returns 429DB0. Open 1E2CD5 sets +2;
     * Resume 1E2BDA clears it. Engine mode 431610 is zero while driving.
     * Check both on each input poll so menu arrows work immediately when
     * a paused car still has a recent update. */
    return MEM8(0x00429DB2u) == 0 && MEM32(0x00431610u) == 0;
}

/* Called several times a frame per player; nearly every query lands in the
 * same committed region, so the last good one is remembered (the hook runs on
 * the game thread only). */
static int mapped(uint32_t va, size_t size)
{
    static uintptr_t good_lo, good_hi;
    MEMORY_BASIC_INFORMATION info;
    uintptr_t p = XBOX_PTR(va);
    if (va < 0x10000u || va > 0x07FFFFFFu || size > 0x08000000u - va)
        return 0;
    if (p >= good_lo && p + size <= good_hi)
        return 1;
    if (!VirtualQuery((void *)p, &info, sizeof(info)) || info.State != MEM_COMMIT ||
        (info.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return 0;
    good_lo = (uintptr_t)info.BaseAddress;
    good_hi = good_lo + info.RegionSize;
    return p + size <= good_hi;
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

uint32_t mm3_debug_repair_player(void)
{
    uint32_t player = primary_player(), car, vehicle;
    /* Native repair follows car+38 to its damage state before resetting
     * components. A live transform alone does not establish that it exists. */
    if (!enabled || !body_for(player, &car, &vehicle) || !mapped(car, 0xC4) ||
        !mapped(MEM32(car + 0x38), 0x34)) return 0;
    uint32_t simulation = MEM32(car + 0x3C), wheels = MEM32(car + 0x40);
    /* 1D5FB7 -> 1CDAD3 -> 1CB424 restores all four wheel parameters and
     * the drivetrain as well as the visible damage state. */
    if (!mapped(simulation, 0xB0) || !mapped(wheels, 16) ||
        !mapped(MEM32(simulation + 0x4C), 0x14) ||
        !mapped(MEM32(simulation + 0x3C), 0x14)) return 0;
    for (unsigned i = 0; i < 4; ++i)
        if (!mapped(MEM32(wheels + i * 4), 0xFC)) return 0;
    return player;
}

static int teleport(uint32_t player, uint32_t vehicle, uint32_t car,
                    uint32_t body, const float xyz[3])
{
    uint32_t wheels = MEM32(car + 0x40);
    if (!mapped(MEM32(player + 0x1AC), 0x1504C) || !mapped(wheels, 4) ||
        !mapped(MEM32(wheels), 0x100)) return 0;
    uint32_t regs[] = {g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi, g_seh_ebp, g_ebp};
    double fp[8];
    int top = g_fp_top, cmp = g_fp_cmp, df = g_df;
    uint16_t cc = g_fp_cc, control = g_fp_control_word;
    RecompXmm xmm[] = {g_xmm0, g_xmm1, g_xmm2, g_xmm3, g_xmm4, g_xmm5, g_xmm6, g_xmm7};
    uint32_t point = g_esp - 0x100u;
    float heading = isfinite(destination_heading) ? destination_heading
                                                   : atan2f(MEMF(body + 0xBC), MEMF(body + 0xC4));
    int applied;
    (void)vehicle; (void)car;
    memcpy(fp, g_fp_stack, sizeof(fp));
    /* Native 21ADF3 finds ground, clears motion, aligns to the slope, applies
     * car clearance, and updates the cached transform and reset flags. */
    for (unsigned i = 0; i < 3; ++i) MEMF(point + i * 4) = xyz[i];
    g_esp = point - 16;
    g_esp -= 4; MEMF(g_esp) = heading;
    PUSH32(g_esp, 0); /* full-world vertical ray; supplied Y is ignored */
    PUSH32(g_esp, point);
    PUSH32(g_esp, player);
    PUSH32(g_esp, 0);
    sub_0021ADF3();
    applied = (g_eax & 0xFF) != 0;
    g_eax = regs[0]; g_ecx = regs[1]; g_edx = regs[2]; g_esp = regs[3];
    g_ebx = regs[4]; g_esi = regs[5]; g_edi = regs[6]; g_seh_ebp = regs[7]; g_ebp = regs[8];
    memcpy(g_fp_stack, fp, sizeof(fp));
    g_fp_top = top; g_fp_cmp = cmp; g_fp_cc = cc; g_fp_control_word = control; g_df = df;
    g_xmm0 = xmm[0]; g_xmm1 = xmm[1]; g_xmm2 = xmm[2]; g_xmm3 = xmm[3];
    g_xmm4 = xmm[4]; g_xmm5 = xmm[5]; g_xmm6 = xmm[6]; g_xmm7 = xmm[7];
    return applied;
}

static void sample_car(uint32_t player)
{
    /* body_for without its player-vehicle vtable check: AI opponents may
     * drive through another vehicle interface with the same car layout. */
    uint32_t vehicle, car, simulation, body;
    unsigned i, slot = 16;
    if (!mapped(player, 0x7A0)) return;
    vehicle = MEM32(player + 0x19C);
    if (!mapped(vehicle, 0x2C)) return;
    car = MEM32(vehicle + 8);
    if (!mapped(car, 0x48)) return;
    simulation = MEM32(car + 0x3C);
    if (!mapped(simulation, 8)) return;
    body = MEM32(simulation + 4);
    if (!mapped(body, 0x1C4)) return;
    AcquireSRWLockExclusive(&lock);
    for (i = 0; i < 16; ++i) {
        if (cars[i].player == player || GetTickCount() - cars[i].tick > 2000) { slot = i; break; }
    }
    if (slot < 16) {
        cars[slot].player = player;
        cars[slot].vtable = MEM32(vehicle);
        cars[slot].x = MEMF(body + 0xC8); cars[slot].z = MEMF(body + 0xD0);
        cars[slot].fx = MEMF(body + 0xBC); cars[slot].fz = MEMF(body + 0xC4);
        cars[slot].tick = GetTickCount();
    }
    ReleaseSRWLockExclusive(&lock);
}

void sub_002203E5(void)
{
    uint32_t primary = primary_player();
    if (primary && g_ecx == primary) {
        uint32_t car, vehicle;
        uint32_t body = body_for(primary, &car, &vehicle);
        int active = body && isfinite(MEMF(body + 0xC8)) &&
            isfinite(MEMF(body + 0xCC)) && isfinite(MEMF(body + 0xD0));
        InterlockedExchange(&keyboard_sampled, (LONG)GetTickCount());
        InterlockedExchange(&keyboard_active, active);
    }
    if (enabled) sample_car(g_ecx);
    if (enabled && g_ecx == primary) {
        uint32_t car = 0, vehicle = 0, player = g_ecx;
        uint32_t body = body_for(player, &car, &vehicle);
        AcquireSRWLockExclusive(&lock);
        available = body != 0;
        sampled_player = player;
        sampled_car = body ? car : 0;
        sampled_world = body ? MEM32(player + 0x1AC) : 0;
        if (body) {
            float current[3];
            for (unsigned i = 0; i < 3; ++i) current[i] = MEMF(body + 0xC8 + i * 4);
            available = isfinite(current[0]) && isfinite(current[1]) && isfinite(current[2]);
            if (available) {
                memcpy(position, current, sizeof(current));
                heading_now = atan2f(MEMF(body + 0xBC), MEMF(body + 0xC4));
            }
        }
        sampled = GetTickCount();
        if (pending) {
            if ((LONG)(GetTickCount() - pending_until) >= 0)
                strcpy_s(result, sizeof(result), "game update timeout");
            else if (player != requested_player || car != requested_car || sampled_world != requested_world)
                strcpy_s(result, sizeof(result), "active car or world changed; teleport cancelled");
            else if (available && mapped(g_esp - 0x400, 0x400)) {
                if (teleport(player, vehicle, car, body, destination)) {
                    for (unsigned i = 0; i < 3; ++i) position[i] = MEMF(body + 0xC8 + i * 4);
                    strcpy_s(result, sizeof(result), "applied");
                } else strcpy_s(result, sizeof(result), "no usable ground at destination");
            } else strcpy_s(result, sizeof(result), "no active car");
            completed = pending;
            pending = 0;
            SetEvent(finished);
        }
        ReleaseSRWLockExclusive(&lock);
    }
    sub_002203E5_original();
    mm3_mods_player_update();
}

static int request_teleport(const float xyz[3], float heading, char *reply, size_t size)
{
    int applied;
    REQUEST_LOCK();
    AcquireSRWLockExclusive(&lock);
    if (pending) {
        snprintf(reply, size, "{\"result\":\"another teleport is pending\"}");
        ReleaseSRWLockExclusive(&lock);
        REQUEST_UNLOCK();
        return 0;
    }
    if (available && GetTickCount() - sampled < 500) {
        unsigned request = ++serial;
        memcpy(destination, xyz, sizeof(destination));
        destination_heading = heading;
        ResetEvent(finished);
        pending = request;
        pending_until = GetTickCount() + 2000;
        requested_player = sampled_player; requested_car = sampled_car;
        requested_world = sampled_world;
        ReleaseSRWLockExclusive(&lock);
        WaitForSingleObject(finished, 2000);
        AcquireSRWLockExclusive(&lock);
        if (completed != request) {
            pending = 0; /* cancel; a timed-out request cannot run later */
            strcpy_s(result, sizeof(result), "game update timeout");
        }
    } else strcpy_s(result, sizeof(result), available ? "player update is paused or stalled" : "no active car");
    snprintf(reply, size, "{\"result\":\"%s\",\"x\":%.9g,\"y\":%.9g,\"z\":%.9g}",
             result, position[0], position[1], position[2]);
    applied = strcmp(result, "applied") == 0;
    ReleaseSRWLockExclusive(&lock);
    REQUEST_UNLOCK();
    return applied;
}

void mm3_teleport_state(Mm3TeleportState *state)
{
    AcquireSRWLockShared(&lock);
    memcpy(state->position, position, sizeof position);
    state->heading = heading_now;
    state->player = sampled_player;
    state->car = sampled_car;
    state->world = sampled_world;
    state->available = enabled && available && GetTickCount() - sampled < 300;
    state->pending = pending != 0;
    strcpy_s(state->result, sizeof state->result, result);
    ReleaseSRWLockShared(&lock);
}

int mm3_teleport_queue(float x, float z, float heading)
{
    int accepted = 0;
    if (!isfinite(x) || !isfinite(z) || (!isfinite(heading) && !isnan(heading))) return 0;
    if (!REQUEST_TRY_LOCK()) return 0;
    AcquireSRWLockExclusive(&lock);
    if (enabled && available && GetTickCount() - sampled < 300 && !pending) {
        destination[0] = x; destination[1] = 0; destination[2] = z;
        destination_heading = heading;
        pending = ++serial;
        pending_until = GetTickCount() + 500;
        requested_player = sampled_player; requested_car = sampled_car;
        requested_world = sampled_world;
        strcpy_s(result, sizeof result, "queued");
        accepted = 1;
    }
    ReleaseSRWLockExclusive(&lock);
    REQUEST_UNLOCK();
    return accepted;
}

/* One request in, one JSON reply out. Both transports (pipe on Windows,
 * Unix socket elsewhere) call this, so the protocol lives in one place. */
static void teleport_answer(const char *command, char *reply, size_t size)
{
    char extra;
    unsigned peek_at = 0, peek_len = 0;
    float xyz[3];
    xyz[1] = 0;
    float heading = NAN;
    int fields = sscanf(command, "teleport %f %f %f %c", &xyz[0], &xyz[2], &heading, &extra);
    if ((fields == 2 || (fields == 3 && isfinite(heading))) &&
        isfinite(xyz[0]) && isfinite(xyz[2])) {
        request_teleport(xyz, fields == 3 ? heading : NAN, reply, size);
    } else if (strcmp(command, "cars") == 0) {
        uint32_t primary = primary_player();
        size_t n = (size_t)snprintf(reply, size, "{\"cars\":[");
        AcquireSRWLockShared(&lock);
        for (unsigned i = 0; i < 16 && n < size - 160; ++i) {
            if (!cars[i].player || GetTickCount() - cars[i].tick > 500) continue;
            n += (size_t)snprintf(reply + n, size - n,
                "%s{\"id\":%u,\"vt\":%u,\"player\":%s,\"x\":%.6g,\"z\":%.6g,\"fx\":%.6g,\"fz\":%.6g}",
                reply[n - 1] == '[' ? "" : ",", cars[i].player, cars[i].vtable,
                cars[i].player == primary ? "true" : "false",
                cars[i].x, cars[i].z, cars[i].fx, cars[i].fz);
        }
        ReleaseSRWLockShared(&lock);
        snprintf(reply + n, size - n, "]}");
    } else if (sscanf(command, "peek %x %x", &peek_at, &peek_len) == 2) {
        /* Guest memory as hex, for working out a structure. */
        size_t n = 0;
        if (peek_len > 512) peek_len = 512;
        if (mapped(peek_at, peek_len))
            for (unsigned i = 0; i < peek_len && n + 3 < size; i++)
                n += (size_t)snprintf(reply + n, size - n, "%02X", MEM8(peek_at + i));
        reply[n] = 0;
    } else if (strcmp(command, "position") == 0) {
        AcquireSRWLockShared(&lock);
        snprintf(reply, size, "{\"available\":%s,\"x\":%.9g,\"y\":%.9g,\"z\":%.9g}",
            available && GetTickCount() - sampled < 500 ? "true" : "false",
            position[0], position[1], position[2]);
        ReleaseSRWLockShared(&lock);
    } else strcpy_s(reply, size, "{\"error\":\"use position or teleport X Z [heading radians]; height is automatic; coordinates must be finite\"}");
}

#if defined(_WIN32)
static DWORD WINAPI pipe_thread(void *unused)
{
    char name[80];
    (void)unused;
    snprintf(name, sizeof(name), "\\\\.\\pipe\\MM3Teleport-%lu", GetCurrentProcessId());
    for (;;) {
        HANDLE pipe = CreateNamedPipeA(name, PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
            1, 2048, 2048, 0, NULL);
        if (pipe == INVALID_HANDLE_VALUE) return 1;
        if (ConnectNamedPipe(pipe, NULL) || GetLastError() == ERROR_PIPE_CONNECTED) {
            char command[128] = {0}, reply[2048];
            DWORD bytes;
            if (ReadFile(pipe, command, sizeof(command) - 1, &bytes, NULL)) {
                command[bytes] = 0;
                teleport_answer(command, reply, sizeof(reply));
                if (WriteFile(pipe, reply, (DWORD)strlen(reply), &bytes, NULL))
                    FlushFileBuffers(pipe); /* do not discard an unread reply on disconnect */
            }
        }
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
    }
}
#else
/* $XDG_RUNTIME_DIR/MM3Teleport-<pid>.sock, else /tmp. Set once at init. */
static char socket_path[sizeof(((struct sockaddr_un *)0)->sun_path)];   /* 108 on Linux, 104 on macOS */

static void socket_unlink(void)
{
    unlink(socket_path);
}

static int socket_path_init(void)
{
    const char *dir = getenv("XDG_RUNTIME_DIR");
    int n;
    if (!dir || !*dir) dir = "/tmp";
    n = snprintf(socket_path, sizeof(socket_path), "%s/MM3Teleport-%lu.sock",
                 dir, (unsigned long)getpid());
    if (n < 0 || (size_t)n >= sizeof(socket_path)) return 0;
    atexit(socket_unlink);
    return 1;
}

/* Request: bytes up to the first newline, or until the client half-closes.
 * Reply: the JSON, then the connection closes (the client reads to EOF). */
static void serve_connection(int fd)
{
    char command[128] = {0}, reply[2048];
    size_t used = 0, sent = 0, len;
    ssize_t got;
    /* A client that sends neither a newline nor a half-close must not hold the
     * single accept loop; the timeout drops it. */
    struct timeval idle = {2, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &idle, sizeof idle);
    while (used < sizeof(command) - 1 &&
           (got = recv(fd, command + used, sizeof(command) - 1 - used, 0)) != 0) {
        if (got < 0) {
            if (errno == EINTR) continue;
            break;
        }
        used += (size_t)got;
        if (memchr(command, '\n', used)) break;
    }
    command[used] = 0;
    command[strcspn(command, "\r\n")] = 0;
    teleport_answer(command, reply, sizeof(reply));
    len = strlen(reply);
    while (sent < len) {
        ssize_t w = send(fd, reply + sent, len - sent, MSG_NOSIGNAL);
        if (w < 0 && errno == EINTR) continue;
        if (w <= 0) break;
        sent += (size_t)w;
    }
}

static DWORD WINAPI socket_thread(void *unused)
{
    struct sockaddr_un addr;
    int listener;
    (void)unused;
    listener = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listener < 0) return 1;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", socket_path);
    unlink(socket_path); /* stale file, if a dead process left one behind */
    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) != 0 || listen(listener, 4) != 0) {
        close(listener);
        return 1;
    }
    for (;;) {
        int fd = accept(listener, NULL, NULL);
        if (fd < 0) {
            if (errno == EINTR || errno == ECONNABORTED) continue;
            close(listener);
            return 1;
        }
        serve_connection(fd);
        close(fd);
    }
}
#endif

void mm3_debug_teleport_init(void)
{
    const char *opt = getenv("MM3_DEBUG_TELEPORT");
    const char *overlay = getenv("MM3_DEBUG_OVERLAY");
    HANDLE thread;
    int with_overlay = overlay && *overlay && strcmp(overlay, "0") != 0;   /* as mm3_mod_on */
    if ((!opt || strcmp(opt, "1") != 0) && !with_overlay) return;
#if !defined(_WIN32)
    if (!socket_path_init()) return;
#endif
    finished = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (!finished) return;
    enabled = 1;
#if defined(_WIN32)
    thread = CreateThread(NULL, 0, pipe_thread, NULL, 0, NULL);
#else
    thread = CreateThread(NULL, 0, socket_thread, NULL, 0, NULL);
#endif
    if (!thread) { enabled = 0; CloseHandle(finished); return; }
    CloseHandle(thread);
}
