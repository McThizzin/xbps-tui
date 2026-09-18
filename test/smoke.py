#!/usr/bin/env python3
"""Drive the TUI under a pty with fake xbps binaries and dump the VT output.

Usage: smoke.py <path-to-xbps-tui-binary>
"""
import fcntl
import os
import pty
import select
import struct
import sys
import termios
import time

BIN = sys.argv[1]
FDIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "bin")


def read_all(fd, timeout):
    chunks = []
    end = time.time() + timeout
    while True:
        r, _, _ = select.select([fd], [], [], 0.2)
        if r:
            try:
                d = os.read(fd, 65536)
            except OSError:
                break
            if not d:
                break
            chunks.append(d)
            end = time.time() + 0.2  # keep draining while data flows
        elif time.time() >= end:
            break
    return b"".join(chunks)


def send(fd, data):
    os.write(fd, data)


pid, fd = pty.fork()
if pid == 0:
    env = dict(os.environ)
    env["TERM"] = "xterm"
    env["PATH"] = FDIR + ":" + env.get("PATH", "")
    env["LINES"] = "24"
    env["COLUMNS"] = "80"
    os.execve(BIN, [BIN], env)

fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", 24, 80, 0, 0))

out = bytearray()

out += read_all(fd, 1.5)   # initial screen
send(fd, b"\x1b[B")        # Down
out += read_all(fd, 0.3)
send(fd, b"\x1b[B")        # Down
out += read_all(fd, 0.3)
send(fd, b"\x0e")          # ^N name filter
out += read_all(fd, 0.3)
send(fd, b"bar")           # filter text
out += read_all(fd, 0.3)
send(fd, b"\n")            # confirm filter
out += read_all(fd, 0.3)
send(fd, b"\x11")          # ^Q quit
time.sleep(0.3)

try:
    while True:
        d = os.read(fd, 65536)
        if not d:
            break
        out += d
except OSError:
    pass

os.close(fd)
sys.stdout.buffer.write(bytes(out))