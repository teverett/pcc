"""Waits for a qemu guest to settle into its end-of-program spin loop,
then prints its final `info registers` dump.

Usage: qemu_wait.py MONITOR_SOCKET TIMEOUT_SECONDS [REQUIRED_PATTERN]

MONITOR_SOCKET is the unix socket qemu was started with
(`-monitor unix:PATH,server,nowait`). Every test program ends by
spinning in place (a branch-to-self, or `hlt` plus a jump back to it),
so the machine is finished once the register dump stops changing,
apart from flipping between the few states of that loop. Each sample
is one `info registers` round trip over a single connection, so
sampling every SAMPLE_INTERVAL costs almost nothing.

A program still computing changes its program counter on almost
every sample, so WINDOW consecutive samples with at most MAX_STATES
distinct dumps marks it done. The window spans several timer-interrupt
periods, so firmware idling in `hlt` while waiting for an interrupt
(the 8086 BIOS) is seen changing state rather than mistaken for the
end. REQUIRED_PATTERN, a regular expression, must also match the
dump before it counts as settled; the 8086 uses it to tell its program
(code segment 0) from BIOS code that can idle long enough to look
settled. On timeout the last dump is printed anyway, and the caller's
own result checks report the failure.
"""

import re
import socket
import sys
import time

SAMPLE_INTERVAL = 0.05
WINDOW = 12
MAX_STATES = 3
PROMPT = b"(qemu) "


def read_until_prompt(sock, deadline):
    data = b""
    while not data.endswith(PROMPT):
        sock.settimeout(max(0.01, deadline - time.monotonic()))
        chunk = sock.recv(65536)
        if not chunk:
            raise ConnectionError("monitor closed")
        data += chunk
    return data


def connect(path, deadline):
    while True:
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        try:
            sock.connect(path)
            return sock
        except OSError:
            sock.close()
            if time.monotonic() >= deadline:
                raise
            time.sleep(SAMPLE_INTERVAL)


def main():
    path, timeout = sys.argv[1], float(sys.argv[2])
    required = re.compile(sys.argv[3].encode()) if len(sys.argv) > 3 else None
    deadline = time.monotonic() + timeout
    last = b""
    try:
        sock = connect(path, deadline)
        read_until_prompt(sock, deadline)  # banner
        recent = []
        while time.monotonic() < deadline:
            sock.sendall(b"info registers\n")
            last = read_until_prompt(sock, deadline)
            recent = (recent + [last])[-WINDOW:]
            if (len(recent) == WINDOW and len(set(recent)) <= MAX_STATES
                    and (required is None or all(required.search(d) for d in recent))):
                break
            time.sleep(SAMPLE_INTERVAL)
    except (OSError, ConnectionError):
        pass
    sys.stdout.write(last.decode("latin-1"))


if __name__ == "__main__":
    main()
