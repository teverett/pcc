#!/usr/bin/env bash
# Exercises src/rt/trs80/trs80.c's public functions for real under a real,
# independent Z80 core (`z80-python` - same package tests/run_tests.sh's own
# check_z80() uses). Unlike tests/run_apple2_rom_wrapper_tests.sh, no ROM
# entry point needs patching with a stub first: trs80.c never makes a
# ROM/DOS call at all, only plain loads/stores through a pointer cast from
# an absolute address (see trs80.h's own header comment), so the
# simulator's own memory array already is the screen and the keyboard.
#
# Usage: tests/run_trs80_rt_tests.sh [path/to/rcc]

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

run_z80() {
    # $1 = .flat path, writes final memory image (16384 bytes, as hex) to stdout
    python3 - "$1" <<'PY'
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
    sys.exit("simulator never reached the halt loop (step limit exceeded)")

sys.stdout.write(memory[0x3C00:0x4000].hex())
PY
}

# trs80_cls() fills the whole 64x16 (1024-byte) screen with spaces, and
# nothing else - every byte of video RAM should read back as 0x20.
cat >cls_main.c <<'EOF'
#include "trs80.h"
int main() {
    trs80_cls();
    return 0;
}
EOF
cp "$ROOT_DIR/src/rt/trs80/trs80.h" "$ROOT_DIR/src/rt/trs80/trs80.c" .
"$RCC" --target=z80 --format=coff cls_main.c >/dev/null
"$RCC" --target=z80 --format=coff trs80.c >/dev/null
"$RCC" --link --target=z80 --format=raw --out=cls.flat build/cls_main.o build/trs80.o

mem_hex="$(run_z80 "$PWD/cls.flat")" || { echo "FAIL trs80_cls: $mem_hex"; fail=1; }
expected_hex="$(python3 -c 'print(("20" * 1024))')"
if [ "$mem_hex" = "$expected_hex" ]; then
    echo "PASS trs80_cls_fills_screen_with_spaces"
else
    echo "FAIL trs80_cls_fills_screen_with_spaces: video RAM isn't all spaces"
    fail=1
fi

# trs80_puts(row, col, s) writes s's own bytes starting at row*64+col,
# left to right, leaving every other cell as trs80_cls() left it.
cat >puts_main.c <<'EOF'
#include "trs80.h"
int main() {
    trs80_cls();
    trs80_puts(2, 10, "HI!");
    return 0;
}
EOF
"$RCC" --target=z80 --format=coff puts_main.c >/dev/null
"$RCC" --link --target=z80 --format=raw --out=puts.flat build/puts_main.o build/trs80.o

mem_hex="$(run_z80 "$PWD/puts.flat")" || { echo "FAIL trs80_puts: $mem_hex"; fail=1; }
python3 - "$mem_hex" <<'PY' || fail=1
import sys
mem = bytes.fromhex(sys.argv[1])
start = 2 * 64 + 10
assert mem[start:start + 3] == b"HI!", f"expected b'HI!' at offset {start}, got {mem[start:start + 3]!r}"
rest = mem[:start] + mem[start + 3:]
assert rest == b" " * len(rest), "trs80_puts touched a cell outside its own 3-character span"
print("PASS trs80_puts_writes_string_at_row_col")
PY

# trs80_putchar(c)/trs80_print(s) track their own cursor, wrapping to column 0
# of the next row (never touching row 0 - a 64-character string stays on one
# row, exactly filling it) once the cursor would otherwise run past column 63.
cat >print_main.c <<'EOF'
#include "trs80.h"
int main() {
    trs80_cls();
    trs80_print("ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKL");
    trs80_putchar('!');
    return 0;
}
EOF
"$RCC" --target=z80 --format=coff print_main.c >/dev/null
"$RCC" --link --target=z80 --format=raw --out=print.flat build/print_main.o build/trs80.o

mem_hex="$(run_z80 "$PWD/print.flat")" || { echo "FAIL trs80_print: $mem_hex"; fail=1; }
python3 - "$mem_hex" <<'PY' || fail=1
import sys
mem = bytes.fromhex(sys.argv[1])
row0 = mem[0:64]
row1 = mem[64:128]
assert row0 == b"ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKL", row0
assert row1[0:1] == b"!", f"expected the 65th character to wrap to row 1 column 0, got {row1[0:1]!r}"
assert row1[1:] == b" " * 63, "trs80_putchar's wrap touched more of row 1 than just column 0"
print("PASS trs80_print_wraps_at_column_63")
PY

# trs80_scroll() shifts every row up by one (row 0's own contents are lost)
# and clears the newly-exposed bottom row, without moving any other row's
# own contents sideways.
cat >scroll_main.c <<'EOF'
#include "trs80.h"
int main() {
    trs80_cls();
    trs80_putc(0, 0, 'X');
    trs80_putc(1, 0, 'Y');
    trs80_putc(15, 63, 'Z');
    trs80_scroll();
    return 0;
}
EOF
"$RCC" --target=z80 --format=coff scroll_main.c >/dev/null
"$RCC" --link --target=z80 --format=raw --out=scroll.flat build/scroll_main.o build/trs80.o

