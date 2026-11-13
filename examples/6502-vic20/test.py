"""Verifies main.raw prints "HELLO, WORLD!" via py65's 6502 MPU core. It
makes real CHROUT calls (asm { jsr ... } in src/rt/vic20/vic20.c) that
would otherwise JSR into empty memory. CHROUT ($FFD2) is patched with a
tiny hand-written 6502 stub before running: it appends A verbatim (no
high-bit masking needed - unlike Apple ][ COUT, see vic20.c's own
v20_chrout() comment) into a buffer at $0400, tracked via a zero-page
position counter at $F0 (ldx $F0; sta $0400,x; inx; stx $F0; rts).

Usage: test.py MAINRAW
"""
import sys

from py65.devices.mpu6502 import MPU

with open(sys.argv[1], "rb") as f:
    data = f.read()

base = 0x1001
mpu = MPU()
mpu.memory[base:base + len(data)] = data

mpu.memory[0xFFD2:0xFFD2 + 9] = [0xA6, 0xF0, 0x9D, 0x00, 0x04, 0xE8, 0x86, 0xF0, 0x60]
mpu.memory[0xF0] = 0

mpu.pc = base
for _ in range(2_000_000):
    pc_before = mpu.pc
    mpu.step()
    if mpu.pc == pc_before:
        break
else:
    raise SystemExit("simulator never reached the halt loop (step limit exceeded)")

n = mpu.memory[0xF0]
out = bytes(mpu.memory[0x0400 + i] for i in range(n))
# v20_clrscr() sends PETSCII 147 (clear screen) before the text, and
# v20_crout() sends PETSCII 13 (carriage return) after it.
assert out == bytes([147]) + b"HELLO, WORLD!" + bytes([13]), out
print("PASS")
