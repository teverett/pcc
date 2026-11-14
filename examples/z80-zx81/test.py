"""Verifies main.raw issues the documented port sequence - OUT $FE (display on), IN A,(n) with
the half-row-5 select byte on the upper address bus, OUT $FD (display off) - and returns the
value that read_port supplied for that row, run on a real Z80 core (z80-python). This only
checks that the code does what src/rt/zx81/zx81.h documents, not that the documented sequence
itself matches real ZX81 hardware - see that file's own header comment for why.

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
want_port = (0xDF << 8) | 0xFE


def write_port(port, value):
    writes.append((port & 0xFF, value))


def read_port(port):
    return 17 if port == want_port else 0xFF


cpu = Z80CPU(memory.__getitem__, memory.__setitem__, read_port=read_port, write_port=write_port)
cpu.pc = base
for _ in range(2_000_000):
    pc_before = cpu.pc
    cpu.step()
    if cpu.pc == pc_before:  # "halt: jp halt"
        break
else:
    sys.exit("simulator never reached the halt loop (step limit exceeded)")

assert writes == [(0xFE, 0), (0xFD, 0)], f"expected display-on then display-off, got {writes!r}"
result = (cpu.h << 8) | cpu.l
assert result == 17, f"expected zx81_key_row(5) == 17, got {result}"
print("PASS")
