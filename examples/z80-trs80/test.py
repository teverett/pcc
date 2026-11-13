"""Verifies main.raw writes "HELLO, WORLD!" to TRS-80 video RAM, run on a
real, independent Z80 core (z80-python) rather than py65/6502. No ROM call
needs patching with a stub first: trs80.c only ever stores through a
pointer cast straight to video RAM's own absolute address (see
src/rt/trs80/trs80.h's own header comment), so the simulator's memory
array already is the screen - the printed text is read straight back out
of it.

Usage: test.py MAINRAW
"""
import sys

from z80_python import Z80CPU

with open(sys.argv[1], "rb") as f:
    data = f.read()

base = 0x8000
memory = bytearray(0x10000)
memory[base:base + len(data)] = data

cpu = Z80CPU(memory.__getitem__, memory.__setitem__)
cpu.pc = base
for _ in range(2_000_000):
    pc_before = cpu.pc
    cpu.step()
    if cpu.pc == pc_before:  # "halt: jp halt"
        break
else:
    raise SystemExit("simulator never reached the halt loop (step limit exceeded)")

row = bytes(memory[0x3C00 + 7 * 64:0x3C00 + 8 * 64])
assert row == b" " * 25 + b"HELLO, WORLD!" + b" " * 26, row
print("PASS")
