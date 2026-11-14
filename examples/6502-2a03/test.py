"""Verifies main.raw actually sums 1..10 in an ordinary C loop and pokes
the result (55) into $0050, run in py65's bare 6502 MPU core - no PPU/
APU/mapper hardware is simulated, so this only confirms the compiler's
own codegen/linker correctness, not real NES hardware behavior.

Usage: test.py MAINRAW
"""
import sys

from py65.devices.mpu6502 import MPU

with open(sys.argv[1], "rb") as f:
    data = f.read()

base = 0x8000
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

result = mpu.memory[0x50]
assert result == 55, f"expected 55 (1+2+...+10), got {result}"
print("PASS")
