"""Verifies main.raw sets the screen mode/pens/border through the Gate Array (real `OUT`s to
port $7F) and plots an X shape into screen memory - all real, ROM-free Amstrad CPC hardware -
run on a real Z80 core (z80-python). `OUT (n),A` puts A on the address bus's own upper byte too
(z80-python's own `_op_out_n_a()` models this faithfully - port address "A:n"), so port writes
are checked with the port masked to 8 bits, same convention ../z80-coleco/test.py uses.

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

expected_writes = [
    (0x7F, 130),  # mode 2 (128 | 2)
    (0x7F, 0),    # select pen 0
    (0x7F, 64),   # pen 0's color: black (0)
    (0x7F, 1),    # select pen 1
    (0x7F, 64 | 26),  # pen 1's color: bright white (26)
    (0x7F, 16),   # select the border
    (0x7F, 64 | 1),  # border's color: blue (1)
]
assert writes == expected_writes, f"expected {expected_writes}, got {writes!r}"

screen = memory[0xC000:0xC000 + 15872]  # cpc_cls()'s own safe region - see cpc.h


def addr_bit(x, y):
    char_row = y >> 3
    line = y & 7
    addr = char_row * 80 + (line << 11) + (x >> 3)
    return addr, 128 >> (x & 7)


for x in range(32):
    for y in (76 - x, 76 + x):
        off, bit = addr_bit(100 + x, y)
        assert screen[off] & bit, f"expected pixel ({100 + x},{y}) set"

print("PASS")
