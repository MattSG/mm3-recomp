/* Local control server: one request line in, one reply out, per connection.
 *
 * Windows serves a message-mode named pipe, \\.\pipe\<name>-<pid>. Other hosts
 * serve a Unix stream socket, $XDG_RUNTIME_DIR/<name>-<pid>.sock (else /tmp),
 * removed at exit. A request is read up to the first newline, or until the
 * client half-closes; the reply is written and the connection closed. The
 * answer callback owns the protocol. */
#ifndef PLATFORM_LINE_SERVER_H
#define PLATFORM_LINE_SERVER_H

#include <stddef.h>

typedef void (*LineServerAnswer)(const char *request, char *reply, size_t size);

/* Starts the server thread. Returns 1 when it is running. */
int line_server_start(const char *name, LineServerAnswer answer);

#endif