mem_hex="$(run_z80 "$PWD/scroll.flat")" || { echo "FAIL trs80_scroll: $mem_hex"; fail=1; }
python3 - "$mem_hex" <<'PY' || fail=1
import sys
mem = bytes.fromhex(sys.argv[1])
assert mem[0:1] == b"Y", f"row 0 should now hold what was row 1 ('Y'), got {mem[0:1]!r}"
assert mem[64:65] == b" ", "row 1 should now hold what was row 2 (blank)"
assert mem[15 * 64:16 * 64] == b" " * 64, "the newly-exposed bottom row should be all spaces"
print("PASS trs80_scroll_shifts_rows_up_and_clears_bottom_row")
PY

# trs80_key_row(row) reads the keyboard matrix row at TRS80_KEYBOARD_BASE |
# (1 << row) with no decoding - whatever bits are set there come back
# unchanged.
cat >key_row_main.c <<'EOF'
#include "trs80.h"
int main() {
    return trs80_key_row(2);
}
EOF
"$RCC" --target=z80 --format=coff key_row_main.c >/dev/null
"$RCC" --link --target=z80 --format=raw --out=key_row.flat build/key_row_main.o build/trs80.o

python3 - "$PWD/key_row.flat" <<'PY' || fail=1
import sys
from z80_python import Z80CPU

with open(sys.argv[1], "rb") as f:
    data = f.read()

base = 0x8000
memory = bytearray(0x10000)
memory[base:base + len(data)] = data
memory[0x3800 | (1 << 2)] = 0x5A  # row 2's own address; an arbitrary column mask

cpu = Z80CPU(memory.__getitem__, memory.__setitem__)
cpu.pc = base
for _ in range(2_000_000):
    pc_before = cpu.pc
    cpu.step()
    if cpu.pc == pc_before:
        break
else:
    sys.exit("simulator never reached the halt loop (step limit exceeded)")

val = (cpu.h << 8) | cpu.l
assert val == 0x5A, f"expected trs80_key_row(2) == 0x5A, got {val:#x}"
print("PASS trs80_key_row_reads_matrix_address")
PY

# trs80_any_key_down() ORs all 8 rows together, so it's 0 when nothing is
# held down anywhere and 1 as soon as any one row has any bit set.
cat >any_key_main.c <<'EOF'
#include "trs80.h"
int main() {
    return trs80_any_key_down();
}
EOF
"$RCC" --target=z80 --format=coff any_key_main.c >/dev/null
"$RCC" --link --target=z80 --format=raw --out=any_key.flat build/any_key_main.o build/trs80.o

python3 - "$PWD/any_key.flat" <<'PY' || fail=1
import sys
from z80_python import Z80CPU

def run(poke_addr, poke_val):
    with open(sys.argv[1], "rb") as f:
        data = f.read()
    base = 0x8000
    memory = bytearray(0x10000)
    memory[base:base + len(data)] = data
    if poke_addr is not None:
        memory[poke_addr] = poke_val
    cpu = Z80CPU(memory.__getitem__, memory.__setitem__)
    cpu.pc = base
    for _ in range(2_000_000):
        pc_before = cpu.pc
        cpu.step()
        if cpu.pc == pc_before:
            return cpu.l
    sys.exit("simulator never reached the halt loop (step limit exceeded)")

assert run(None, 0) == 0, "expected trs80_any_key_down() == 0 with no keys held"
assert run(0x3800 | (1 << 5), 0x01) == 1, "expected trs80_any_key_down() == 1 with row 5 held"
print("PASS trs80_any_key_down_detects_any_row")
PY

# --format=cmd (CmdFormat.cpp) wraps the exact same linked program --format=raw
# produces, just split into TYPE/LENGTH/DATA records (see
# ../src/fileformat/cmd/CmdFormat.h's own header comment) - decoded here with
# a from-scratch Python reader, independent of CmdFormat.cpp's own, so this
# also catches a bug the two could otherwise share.
"$RCC" --link --target=z80 --format=cmd --out=puts.cmd build/puts_main.o build/trs80.o

python3 - "$PWD" <<'PY' || fail=1
import sys
d = sys.argv[1] + "/"
flat = open(d + "puts.flat", "rb").read()
cmd = open(d + "puts.cmd", "rb").read()

i = 0
chunks = []
transfer = None
while i < len(cmd):
    rec_type, raw_len = cmd[i], cmd[i + 1]
    i += 2
    if rec_type == 1:
        data_area = raw_len + 256 if raw_len < 3 else raw_len
        code_len = data_area - 2
        addr = cmd[i] | (cmd[i + 1] << 8)
        chunks.append((addr, cmd[i + 2:i + 2 + code_len]))
        i += 2 + code_len
    elif rec_type == 2:
        assert raw_len == 2
        transfer = cmd[i] | (cmd[i + 1] << 8)
        i += 2
    else:
        raise AssertionError(f"unexpected record type {rec_type}")

base = chunks[0][0]
assert transfer == base, f"expected transfer address {base}, got {transfer}"
payload = b"".join(bytes(c[1]) for c in chunks)
assert payload == flat, "cmd payload doesn't match the flat link"
print(f"PASS trs80_cmd_format_matches_raw_link (base={base} chunks={len(chunks)})")
PY

exit $fail
