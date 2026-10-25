#include "line_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include "platform/xbox_winnt.h"
#include <errno.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#endif

#if !defined(_WIN32)
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
#endif

static LineServerAnswer s_answer;
#if defined(_WIN32)
static char s_pipe_name[96];
#endif

#if defined(_WIN32)
static DWORD WINAPI pipe_thread(void *unused)
{
    for (;;) {
        HANDLE pipe = CreateNamedPipeA(s_pipe_name, PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
            1, 2048, 2048, 0, NULL);
        if (pipe == INVALID_HANDLE_VALUE) return 1;
        if (ConnectNamedPipe(pipe, NULL) || GetLastError() == ERROR_PIPE_CONNECTED) {
            char command[128] = {0}, reply[2048];
            DWORD bytes;
            if (ReadFile(pipe, command, sizeof(command) - 1, &bytes, NULL)) {
                command[bytes] = 0;
                s_answer(command, reply, sizeof(reply));
                if (WriteFile(pipe, reply, (DWORD)strlen(reply), &bytes, NULL))
                    FlushFileBuffers(pipe); /* do not discard an unread reply on disconnect */
            }
        }
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
    }
}
#else
static char socket_path[sizeof(((struct sockaddr_un *)0)->sun_path)];

static void socket_unlink(void)
{
    unlink(socket_path);
}

static int socket_path_init(const char *name)
{
    const char *dir = getenv("XDG_RUNTIME_DIR");
    int n;
    if (!dir || !*dir) dir = "/tmp";
    n = snprintf(socket_path, sizeof(socket_path), "%s/%s-%lu.sock",
                 dir, name, (unsigned long)getpid());
    if (n < 0 || (size_t)n >= sizeof(socket_path)) return 0;
    atexit(socket_unlink);
    return 1;
}

/* Request: bytes up to the first newline, or until the client half-closes.
 * Reply: the text, then the connection closes (the client reads to EOF). */
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
    s_answer(command, reply, sizeof(reply));
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

int line_server_start(const char *name, LineServerAnswer answer)
{
    HANDLE thread;
    s_answer = answer;
#if defined(_WIN32)
    snprintf(s_pipe_name, sizeof(s_pipe_name), "\\\\.\\pipe\\%s-%lu", name, GetCurrentProcessId());
    thread = CreateThread(NULL, 0, pipe_thread, NULL, 0, NULL);
#else
    if (!socket_path_init(name)) return 0;
    thread = CreateThread(NULL, 0, socket_thread, NULL, 0, NULL);
#endif
    if (!thread) return 0;
    CloseHandle(thread);
    return 1;
}
