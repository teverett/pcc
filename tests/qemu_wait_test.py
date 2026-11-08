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


def serve(path, dump_for, commands=None, disconnect_after=None):
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
                        if disconnect_after is not None and n >= disconnect_after:
                            return
                        conn.sendall(dump_for(n).encode() + b"\r\n(qemu) ")
                        n += 1
                    else:
                        conn.sendall(b"(qemu) ")
        except OSError:
            pass
        finally:
            conn.close()
            server.close()

    threading.Thread(target=run, daemon=True).start()


def run_wait(path, timeout, *extra, check=True):
    start = time.monotonic()
    result = subprocess.run([sys.executable, "-I", WAIT, path, str(timeout), *extra],
                            capture_output=True, text=True, check=check)
    return result, time.monotonic() - start


class QemuWaitTest(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp(dir="/tmp")
        self.path = os.path.join(self.dir, "m")

    def tearDown(self):
        if os.path.exists(self.path):
            os.unlink(self.path)
        image = os.path.join(self.dir, "program.bin")
        if os.path.exists(image):
            os.unlink(image)
        for name in ("test.qemu.log", "test.wait.log", "test.out"):
            path = os.path.join(self.dir, name)
            if os.path.exists(path):
                os.unlink(path)
        os.rmdir(self.dir)

    def i8086_image(self):
        path = os.path.join(self.dir, "program.bin")
        with open(path, "wb") as stream:
            stream.write(bytes.fromhex("fa31c08ed88ec08ed0bc007cfbe80000f4e9fdff"))
        return path

    def run_runner(self, *extra):
        with open(os.path.join(HERE, "run_tests.sh")) as stream:
            helper = "wait_for_qemu() {" + stream.read().split(
                "wait_for_qemu() {", 1)[1].split("\n}\n", 1)[0] + "\n}\n"
        with open(os.path.join(self.dir, "test.qemu.log"), "w") as stream:
            stream.write("QEMU diagnostic\n")
        script = helper + """
SCRIPT_DIR=$1
tmpdir=$2
monitor=$3
shift 3
sleep 30 &
qemu_pid=$!
sleep 30 &
watcher=$!
wait_for_qemu test "$monitor" "$tmpdir/test.out" "$qemu_pid" "$watcher" "$@"
"""
        return subprocess.run(["bash", "-c", script, "test", HERE, self.dir,
                               self.path, *extra], capture_output=True, text=True,
                              timeout=10)

    def test_returns_final_dump_once_registers_settle(self):
        serve(self.path, lambda n: "R00=%08x PC=%04x" % (min(n, 5), 0x100 + min(n, 5)))
        out, elapsed = run_wait(self.path, 10)
        self.assertIn("R00=00000005 PC=0105", out.stdout)
        self.assertLess(elapsed, 5)

    def test_settles_on_a_loop_alternating_between_two_states(self):
        serve(self.path, lambda n: "R00=00000007 PC=%04x" % (0x200 + n % 2))
        out, elapsed = run_wait(self.path, 10)
        self.assertIn("R00=00000007", out.stdout)
        self.assertLess(elapsed, 5)

    def test_keeps_waiting_while_the_program_counter_moves(self):
        serve(self.path, lambda n: "R00=00000003 PC=%04x" % (0x300 + n))
        out, elapsed = run_wait(self.path, 2, check=False)
        self.assertNotEqual(out.returncode, 0)
        self.assertIn("timed out", out.stderr)
        self.assertIn("R00=00000003", out.stdout)
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
        self.assertIn("CS =0000 PC=7c11", out.stdout)

    def test_times_out_when_the_required_pattern_never_matches(self):
        serve(self.path, lambda n: "CS =f000 PC=0010")
        out, elapsed = run_wait(self.path, 2, "CS =0000 ", check=False)
        self.assertNotEqual(out.returncode, 0)
        self.assertIn("timed out", out.stderr)
        self.assertIn("CS =f000", out.stdout)
        self.assertGreaterEqual(elapsed, 2)

    def test_monitor_disconnect_reports_failure_and_preserves_last_dump(self):
        serve(self.path, lambda n: "R00=00000001 PC=0100", disconnect_after=1)
        out, _ = run_wait(self.path, 10, check=False)
        self.assertNotEqual(out.returncode, 0)
        self.assertIn("monitor closed", out.stderr)
        self.assertIn("R00=00000001", out.stdout)

    def test_missing_monitor_reports_connection_failure(self):
        out, _ = run_wait(self.path, 0.2, check=False)
        self.assertNotEqual(out.returncode, 0)
        self.assertIn("monitor failure", out.stderr)
        self.assertEqual(out.stdout, "")

    def test_8086_startup_does_not_stop_the_bios(self):
        commands = []
        image = self.i8086_image()
        serve(self.path, lambda n: "EIP=0000ff00 CS =f000 " if n < 3
              else "EAX=0000002e EIP=00007c11 CS =0000 ", commands)
        out, elapsed = run_wait(self.path, 10, "--8086-image", image)
        self.assertEqual(commands[:4], ["info registers"] * 4)
        self.assertEqual(commands[4:], ["stop", "info registers"])
        self.assertGreaterEqual(elapsed, 1.5)
        self.assertIn("EAX=0000002e", out.stdout)

    def test_8086_stable_nonterminal_program_location_is_not_completion(self):
        image = self.i8086_image()
        serve(self.path, lambda n: "EAX=0000f9b6 EIP=00007c80 CS =0000 ")
        out, _ = run_wait(self.path, 1.2, "--8086-image", image, check=False)
        self.assertNotEqual(out.returncode, 0)
        self.assertIn("timed out", out.stderr)
        self.assertIn("EIP=00007c80", out.stdout)

    def test_8086_completion_is_rechecked_with_cpu_stopped(self):
        image = self.i8086_image()
        commands = []
        serve(self.path, lambda n: "EIP=0000ff00 CS =f000 " if n == 1
              else "EAX=0000000b EIP=00007c11 CS =0000 ", commands)
        out, _ = run_wait(self.path, 10, "--8086-image", image)
        self.assertEqual(commands, ["info registers", "stop", "info registers", "cont",
                                    "info registers", "stop", "info registers"])
        self.assertIn("EAX=0000000b", out.stdout)

    def test_8086_rejects_unknown_entry_sequence(self):
        image = self.i8086_image()
        with open(image, "wb") as stream:
            stream.write(b"\0" * 19)
        out, _ = run_wait(self.path, 10, "--8086-image", image, check=False)
        self.assertNotEqual(out.returncode, 0)
        self.assertIn("unrecognized entry sequence", out.stderr)

    def test_8086_accepts_short_branch_to_self(self):
        image = self.i8086_image()
        with open(image, "wb") as stream:
            stream.write(bytes.fromhex("fa31c08ed88ec08ed0bc007cfbe80000f4ebfe"))
        serve(self.path, lambda n: "EAX=0000002a EIP=00007c11 CS =0000 ")
        out, _ = run_wait(self.path, 10, "--8086-image", image)
        self.assertIn("EAX=0000002a", out.stdout)

    def test_runner_does_not_accept_result_after_monitor_disconnect(self):
        serve(self.path, lambda n: "EAX=0000002a PC=0100", disconnect_after=1)
        out = self.run_runner()
        self.assertNotEqual(out.returncode, 0)
        self.assertIn("qemu execution failed", out.stdout)
        self.assertIn("monitor closed", out.stdout)
        self.assertIn("EAX=0000002a", out.stdout)
        self.assertIn("QEMU diagnostic", out.stdout)

    def test_runner_reports_invalid_image_and_qemu_diagnostics(self):
        image = self.i8086_image()
        with open(image, "wb") as stream:
            stream.write(b"\0" * 20)
        out = self.run_runner("--8086-image", image)
        self.assertNotEqual(out.returncode, 0)
        self.assertIn("qemu execution failed", out.stdout)
        self.assertIn("unrecognized entry sequence", out.stdout)
        self.assertIn("QEMU diagnostic", out.stdout)


if __name__ == "__main__":
    unittest.main()
