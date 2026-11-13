"""Verifies main.raw prints "HELLO, WORLD!" via py65's 6502 MPU core. It
makes real COUT/CROUT/HOME/SETTXT/VTAB/SETINV/SETNORM calls (asm { jsr ... }
in src/rt/apple2/apple2.c) that would otherwise JSR into empty memory. Each
is patched with a tiny hand-written 6502 stub before running: COUT ($FDED)
appends A (masked back to 7 bits - COUT itself expects bit 7 set for normal
display text, see apple2.c's own a2_cout() comment) into a buffer at
$0400, tracked via a zero-page position counter at $F0 (ldx $F0; sta
$0400,x; inx; stx $F0; rts); CROUT ($FD8E) pushes a newline then falls
into that same COUT stub; HOME/SETTXT/VTAB/SETINV/SETNORM are all no-ops
(rts) - their effects aren't observable from outside the (nonexistent)
text screen, so this only confirms each call itself doesn't crash.

Usage: test.py MAINRAW
"""
import sys

from py65.devices.mpu6502 import MPU

with open(sys.argv[1], "rb") as f:
    data = f.read()

base = 0x2000
mpu = MPU()
mpu.memory[base:base + len(data)] = data

mpu.memory[0xFDED:0xFDED + 9] = [0xA6, 0xF0, 0x9D, 0x00, 0x04, 0xE8, 0x86, 0xF0, 0x60]
mpu.memory[0xF0] = 0
mpu.memory[0xFD8E:0xFD8E + 5] = [0xA9, 0x8A, 0x4C, 0xED, 0xFD]
mpu.memory[0xFC58] = 0x60  # HOME
mpu.memory[0xFB39] = 0x60  # SETTXT
mpu.memory[0xFC22] = 0x60  # VTAB
mpu.memory[0xFE80] = 0x60  # SETINV
mpu.memory[0xFE84] = 0x60  # SETNORM

mpu.pc = base
for _ in range(2_000_000):
    pc_before = mpu.pc
    mpu.step()
    if mpu.pc == pc_before:
        break
else:
    raise SystemExit("simulator never reached the halt loop (step limit exceeded)")

n = mpu.memory[0xF0]
out = bytes(mpu.memory[0x0400 + i] & 0x7F for i in range(n))
assert out == b"HELLO, WORLD!\n", out
print("PASS")
