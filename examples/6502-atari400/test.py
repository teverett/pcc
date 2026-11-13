"""Verifies main.raw prints "HELLO, WORLD!" via py65's 6502 MPU core. It
makes a real CIOV call (asm { jsr ... } plus ldxi in src/rt/atari400/
atari400.c) that would otherwise JSR into empty memory. CIOV ($E456) is
patched with a tiny hand-written 6502 stub before running: it reads IOCB
0's own ICCOM cell ($0342) into Y (without disturbing A, which holds the
character for a PUTCHR call); if it's PUTCHR (11), it appends A verbatim
into a buffer at $0400, tracked via a zero-page position counter at $F0.

Usage: test.py MAINRAW
"""
import sys

from py65.devices.mpu6502 import MPU

with open(sys.argv[1], "rb") as f:
    data = f.read()

base = 0x2000
mpu = MPU()
mpu.memory[base:base + len(data)] = data

mpu.memory[0xE456:0xE456 + 16] = [
    0xAC, 0x42, 0x03,  # LDY $0342 (ICCOM)
    0xC0, 0x0B,        # CPY #11 (PUTCHR)
    0xD0, 0x08,        # BNE skip
    0xA6, 0xF0,        # LDX $F0
    0x9D, 0x00, 0x04,  # STA $0400,X
    0xE8,              # INX
    0x86, 0xF0,        # STX $F0
    0x60,              # RTS
]
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
# atari_clrscr() sends ATASCII 125 (clear screen) before the text, and
# atari_crout() sends ATASCII 155 (end-of-line, not ASCII CR/LF) after it.
assert out == bytes([125]) + b"HELLO, WORLD!" + bytes([155]), out
print("PASS")
