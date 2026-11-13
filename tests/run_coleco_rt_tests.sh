#!/usr/bin/env bash
# Exercises src/rt/coleco/coleco.c's public functions for real under a real,
# independent Z80 core (`z80-python`, same package tests/run_trs80_rt_tests.sh
# uses). Unlike that script, coleco.c reaches hardware entirely through
# `IN`/`OUT` (the VDP/controllers/sound chip are port-mapped, not memory-
# mapped - see coleco.h's own header comment), so each test here supplies
# z80-python's own `read_port`/`write_port` callbacks instead of just
# inspecting a plain memory array.
#
# One real Z80 hardware quirk this harness has to account for: `OUT (n),A`
# puts A on the address bus's own upper byte too (z80-python's own
# `_op_out_n_a()` models this faithfully, port address "A:n"), so every
# `write_port`/`read_port` callback here masks its own `port` argument down
# to 8 bits - exactly what a real ColecoVision's own simple port decoder
# does (only A0-A7 select a device; A8-A15 are never looked at), and what
# the next OUT's own data byte would otherwise corrupt the port number
# with.
#
# Usage: tests/run_coleco_rt_tests.sh [path/to/rcc]

set -eu

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
RCC="$ROOT_DIR/build/bin/rcc"
for arg in "$@"; do
    case "$arg" in
        *) RCC="$arg" ;;
    esac
done

if [ ! -x "$RCC" ]; then
    echo "rcc binary not found at $RCC - build it first ('make')" >&2
    exit 1
fi
if ! python3 -c "import z80_python" >/dev/null 2>&1; then
    echo "z80-python not found - install it (e.g. 'pip install z80-python')" >&2
    exit 1
fi

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT
cd "$tmpdir"

fail=0

# Common synthetic-hardware harness: a 16KB VRAM array behind the VDP's own
# two-byte $BF address/register latch, plus raw records of every port
# write/read so a test can check the controller/sound primitives directly
# without needing a model of those chips at all.
run_z80() {
    # $1 = .flat path; prints "VRAM=<hex>|WRITES=<port>:<value>,...|READS=<port>,..."
    python3 - "$1" "${2:-}" "${3:-}" <<'PY'
import sys
from z80_python import Z80CPU

with open(sys.argv[1], "rb") as f:
    data = f.read()
ctrl1_value = int(sys.argv[2]) if len(sys.argv) > 2 and sys.argv[2] else 0
ctrl2_value = int(sys.argv[3]) if len(sys.argv) > 3 and sys.argv[3] else 0

base = 0x8000
memory = bytearray(0x10000)
memory[base:base + len(data)] = data

vram = bytearray(0x4000)
state = {"addr": 0, "latch": None}
writes = []
reads = []


def read_port(port):
    port &= 0xFF
    reads.append(port)
    if port == 0xBE:
        val = vram[state["addr"] & 0x3FFF]
        state["addr"] = (state["addr"] + 1) & 0x3FFF
        return val
    if port == 0xFC:
        return ctrl1_value
    if port == 0xFF:
        return ctrl2_value
    return 0xFF


def write_port(port, value):
    port &= 0xFF
    writes.append((port, value))
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
    sys.exit("simulator never reached the halt loop (step limit exceeded)")

hl = (cpu.h << 8) | cpu.l
sys.stdout.write(vram.hex() + "|" + str(hl) + "|" + ",".join(f"{p}:{v}" for p, v in writes))
PY
}

cp "$ROOT_DIR/src/rt/coleco/coleco.h" "$ROOT_DIR/src/rt/coleco/coleco.c" .
"$RCC" --target=z80-coleco --format=coff coleco.c >/dev/null

# coleco_cls() fills the whole 32x24 (768-byte) name table at $1800 with
# spaces, and nothing else - every one of those bytes should read back as
# 0x20, with nothing written to any other VRAM address.
cat >cls_main.c <<'EOF'
#include "coleco.h"
int main() {
    coleco_cls();
    return 0;
}
EOF
"$RCC" --target=z80-coleco --format=coff cls_main.c >/dev/null
"$RCC" --link --target=z80-coleco --format=raw --out=cls.flat build/cls_main.o build/coleco.o

