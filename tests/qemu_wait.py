"""Waits for a qemu guest to settle into its end-of-program spin loop,
then prints its final `info registers` dump.

Usage: qemu_wait.py MONITOR_SOCKET TIMEOUT_SECONDS [REQUIRED_PATTERN]
                   [--8086-image IMAGE]

MONITOR_SOCKET is the unix socket qemu was started with
(`-monitor unix:PATH,server,nowait`). Every test program ends by
spinning in place (a branch-to-self, or `hlt` plus a jump back to it),
so the machine is finished once the register dump stops changing,
apart from flipping between the few states of that loop.

Each sample stops the CPU (`stop`), reads `info registers` and resumes
it (`cont`): without the stop, software emulation can report register
state last saved before a long stretch of chained translated code, so a
running program could look frozen. A program still computing changes
its program counter on almost every sample, so samples spanning
WINDOW_SECONDS with at most MAX_STATES distinct dumps mark it done. The
window is long enough to ride out the CPU not being scheduled for a
moment on a busy machine, and spans many timer-interrupt periods, so
firmware idling in `hlt` while waiting for an interrupt (the 8086
BIOS) is seen changing state rather than mistaken for the end. REQUIRED_PATTERN, a regular expression, must also match the
dump before it counts as settled.

With --8086-image, the entry sequence is checked against IMAGE and its
final branch-to-self address is used instead of register stability.
Startup samples do not stop the CPU and are spaced farther apart so
BIOS timers and disk reads can progress without monitor interruptions.
A candidate completion is confirmed with the CPU stopped.

Timeouts and monitor failures return nonzero, with a diagnostic on stderr
and the last register dump on stdout.
"""

import argparse
import re
import socket
import sys
import time

SAMPLE_INTERVAL = 0.05
WINDOW_SECONDS = 1.0
MAX_STATES = 3
PROMPT = b"(qemu) "
STARTUP_INTERVAL = 0.5


def i8086_end_pattern(image):
    with open(image, "rb") as stream:
        entry = stream.read(20)
    prefix = bytes.fromhex("fa 31 c0 8e d8 8e c0 8e d0 bc 00 7c fb e8")
    if (entry[:14] != prefix or
            not (entry[16:19] == bytes.fromhex("f4 eb fe") or
                 entry[16:20] == bytes.fromhex("f4 e9 fd ff"))):
        raise ValueError("8086 image has an unrecognized entry sequence")
    return re.compile(rb"(?=.*\bEIP=00007c11\b)(?=.*\bCS =0000 )", re.DOTALL | re.I)


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
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("monitor_socket")
    parser.add_argument("timeout_seconds", type=float)
    parser.add_argument("required_pattern", nargs="?")
    parser.add_argument("--8086-image", dest="i8086_image")
    args = parser.parse_args()
    if args.timeout_seconds <= 0:
        parser.error("timeout must be positive")
    required = re.compile(args.required_pattern.encode()) if args.required_pattern else None
    try:
        end = i8086_end_pattern(args.i8086_image) if args.i8086_image else None
    except (OSError, ValueError) as error:
        print(f"qemu_wait: {error}", file=sys.stderr)
        return 1
    deadline = time.monotonic() + args.timeout_seconds
    last = b""
    try:
        with connect(args.monitor_socket, deadline) as sock:
            read_until_prompt(sock, deadline)  # banner
            recent = []  # (time, dump), oldest first
            while time.monotonic() < deadline:
                if end is None:
                    sock.sendall(b"stop\n")
                    read_until_prompt(sock, deadline)
                sock.sendall(b"info registers\n")
                last = read_until_prompt(sock, deadline)
                if end is not None and end.search(last):
                    sock.sendall(b"stop\n")
                    read_until_prompt(sock, deadline)
                    sock.sendall(b"info registers\n")
                    last = read_until_prompt(sock, deadline)
                    if end.search(last) and (required is None or required.search(last)):
                        sys.stdout.write(last.decode("latin-1"))
                        return 0
                    sock.sendall(b"cont\n")
                    read_until_prompt(sock, deadline)
                elif end is None:
                    sock.sendall(b"cont\n")
                    read_until_prompt(sock, deadline)
                    now = time.monotonic()
                    recent.append((now, last))
                    while len(recent) > 1 and now - recent[1][0] >= WINDOW_SECONDS:
                        recent.pop(0)
                    dumps = [d for _, d in recent]
                    if (now - recent[0][0] >= WINDOW_SECONDS and len(set(dumps)) <= MAX_STATES
                            and (required is None or all(required.search(d) for d in dumps))):
                        sys.stdout.write(last.decode("latin-1"))
                        return 0
                interval = STARTUP_INTERVAL if end is not None else SAMPLE_INTERVAL
                time.sleep(min(interval, max(0, deadline - time.monotonic())))
        print("qemu_wait: timed out waiting for program completion", file=sys.stderr)
    except socket.timeout:
        print("qemu_wait: timed out communicating with monitor", file=sys.stderr)
    except (OSError, ConnectionError) as error:
        print(f"qemu_wait: monitor failure: {error}", file=sys.stderr)
    sys.stdout.write(last.decode("latin-1"))
    return 1


if __name__ == "__main__":
    sys.exit(main())
