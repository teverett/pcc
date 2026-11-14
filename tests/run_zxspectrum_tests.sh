#!/usr/bin/env bash
# Exercises every function in src/rt/zxspectrum/zxspectrum.c for real under a real, independent
# Z80 core (`z80-python`, same package tests/run_trs80_rt_tests.sh/run_coleco_rt_tests.sh use),
# and checks --format=sna (src/fileformat/sna/SnaFormat.h) produces a correctly-sized, correctly-
# bootstrapped 48K snapshot from the exact same linked program.
#
# zx_set_border()/zx_set_speaker() reach the ULA entirely through `OUT` (port $FE), and
# zx_key_row() reads it back through `IN` (port is A:$FE - see zxspectrum.h's own header
# comment), so those tests supply z80-python's own write_port/read_port callbacks instead of just
# inspecting a plain memory array - same convention run_coleco_rt_tests.sh uses, including its
# own 8-bit port-number masking (`OUT (n),A`/`IN A,(n)` both put a second byte on the address
# bus - see that script's own header comment). zx_set_attr()/zx_fill_attrs()/zx_set_pixel()/
# zx_cls_bitmap() are ordinary memory-mapped stores, so those tests just inspect the simulator's
# own memory array afterward, like run_trs80_rt_tests.sh does.
#
# Usage: tests/run_zxspectrum_tests.sh [path/to/rcc]

set -eu

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
RCC="$ROOT_DIR/build/bin/rcc"
for arg in "$@"; do
    case "$arg" in
        *) RCC="$arg" ;;
    esac
done
# Resolve to an absolute path before the later `cd "$tmpdir"` below - see
# docs/known-bugs.md's "make test's Apple II/TRS-80/ColecoVision runtime
# test scripts break when $(TARGET) is a relative path" entry, which this
# script avoids from the start.
RCC="$(cd "$(dirname "$RCC")" && pwd)/$(basename "$RCC")"

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

cp "$ROOT_DIR/src/rt/zxspectrum/zxspectrum.h" "$ROOT_DIR/src/rt/zxspectrum/zxspectrum.c" .
"$RCC" --target=z80 --format=coff zxspectrum.c >/dev/null

# Shared harness: runs a .flat image on a real Z80 core, with configurable
# read_port (defaulting to 0xFF, "nothing pressed", for the keyboard test)
# and a record of every port write. Prints
# "MEM=<hex>|HL=<int>|WRITES=<port>:<value>,...".
run_z80() {
    # $1 = .flat path; $2 (optional) = "port:value" the read_port callback should answer with
    python3 - "$1" "${2:-}" <<'PY'
import sys
from z80_python import Z80CPU

with open(sys.argv[1], "rb") as f:
    data = f.read()
read_spec = sys.argv[2]

base = 0x8000
memory = bytearray(0x10000)
memory[base:base + len(data)] = data

writes = []


def write_port(port, value):
    writes.append((port & 0xFF, value))


def read_port(port):
    if read_spec:
        want_port, want_value = read_spec.split(":")
        if port == int(want_port):
            return int(want_value)
    return 0xFF


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
sys.stdout.write(memory.hex() + "|" + str(hl) + "|" + ",".join(f"{p}:{v}" for p, v in writes))
PY
}

# zx_set_border()/zx_set_speaker() share port $FE's own shadow byte, so
# setting one never clobbers the other's last-written bits: border(5),
# speaker(on), border(3) should write 5, then 5|16=21, then 16|3=19 - the
# third write proves the speaker bit survived the second border() call.
cat >border_main.c <<'EOF'
#include "zxspectrum.h"
int main() {
    zx_set_border(5);
    zx_set_speaker(1);
    zx_set_border(3);
    return 0;
}
EOF
"$RCC" --target=z80 --format=coff border_main.c >/dev/null
"$RCC" --link --target=z80 --format=raw --out=border.raw build/border_main.o build/zxspectrum.o
result="$(run_z80 "$PWD/border.raw")" || { echo "FAIL zx_set_border/zx_set_speaker: $result" >&2; fail=1; }
IFS='|' read -r _ _ writes <<<"$result"
python3 - "$writes" <<'PY' || fail=1
import sys
writes = sys.argv[1]
assert writes == "254:5,254:21,254:19", f"expected 254:5,254:21,254:19, got {writes!r}"
print("PASS zx_set_border_and_zx_set_speaker_share_ula_shadow")
PY

# zx_key_row(3) selects half-row 3 ($F7) on the upper address byte and
# reads port $FE - z80-python's own IN A,(n) models the real A:n port
# address, so supplying read_port only for port (0xF7<<8)|0xFE confirms
# the half-row select byte actually reached the bus.
cat >key_main.c <<'EOF'
#include "zxspectrum.h"
int main() {
    return zx_key_row(3);
}
EOF
"$RCC" --target=z80 --format=coff key_main.c >/dev/null
"$RCC" --link --target=z80 --format=raw --out=key.raw build/key_main.o build/zxspectrum.o
want_port=$(( (0xF7 << 8) | 0xFE ))
result="$(run_z80 "$PWD/key.raw" "$want_port:29")" || { echo "FAIL zx_key_row: $result" >&2; fail=1; }
IFS='|' read -r _ hl _ <<<"$result"
python3 - "$hl" <<'PY' || fail=1
import sys
hl = int(sys.argv[1])
assert hl == 29, f"expected zx_key_row(3) == 29 (the row-3-only read_port answer), got {hl}"
print("PASS zx_key_row_selects_half_row_on_upper_address_byte")
PY