result="$(run_z80 "$PWD/cls.flat")" || { echo "FAIL coleco_cls: $result"; fail=1; }
IFS='|' read -r vram_hex _ _ <<<"$result"
python3 - "$vram_hex" <<'PY' || fail=1
import sys
vram = bytes.fromhex(sys.argv[1])
name_table = vram[0x1800:0x1800 + 32 * 24]
assert name_table == b" " * (32 * 24), "name table isn't all spaces after coleco_cls()"
assert vram[0:0x1800] == b"\x00" * 0x1800, "coleco_cls() touched VRAM before the name table"
assert vram[0x1800 + 32 * 24:] == b"\x00" * (len(vram) - 0x1800 - 32 * 24), \
    "coleco_cls() touched VRAM past the name table"
print("PASS coleco_cls_fills_name_table_with_spaces")
PY

# coleco_puts(row, col, s) writes s's own bytes starting at row*32+col, left
# to right, via the VDP's own auto-incrementing address - leaving every
# other name-table cell untouched.
cat >puts_main.c <<'EOF'
#include "coleco.h"
int main() {
    coleco_cls();
    coleco_puts(2, 10, "HI!");
    return 0;
}
EOF
"$RCC" --target=z80-coleco --format=coff puts_main.c >/dev/null
"$RCC" --link --target=z80-coleco --format=raw --out=puts.flat build/puts_main.o build/coleco.o

result="$(run_z80 "$PWD/puts.flat")" || { echo "FAIL coleco_puts: $result"; fail=1; }
IFS='|' read -r vram_hex _ _ <<<"$result"
python3 - "$vram_hex" <<'PY' || fail=1
import sys
vram = bytes.fromhex(sys.argv[1])
name_table = vram[0x1800:0x1800 + 32 * 24]
start = 2 * 32 + 10
assert name_table[start:start + 3] == b"HI!", f"expected b'HI!' at offset {start}, got {name_table[start:start + 3]!r}"
rest = name_table[:start] + name_table[start + 3:]
assert rest == b" " * len(rest), "coleco_puts touched a name-table cell outside its own 3-character span"
print("PASS coleco_puts_writes_string_at_row_col")
PY

# coleco_putc(row, col, c) writes exactly one cell.
cat >putc_main.c <<'EOF'
#include "coleco.h"
int main() {
    coleco_cls();
    coleco_putc(5, 7, 'X');
    return 0;
}
EOF
"$RCC" --target=z80-coleco --format=coff putc_main.c >/dev/null
"$RCC" --link --target=z80-coleco --format=raw --out=putc.flat build/putc_main.o build/coleco.o

result="$(run_z80 "$PWD/putc.flat")" || { echo "FAIL coleco_putc: $result"; fail=1; }
IFS='|' read -r vram_hex _ _ <<<"$result"
python3 - "$vram_hex" <<'PY' || fail=1
import sys
vram = bytes.fromhex(sys.argv[1])
name_table = vram[0x1800:0x1800 + 32 * 24]
pos = 5 * 32 + 7
assert name_table[pos:pos + 1] == b"X", f"expected b'X' at offset {pos}, got {name_table[pos:pos + 1]!r}"
rest = name_table[:pos] + name_table[pos + 1:]
assert rest == b" " * len(rest), "coleco_putc touched more than its own one cell"
print("PASS coleco_putc_writes_one_cell")
PY

