"""Verifies main.bin returns 45 under a real qemu-system-m68k -M none -cpu
m68000, the same way tests/run_tests.sh's own check_68000() does: a bare
CPU with no boot ROM/vector table, loaded at 0x1000 with the `loader`
device forcing the CPU's PC there directly, then polled through
tests/qemu_wait.py until the register dump settles into the linker's own
halt loop, reading main()'s return value back out of D0.

Usage: test.py BINFILE
"""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
binfile = Path(sys.argv[1]).resolve()
monitor = binfile.with_name("qemu.monitor")
monitor.unlink(missing_ok=True)

qemu = subprocess.Popen([
    "qemu-system-m68k", "-M", "none", "-cpu", "m68000", "-m", "16M",
    "-device", f"loader,file={binfile},addr=0x1000,cpu-num=0",
    "-device", "loader,addr=0x1000,cpu-num=0",
    "-monitor", f"unix:{monitor},server,nowait", "-display", "none", "-no-reboot",
])
try:
    result = subprocess.run(
        [sys.executable, str(ROOT / "tests" / "qemu_wait.py"), str(monitor), "40"],
        capture_output=True, text=True, timeout=45,
    )
finally:
    qemu.terminate()
    try:
        qemu.wait(timeout=5)
    except subprocess.TimeoutExpired:
        qemu.kill()

if result.returncode != 0:
    sys.exit(f"qemu_wait failed:\n{result.stdout}\n{result.stderr}")

match = re.search(r"D0 = ([0-9a-fA-F]+)", result.stdout)
assert match, result.stdout
d0 = int(match.group(1), 16)
if d0 >= 0x80000000:
    d0 -= 0x100000000
assert d0 == 45, d0
print("PASS (D0=45)")