# zx_attr() packs INK/PAPER/BRIGHT/FLASH into the real bit layout
# FLASH(1) BRIGHT(1) PAPER(3) INK(3): ink=1, paper=2, bright=1, flash=1 ->
# 1 | (2<<3) | 64 | 128 = 209.
cat >attr_fn_main.c <<'EOF'
#include "zxspectrum.h"
int main() {
    return zx_attr(1, 2, 1, 1);
}
EOF
"$RCC" --target=z80 --format=coff attr_fn_main.c >/dev/null
"$RCC" --link --target=z80 --format=raw --out=attr_fn.raw build/attr_fn_main.o build/zxspectrum.o
result="$(run_z80 "$PWD/attr_fn.raw")" || { echo "FAIL zx_attr: $result" >&2; fail=1; }
IFS='|' read -r _ hl _ <<<"$result"
python3 - "$hl" <<'PY' || fail=1
import sys
hl = int(sys.argv[1])
assert hl == 209, f"expected zx_attr(1,2,1,1) == 209, got {hl}"
print("PASS zx_attr_packs_ink_paper_bright_flash")
PY

# zx_fill_attrs()/zx_set_attr() are plain memory stores into $5800-$5AFF -
# fill every one of the 768 cells with 0x38, then overwrite exactly one
# (row 2, col 5) with 0x47, leaving every other cell at 0x38.
cat >attr_main.c <<'EOF'
#include "zxspectrum.h"
int main() {
    zx_fill_attrs(0x38);
    zx_set_attr(2, 5, 0x47);
    return 0;
}
EOF
"$RCC" --target=z80 --format=coff attr_main.c >/dev/null
"$RCC" --link --target=z80 --format=raw --out=attr.raw build/attr_main.o build/zxspectrum.o
result="$(run_z80 "$PWD/attr.raw")" || { echo "FAIL zx_fill_attrs/zx_set_attr: $result" >&2; fail=1; }
IFS='|' read -r mem_hex _ _ <<<"$result"
python3 - "$mem_hex" <<'PY' || fail=1
import sys
mem = bytes.fromhex(sys.argv[1])
attrs = mem[0x5800:0x5800 + 32 * 24]
pos = 2 * 32 + 5
assert attrs[pos] == 0x47, f"expected attrs[{pos}] == 0x47, got {attrs[pos]:#x}"
rest = attrs[:pos] + attrs[pos + 1:]
assert rest == bytes([0x38]) * len(rest), "zx_fill_attrs/zx_set_attr touched a cell unexpectedly"
print("PASS zx_fill_attrs_and_zx_set_attr_write_attribute_memory")
PY

# zx_cls_bitmap()/zx_set_pixel() are plain memory stores into $4000-$57FF,
# using the real non-linear screen address formula - set two pixels, clear
# one of them, and confirm only the one surviving bit is set anywhere in
# the 6144-byte bitmap.
cat >pixel_main.c <<'EOF'
#include "zxspectrum.h"
int main() {
    zx_cls_bitmap();
    zx_set_pixel(10, 20, 1);
    zx_set_pixel(11, 20, 1);
    zx_set_pixel(10, 20, 0);
    return 0;
}
EOF
"$RCC" --target=z80 --format=coff pixel_main.c >/dev/null
"$RCC" --link --target=z80 --format=raw --out=pixel.raw build/pixel_main.o build/zxspectrum.o
result="$(run_z80 "$PWD/pixel.raw")" || { echo "FAIL zx_cls_bitmap/zx_set_pixel: $result" >&2; fail=1; }
IFS='|' read -r mem_hex _ _ <<<"$result"
python3 - "$mem_hex" <<'PY' || fail=1
import sys
mem = bytes.fromhex(sys.argv[1])
bitmap = mem[0x4000:0x4000 + 6144]


def addr_bit(x, y):
    addr = ((y & 192) << 5) + ((y & 7) << 8) + ((y & 56) << 2) + (x >> 3)
    return addr, 128 >> (x & 7)


off, bit = addr_bit(11, 20)
assert bitmap[off] & bit, f"expected pixel (11,20) set at offset {off:#x} bit {bit:#x}"
off10, bit10 = addr_bit(10, 20)
assert not (bitmap[off10] & bit10), "pixel (10,20) should have been cleared again"

total_set_bits = sum(bin(b).count("1") for b in bitmap)
assert total_set_bits == 1, f"expected exactly one set bit in the whole bitmap, got {total_set_bits}"
print("PASS zx_cls_bitmap_and_zx_set_pixel_write_screen_memory")
PY

# --format=sna (SnaFormat.cpp) wraps the exact same linked program
# --format=raw produces, via the bootstrap-trick convention that format's
# own header comment describes - see sna_format_test.cpp for the format's
# own unit tests; this only checks a real linked program round-trips the
# same way, at the plain z80 target's own default base ($8000).
"$RCC" --link --target=z80 --format=sna --out=main.sna build/border_main.o build/zxspectrum.o
python3 - "$PWD" <<'PY' || fail=1
import sys
d = sys.argv[1] + "/"
flat = open(d + "border.raw", "rb").read()
sna = open(d + "main.sna", "rb").read()
assert len(sna) == 27 + 49152, f"expected a 49179-byte snapshot, got {len(sna)} bytes"

sp = sna[23] | (sna[24] << 8)
assert sp == 0xFFFE, f"header SP should point at the reserved bootstrap cell, got ${sp:04X}"

ram = sna[27:]
bootstrap_off = 0xFFFE - 0x4000
entry = ram[bootstrap_off] | (ram[bootstrap_off + 1] << 8)
assert entry == 0x8000, f"bootstrap cell should hold the program's own base ($8000), got ${entry:04X}"

program_off = 0x8000 - 0x4000
assert ram[program_off:program_off + len(flat)] == flat, \
    "the snapshot's own program bytes don't match the raw link"
print("PASS zxspectrum_sna_format_matches_raw_link")
PY

exit $fail
