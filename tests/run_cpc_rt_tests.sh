#!/usr/bin/env bash
# Exercises every function in src/rt/cpc/cpc.c for real under a real, independent Z80 core
# (`z80-python`, same package tests/run_trs80_rt_tests.sh/run_coleco_rt_tests.sh/
# run_zxspectrum_tests.sh use), and checks --format=amsdos (src/fileformat/amsdos/
# AmsdosFormat.h) produces a correctly-headered, correctly-checksummed file from the exact same
# linked program.
#
# cpc_set_pen()/cpc_set_mode()/cpc_psg_write() reach the Gate Array/PPI entirely through `OUT`
# (ports $7F/$F4/$F6), so those tests supply z80-python's own write_port callback instead of
# inspecting plain memory, same convention run_coleco_rt_tests.sh uses (including its own 8-bit
# port-number masking - see that script's own header comment for why `OUT (n),A` needs it).
# cpc_cls()/cpc_set_pixel() are ordinary memory-mapped stores, so those tests just inspect the
# simulator's own memory array afterward.
#
# Usage: tests/run_cpc_rt_tests.sh [path/to/rcc]

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

cp "$ROOT_DIR/src/rt/cpc/cpc.h" "$ROOT_DIR/src/rt/cpc/cpc.c" .
"$RCC" --target=z80 --format=coff cpc.c >/dev/null

# Shared harness: runs a .flat image (linked at $8000, plain z80's own default base) on a real
# Z80 core, recording every port write. Prints "MEM=<hex>|WRITES=<port>:<value>,...".
run_z80() {
    python3 - "$1" <<'PY'
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
for _ in range(20_000_000):
    pc_before = cpu.pc
    cpu.step()
    if cpu.pc == pc_before:  # "halt: jp halt"
        break
else:
    sys.exit("simulator never reached the halt loop (step limit exceeded)")

sys.stdout.write(memory.hex() + "|" + ",".join(f"{p}:{v}" for p, v in writes))
PY
}

# cpc_set_pen(3, 11) selects pen 3 (bit 4 clear, bits 3-0 = 3) then its
# color 11 (bits 7-6 = 01, bits 5-0 = 11): writes 3, then 64|11=75.
# cpc_set_pen(16, 20) (the border) selects bit 4 set (16) then color 20:
# writes 16, then 64|20=84.
cat >pen_main.c <<'EOF'
#include "cpc.h"
int main() {
    cpc_set_pen(3, 11);
    cpc_set_pen(16, 20);
    return 0;
}
EOF
"$RCC" --target=z80 --format=coff pen_main.c >/dev/null
"$RCC" --link --target=z80 --format=raw --out=pen.raw build/pen_main.o build/cpc.o
result="$(run_z80 "$PWD/pen.raw")" || { echo "FAIL cpc_set_pen: $result" >&2; fail=1; }
IFS='|' read -r _ writes <<<"$result"
python3 - "$writes" <<'PY' || fail=1
import sys
writes = sys.argv[1]
assert writes == "127:3,127:75,127:16,127:84", f"expected 127:3,127:75,127:16,127:84, got {writes!r}"
print("PASS cpc_set_pen_selects_pen_or_border_then_color")
PY

# cpc_set_mode(1) writes 128|1=129 to the Gate Array.
cat >mode_main.c <<'EOF'
#include "cpc.h"
int main() {
    cpc_set_mode(1);
    return 0;
}
EOF
"$RCC" --target=z80 --format=coff mode_main.c >/dev/null
"$RCC" --link --target=z80 --format=raw --out=mode.raw build/mode_main.o build/cpc.o
result="$(run_z80 "$PWD/mode.raw")" || { echo "FAIL cpc_set_mode: $result" >&2; fail=1; }
IFS='|' read -r _ writes <<<"$result"
python3 - "$writes" <<'PY' || fail=1
import sys
writes = sys.argv[1]
assert writes == "127:129", f"expected 127:129, got {writes!r}"
print("PASS cpc_set_mode_writes_mode_and_rom_config_byte")
PY

