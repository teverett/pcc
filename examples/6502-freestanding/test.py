"""Verifies build/main.bin returns 45, stepping it in py65's 6502 MPU core
since there's no OS here to just run it on - same self-jump halt detection
and zero-page return-value convention (`ac` at addresses 2/3) as
tests/run_tests.sh's own check_6502(), since the linker's _start stub for
`--format=raw` is the same one M6502Codegen.cpp emits for the raw pipeline.
"""
import sys

from py65.devices.mpu6502 import MPU

with open(sys.argv[1], "rb") as f:
    data = f.read()

base = 0x0600
mpu = MPU()
mpu.memory[base:base + len(data)] = data
mpu.pc = base

for _ in range(2_000_000):
    pc_before = mpu.pc
    mpu.step()
    if mpu.pc == pc_before:  # "halt: jmp halt"
        break
else:
    raise SystemExit("simulator never reached the halt loop (step limit exceeded)")

lo = mpu.memory[2]
hi = mpu.memory[3]
val = lo | (hi << 8)
if val >= 0x8000:
    val -= 0x10000
assert val == 45, val
print("PASS (returned 45)")
