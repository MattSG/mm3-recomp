#!/usr/bin/env python3
"""Print or teleport the player car in a running MM3 recomp process.

Usage:
  python tools/scripts/send_teleport.py --process-id PID
  python tools/scripts/send_teleport.py --process-id PID --x 12.5 --z -40 [--heading 0.3]

Without --x/--z it prints the current position as JSON. With them it teleports
the car (heading in radians, the game's convention; omitted keeps the current
one). Requires MM3_DEBUG_TELEPORT=1 in the game's environment.

The game serves MM3Teleport-<pid> (src/mods/teleport.c): a Win32 named pipe on
Windows, and on other systems a Unix socket at $XDG_RUNTIME_DIR/MM3Teleport-<pid>.sock
(else /tmp/MM3Teleport-<pid>.sock). Both speak the same protocol.

Replies are printed as JSON
rather than a PowerShell object.
"""
import argparse
import json
import math
import os
import socket
import threading
import time


def die(msg):
    raise SystemExit(msg)


def send_command(pid, command):
    """Send one debug command to the game and return the decoded JSON reply."""
    if os.name == "nt":
        return _send_pipe(pid, command)
    return _send_socket(pid, command)


def socket_path(pid):
    """The game's Unix socket path; same lookup order as teleport.c."""
    return os.path.join(os.environ.get("XDG_RUNTIME_DIR") or "/tmp", f"MM3Teleport-{pid}.sock")


def _send_socket(pid, command):
    """POSIX: one request, then read to EOF (the game closes after its reply)."""
    path = socket_path(pid)
    deadline = time.monotonic() + 3.0
    while True:
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        try:
            sock.connect(path)
            break
        except OSError:
            sock.close()
            if time.monotonic() >= deadline:
                raise RuntimeError(f"could not connect to {path}")
            time.sleep(0.05)
    chunks = []
    try:
        sock.settimeout(5.0)
        sock.sendall(command.encode("ascii"))
        sock.shutdown(socket.SHUT_WR)  # end of request
        while True:
            chunk = sock.recv(4096)
            if not chunk:
                break
            chunks.append(chunk)
    except socket.timeout:
        raise RuntimeError("MM3 debug response timed out.")
    finally:
        sock.close()
    if not chunks:
        raise RuntimeError("MM3 debug response was empty.")
    return json.loads(b"".join(chunks).decode("ascii"))


def _send_pipe(pid, command):
    """Windows: one message over the game's named pipe."""
    path = rf"\\.\pipe\MM3Teleport-{pid}"
    deadline = time.monotonic() + 3.0
    while True:
        try:
            pipe = open(path, "r+b", buffering=0)
            break
        except OSError:
            if time.monotonic() >= deadline:
                raise RuntimeError(f"could not connect to {path}")
            time.sleep(0.05)
    try:
        pipe.write(command.encode("ascii"))
        box = {}
        reader = threading.Thread(target=lambda: box.update(data=pipe.read(4096)), daemon=True)
        reader.start()
        reader.join(5.0)
        if reader.is_alive():
            raise RuntimeError("MM3 debug response timed out.")
        if not box.get("data"):
            raise RuntimeError("MM3 debug response was empty.")
        return json.loads(box["data"].decode("ascii"))
    finally:
        pipe.close()


def teleport(pid, x=None, z=None, heading=None):
    """Query the position (x and z None) or teleport; returns the reply dict."""
    if x is None and z is None:
        command = "position"
    else:
        command = f"teleport {x!r} {z!r}"
        if heading is not None:
            command += f" {heading!r}"
    reply = send_command(pid, command)
    if reply.get("error"):
        raise RuntimeError(reply["error"])
    if x is not None and reply.get("result") != "applied":
        raise RuntimeError(f"Teleport failed: {reply.get('result')}")
    return reply


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--process-id", dest="process_id", type=int, required=True)
    parser.add_argument("--x", type=float)
    parser.add_argument("--z", type=float)
    parser.add_argument("--heading", type=float, help="car heading in radians; omitted keeps the current one")
    args = parser.parse_args()
    if (args.x is None) != (args.z is None):
        die("--x and --z must be given together")
    if args.heading is not None and args.x is None:
        die("--heading needs --x and --z")
    for value in (args.x, args.z):
        if value is not None and not math.isfinite(value):
            die("Coordinates must be finite.")
    if args.heading is not None and not math.isfinite(args.heading):
        die("Heading must be finite.")
    try:
        reply = teleport(args.process_id, args.x, args.z, args.heading)
    except RuntimeError as e:
        die(str(e))
    print(json.dumps(reply))


if __name__ == "__main__":
    main()