# cpc_psg_write(7, 0x3F) runs the real 6-write PPI sequence: register
# number to port A ($F4), select ($F6=192), inactive ($F6=0), data to
# port A ($F4), write ($F6=128), inactive ($F6=0).
cat >psg_main.c <<'EOF'
#include "cpc.h"
int main() {
    cpc_psg_write(7, 63);
    return 0;
}
EOF
"$RCC" --target=z80 --format=coff psg_main.c >/dev/null
"$RCC" --link --target=z80 --format=raw --out=psg.raw build/psg_main.o build/cpc.o
result="$(run_z80 "$PWD/psg.raw")" || { echo "FAIL cpc_psg_write: $result" >&2; fail=1; }
IFS='|' read -r _ writes <<<"$result"
python3 - "$writes" <<'PY' || fail=1
import sys
writes = sys.argv[1]
assert writes == "244:7,246:192,246:0,244:63,246:128,246:0", f"got {writes!r}"
print("PASS cpc_psg_write_runs_the_real_ppi_sequence")
PY

# cpc_cls()/cpc_set_pixel() are plain memory stores into $C000-$FFFF,
# using the real CPC screen address formula (each character row's own 8
# scanlines are 2048 bytes apart) - set two pixels, clear one, and
# confirm only the one surviving bit is set anywhere in the 16KB bitmap.
cat >pixel_main.c <<'EOF'
#include "cpc.h"
int main() {
    cpc_cls();
    cpc_set_pixel(10, 20, 1);
    cpc_set_pixel(11, 20, 1);
    cpc_set_pixel(10, 20, 0);
    return 0;
}
EOF
"$RCC" --target=z80 --format=coff pixel_main.c >/dev/null
"$RCC" --link --target=z80 --format=raw --out=pixel.raw build/pixel_main.o build/cpc.o
result="$(run_z80 "$PWD/pixel.raw")" || { echo "FAIL cpc_cls/cpc_set_pixel: $result" >&2; fail=1; }
IFS='|' read -r mem_hex _ <<<"$result"
python3 - "$mem_hex" <<'PY' || fail=1
import sys
mem = bytes.fromhex(sys.argv[1])
screen = mem[0xC000:0xC000 + 16384]


def addr_bit(x, y):
    char_row = y >> 3
    line = y & 7
    addr = char_row * 80 + (line << 11) + (x >> 3)
    return addr, 128 >> (x & 7)


off, bit = addr_bit(11, 20)
assert screen[off] & bit, f"expected pixel (11,20) set at offset {off:#x} bit {bit:#x}"
off10, bit10 = addr_bit(10, 20)
assert not (screen[off10] & bit10), "pixel (10,20) should have been cleared again"

# Only the first 15872 bytes are cpc_cls()'s own responsibility - the last 512
# ($FE00-$FFFF) are deliberately left untouched (see cpc.h's own header comment), so they're
# excluded here rather than asserted zero.
total_set_bits = sum(bin(b).count("1") for b in screen[:15872])
assert total_set_bits == 1, f"expected exactly one set bit in the safe region, got {total_set_bits}"
print("PASS cpc_cls_and_cpc_set_pixel_write_screen_memory")
PY

# --format=amsdos (AmsdosFormat.cpp) wraps the exact same linked program
# --format=raw produces in a real 128-byte AMSDOS header - see
# amsdos_format_test.cpp for the format's own unit tests; this only
# checks a real linked program round-trips the same way, at plain
# z80's own default base ($8000).
"$RCC" --link --target=z80 --format=amsdos --out=main.bin build/pixel_main.o build/cpc.o
python3 - "$PWD" <<'PY' || fail=1
import sys
d = sys.argv[1] + "/"
flat = open(d + "pixel.raw", "rb").read()
bin_ = open(d + "main.bin", "rb").read()
assert len(bin_) == 128 + len(flat), f"expected a 128-byte header plus {len(flat)} bytes, got {len(bin_)}"

data_len = bin_[19] | (bin_[20] << 8)
assert data_len == len(flat), f"data length field should be {len(flat)}, got {data_len}"
entry = bin_[26] | (bin_[27] << 8)
assert entry == 0x8000, f"entry address should be $8000, got ${entry:04X}"

checksum = sum(bin_[0:67]) & 0xFFFF
stored = bin_[67] | (bin_[68] << 8)
assert checksum == stored, f"checksum mismatch: computed {checksum:#x}, stored {stored:#x}"

assert bin_[128:] == flat, "the AMSDOS file's own program bytes don't match the raw link"
print("PASS cpc_amsdos_format_matches_raw_link")
PY

exit $fail