# coleco_controller_read(player) selects joystick mode (a write to $C0) then
# reads $FC for player 0 / $FF for player 1 - whatever byte comes back, with
# no decoding, like src/rt/trs80/trs80.c's own trs80_key_row().
cat >ctrl0_main.c <<'EOF'
#include "coleco.h"
int main() {
    return coleco_controller_read(0);
}
EOF
"$RCC" --target=z80-coleco --format=coff ctrl0_main.c >/dev/null
"$RCC" --link --target=z80-coleco --format=raw --out=ctrl0.flat build/ctrl0_main.o build/coleco.o

result="$(run_z80 "$PWD/ctrl0.flat" 90)" || { echo "FAIL coleco_controller_read(0): $result"; fail=1; }
IFS='|' read -r _ hl writes <<<"$result"
python3 - "$hl" "$writes" <<'PY' || fail=1
import sys
hl = int(sys.argv[1])
writes = sys.argv[2]
assert hl == 90, f"expected coleco_controller_read(0) == 90, got {hl}"
assert any(w == "192:0" for w in writes.split(",")), "expected a write selecting joystick mode ($C0) first"
print("PASS coleco_controller_read_player0")
PY

result="$(run_z80 "$PWD/ctrl0.flat" 0 77)" || { echo "FAIL coleco_controller_read(1): $result"; fail=1; }

cat >ctrl1_main.c <<'EOF'
#include "coleco.h"
int main() {
    return coleco_controller_read(1);
}
EOF
"$RCC" --target=z80-coleco --format=coff ctrl1_main.c >/dev/null
"$RCC" --link --target=z80-coleco --format=raw --out=ctrl1.flat build/ctrl1_main.o build/coleco.o
result="$(run_z80 "$PWD/ctrl1.flat" 0 77)" || { echo "FAIL coleco_controller_read(1): $result"; fail=1; }
IFS='|' read -r _ hl _ <<<"$result"
python3 - "$hl" <<'PY' || fail=1
import sys
hl = int(sys.argv[1])
assert hl == 77, f"expected coleco_controller_read(1) == 77, got {hl}"
print("PASS coleco_controller_read_player1")
PY

# coleco_out_sound(value) issues exactly one OUT to $FF (the SN76489's own
# write port) with that value, and does nothing else observable.
cat >sound_main.c <<'EOF'
#include "coleco.h"
int main() {
    coleco_out_sound(158);
    return 0;
}
EOF
"$RCC" --target=z80-coleco --format=coff sound_main.c >/dev/null
"$RCC" --link --target=z80-coleco --format=raw --out=sound.flat build/sound_main.o build/coleco.o
result="$(run_z80 "$PWD/sound.flat")" || { echo "FAIL coleco_out_sound: $result"; fail=1; }
IFS='|' read -r _ _ writes <<<"$result"
python3 - "$writes" <<'PY' || fail=1
import sys
writes = sys.argv[1]
assert writes == "255:158", f"expected exactly one write (255:158), got {writes!r}"
print("PASS coleco_out_sound_writes_one_byte")
PY

# --format=colecorom (ColecoRomFormat.cpp) wraps the exact same linked
# program --format=raw produces, just padded with 0xFF up to the nearest
# standard cartridge size - see colecorom_format_test.cpp for the format's
# own unit tests; this only checks a real linked program round-trips the
# same way.
"$RCC" --link --target=z80-coleco --format=colecorom --out=puts.col build/puts_main.o build/coleco.o
python3 - "$PWD" <<'PY' || fail=1
import sys
d = sys.argv[1] + "/"
flat = open(d + "puts.flat", "rb").read()
col = open(d + "puts.col", "rb").read()
assert col[:2] in (b"\x55\xaa", b"\xaa\x55"), "colecorom image is missing its cartridge signature"
assert len(col) in (8192, 16384, 24576, 32768), f"colecorom image size {len(col)} isn't a standard cartridge size"
assert col[:len(flat)] == flat, "colecorom's own bytes don't match the raw link"
assert col[len(flat):] == b"\xff" * (len(col) - len(flat)), "colecorom's padding isn't all 0xFF"
print("PASS coleco_colecorom_format_matches_raw_link")
PY

exit $fail
