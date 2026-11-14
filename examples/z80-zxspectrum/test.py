"""Verifies main.raw sets the border, fills the screen attributes, and
plots an X shape into screen memory - all real, ROM-free ULA/memory-
mapped hardware - run on a real Z80 core (z80-python). `OUT (n),A` puts A
on the address bus's own upper byte too (z80-python's own
`_op_out_n_a()` models this faithfully - port address "A:n"), so the
border write is checked with the port masked to 8 bits, same convention
../z80-coleco/test.py uses.

Usage: test.py MAINRAW
"""
import sys

from z80_python import Z80CPU

with open(sys.argv[1], "rb") as f:
    data = f.read()

base = 0x8000
memory = bytearray(0x10000)
memory[base:base + len(data)] = data

writes = []


def write_port(port, value):
    writes.append((port & 0xFF, value))


cpu = Z80CPU(memory.__getitem__, memory.__setitem__, write_port=write_port)
cpu.pc = base
for _ in range(2_000_000):
    pc_before = cpu.pc
    cpu.step()
    if cpu.pc == pc_before:  # "halt: jp halt"
        break
else:
    sys.exit("simulator never reached the halt loop (step limit exceeded)")

assert writes == [(0xFE, 5)], f"expected exactly one border write (0xFE, 5), got {writes!r}"

attrs = memory[0x5800:0x5800 + 32 * 24]
expected_attr = 0 | (5 << 3) | 64  # ink 0, paper 5, bright 1, flash 0
assert attrs == bytes([expected_attr]) * len(attrs), "zx_fill_attrs() didn't fill every attribute cell"

bitmap = memory[0x4000:0x4000 + 6144]


def addr_bit(x, y):
    addr = ((y & 192) << 5) + ((y & 7) << 8) + ((y & 56) << 2) + (x >> 3)
    return addr, 128 >> (x & 7)


for x in range(32):
    for y in (96 - x, 96 + x):
        off, bit = addr_bit(100 + x, y)
        assert bitmap[off] & bit, f"expected pixel ({100 + x},{y}) set"

print("PASS")
