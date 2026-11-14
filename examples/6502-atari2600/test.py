"""Verifies main.raw actually sets TIA's COLUBK register ($09) to the
expected color before falling into its own halt loop, run in py65's bare
6502 MPU core - no TIA hardware is simulated, so $09 is read back as
plain RAM, not a real video register; this only confirms the compiler's
own codegen/linker correctness, not what the color looks like on real/
emulated hardware.

Usage: test.py MAINRAW
"""
import sys

from py65.devices.mpu6502 import MPU

with open(sys.argv[1], "rb") as f:
    data = f.read()

base = 0xF000
mpu = MPU()
mpu.memory[base:base + len(data)] = data
mpu.pc = base

for _ in range(200000):
    pc_before = mpu.pc
    mpu.step()
    if mpu.pc == pc_before:
        break
else:
    raise SystemExit("simulator never reached the halt loop (step limit exceeded)")

colubk = mpu.memory[0x09]
assert colubk == 0x1E, f"expected COLUBK ($09) to be $1E, got ${colubk:02X}"
print("PASS")
