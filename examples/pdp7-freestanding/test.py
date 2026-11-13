"""Verifies main.rim returns 45, run under a real `open-simh` PDP-7
simulator the same way tests/run_tests.sh's own check_pdp7() does: the
RIM-format tape is deposited at octal 10000 (the linker's own load
address) with the simulator's own `load <file> <addr>` command, `go` runs
it to its `hlt`, and main()'s return value is read back out of AC.

Usage: test.py RIMFILE
"""
import re
import subprocess
import sys
import tempfile
from pathlib import Path

rimfile = Path(sys.argv[1]).resolve()

with tempfile.NamedTemporaryFile("w", suffix=".simh", delete=False) as script:
    script.write(f"load {rimfile} 10000\ngo 10000\ne ac\n")
    simfile = script.name

try:
    result = subprocess.run(
        ["pdp7", simfile], stdin=subprocess.DEVNULL, capture_output=True, text=True, timeout=10,
    )
finally:
    Path(simfile).unlink(missing_ok=True)

match = re.search(r"^AC:\s*(\S+)", result.stdout, re.MULTILINE)
if not match:
    sys.exit(f"simulator produced no AC (timed out or crashed):\n{result.stdout}\n{result.stderr}")

ac = int(match.group(1), 8)
if ac >= 0o400000:
    ac -= 0o1000000
assert ac == 45, ac
print("PASS (AC=45)")
