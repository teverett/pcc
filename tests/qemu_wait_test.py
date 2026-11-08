"""Tests for qemu_wait.py against a fake qemu monitor.

Run: python3 tests/qemu_wait_test.py
"""

import os
import socket
import subprocess
import sys
import tempfile
import threading
import time
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
WAIT = os.path.join(HERE, "qemu_wait.py")


def serve(path, dump_for, commands=None):
    """Answers each `info registers` with dump_for(n), n counting from 0,
    and any other command with just the prompt, recording every command
    in `commands` if given."""
    server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    server.bind(path)
    server.listen(1)

    def run():
        conn, _ = server.accept()
        conn.sendall(b"QEMU monitor\r\n(qemu) ")
        n = 0
        buf = b""
        try:
            while True:
                chunk = conn.recv(4096)
                if not chunk:
                    break
                buf += chunk
                while b"\n" in buf:
                    cmd, buf = buf.split(b"\n", 1)
                    if commands is not None:
                        commands.append(cmd.decode())
                    if cmd == b"info registers":
                        conn.sendall(dump_for(n).encode() + b"\r\n(qemu) ")
                        n += 1
                    else:
                        conn.sendall(b"(qemu) ")
        except OSError:
            pass
        conn.close()
        server.close()

    threading.Thread(target=run, daemon=True).start()


def run_wait(path, timeout, *extra):
    start = time.monotonic()
    out = subprocess.run([sys.executable, "-I", WAIT, path, str(timeout), *extra],
                         capture_output=True, text=True, check=True).stdout
    return out, time.monotonic() - start


class QemuWaitTest(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp(dir="/tmp")
        self.path = os.path.join(self.dir, "m")

    def tearDown(self):
        if os.path.exists(self.path):
            os.unlink(self.path)
        os.rmdir(self.dir)

    def test_returns_final_dump_once_registers_settle(self):
        serve(self.path, lambda n: "R00=%08x PC=%04x" % (min(n, 5), 0x100 + min(n, 5)))
        out, elapsed = run_wait(self.path, 10)
        self.assertIn("R00=00000005 PC=0105", out)
        self.assertLess(elapsed, 5)

    def test_settles_on_a_loop_alternating_between_two_states(self):
        serve(self.path, lambda n: "R00=00000007 PC=%04x" % (0x200 + n % 2))
        out, elapsed = run_wait(self.path, 10)
        self.assertIn("R00=00000007", out)
        self.assertLess(elapsed, 5)

    def test_keeps_waiting_while_the_program_counter_moves(self):
        serve(self.path, lambda n: "R00=00000003 PC=%04x" % (0x300 + n))
        out, elapsed = run_wait(self.path, 2)
        self.assertIn("R00=00000003", out)
        self.assertGreaterEqual(elapsed, 2)

    def test_stops_the_cpu_around_each_read(self):
        commands = []
        serve(self.path, lambda n: "R00=00000001 PC=0100", commands)
        run_wait(self.path, 10)
        self.assertGreaterEqual(len(commands), 6)
        for i in range(0, len(commands) - 2, 3):
            self.assertEqual(commands[i:i + 3], ["stop", "info registers", "cont"])

    def test_a_settled_state_must_last_the_whole_window(self):
        serve(self.path, lambda n: "R00=00000001 PC=0100")
        _, elapsed = run_wait(self.path, 10)
        self.assertGreaterEqual(elapsed, 1.0)

    def test_ignores_settled_states_that_miss_the_required_pattern(self):
        serve(self.path, lambda n: "CS =f000 PC=0010" if n < 30 else "CS =0000 PC=7c11")
        out, _ = run_wait(self.path, 10, "CS =0000 ")
        self.assertIn("CS =0000 PC=7c11", out)

    def test_times_out_when_the_required_pattern_never_matches(self):
        serve(self.path, lambda n: "CS =f000 PC=0010")
        out, elapsed = run_wait(self.path, 2, "CS =0000 ")
        self.assertIn("CS =f000", out)
        self.assertGreaterEqual(elapsed, 2)


if __name__ == "__main__":
    unittest.main()
