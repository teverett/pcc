"""Verifies main.raw writes "HELLO, WORLD!" into the TMS9918 VDP's name
table, run on a real Z80 core (z80-python). Unlike z80-trs80's pure
memory-mapped hardware, coleco.c reaches the VDP/controllers/sound chip
entirely through IN/OUT (port-mapped, not memory-mapped - see
src/rt/coleco/coleco.h's own header comment), so this needs z80-python's
own read_port/write_port callbacks, not just a plain memory array. One
real Z80 hardware quirk this harness accounts for: `OUT (n),A` puts A on
the address bus's own upper byte too (z80-python's own _op_out_n_a()
models this faithfully - port address "A:n"), so every callback here
masks `port` down to 8 bits, exactly what a real ColecoVision's own
simple port decoder does (only A0-A7 select a device).

Usage: test.py MAINRAW
"""
import sys

from z80_python import Z80CPU

with open(sys.argv[1], "rb") as f:
    data = f.read()

base = 0x8000
memory = bytearray(0x10000)
memory[base:base + len(data)] = data

vram = bytearray(0x4000)
state = {"addr": 0, "latch": None}


def read_port(port):
    port &= 0xFF
    if port == 0xBE:
        val = vram[state["addr"] & 0x3FFF]
        state["addr"] = (state["addr"] + 1) & 0x3FFF
        return val
    return 0xFF


def write_port(port, value):
    port &= 0xFF
    if port == 0xBE:
        vram[state["addr"] & 0x3FFF] = value
        state["addr"] = (state["addr"] + 1) & 0x3FFF
    elif port == 0xBF:
        if state["latch"] is None:
            state["latch"] = value
        else:
            lo, hi = state["latch"], value
            state["latch"] = None
            if not (hi & 0x80):
                state["addr"] = (lo | ((hi & 0x3F) << 8)) & 0x3FFF


cpu = Z80CPU(memory.__getitem__, memory.__setitem__, read_port=read_port, write_port=write_port)
cpu.pc = base
for _ in range(2_000_000):
    pc_before = cpu.pc
    cpu.step()
    if cpu.pc == pc_before:  # "halt: jp halt"
        break
else:
    raise SystemExit("simulator never reached the halt loop (step limit exceeded)")

name_table = vram[0x1800:0x1800 + 32 * 24]
row = bytes(name_table[10 * 32:11 * 32])
assert row == b" " * 9 + b"HELLO, WORLD!" + b" " * 10, row
print("PASS")
