#!/usr/bin/env bash
# Checks --target=6502/65c02 --link --format=prg (PrgFormat.cpp) byte-for-byte
# against the same program's own --format=raw link at the C64's own BASIC
# start address ($0801, 2049 - see src/fileformat/prg/PrgFormat.h's own
# header comment for what the 2-byte header means), then runs the
# --format=raw link of a src/rt/c64/c64.c-based "hello world" in py65's
# 6502 MPU core, with KERNAL's CHROUT ($FFD2) patched with a tiny hand-
# written stub (same shape as tests/run_vic20_format_tests.sh's own CHROUT
# stub - the two machines share the identical KERNAL jump-table addresses,
# see c64.h's own header comment), to confirm c64.c's real
# `asm { jsr ... }` calls actually produce the right bytes, not just that
# they assemble.
#
# Usage: tests/run_c64_format_tests.sh [path/to/rcc]

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

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT
cd "$tmpdir"

cat >main.c <<'EOF'
int main() {
    return 1;
}
EOF

fail=0

for target in 6502 65c02; do
    "$RCC" --target="$target" --format=coff main.c >/dev/null
    "$RCC" --target="$target" --format=coff "$ROOT_DIR/src/rt/c64/c64.c" >/dev/null
    "$RCC" --link --target="$target" --format=raw --base=2049 --out="prog_$target.raw" \
        "build/main.o" "build/c64.o"
    "$RCC" --link --target="$target" --format=prg --base=2049 --out="prog_$target.prg" \
        "build/main.o" "build/c64.o"

    python3 - "$PWD" "$target" <<'PY' || fail=1
import sys
d, target = sys.argv[1] + "/", sys.argv[2]
flat = open(d + "prog_" + target + ".raw", "rb").read()
prg = open(d + "prog_" + target + ".prg", "rb").read()
load = prg[0] | (prg[1] << 8)
assert load == 2049, f"{target}: prg load address: expected 2049 ($0801), got {load}"
assert prg[2:] == flat, f"{target}: prg payload doesn't match the raw link"
print(f"PASS c64_prg_format_{target} (load={load})")
PY
done

cat >hello.c <<'EOF'
#include "c64.h"

int main() {
    c64_clrscr();
    c64_puts("HELLO, WORLD!");
    c64_crout();
    return 0;
}
EOF
cp "$ROOT_DIR/src/rt/c64/c64.h" .

for target in 6502 65c02; do
    "$RCC" --target="$target" --format=coff --out="hello_$target.o" hello.c >/dev/null
    "$RCC" --target="$target" --format=coff --out="c64_$target.o" "$ROOT_DIR/src/rt/c64/c64.c" >/dev/null
    "$RCC" --link --target="$target" --format=raw --base=2049 --out="hello_$target.raw" \
        "hello_$target.o" "c64_$target.o"

    python3 - "$PWD" "$target" <<'PY' || fail=1
import sys
from py65.devices.mpu65c02 import MPU as MPU65C02
from py65.devices.mpu6502 import MPU as MPU6502

d, target = sys.argv[1] + "/", sys.argv[2]
data = open(d + "hello_" + target + ".raw", "rb").read()
base = 0x0801
mpu = (MPU65C02 if target == "65c02" else MPU6502)()
mpu.memory[base:base + len(data)] = data

# CHROUT ($FFD2) stub: append A verbatim (no high-bit masking needed -
# see c64.c's own c64_chrout() comment) into a buffer at $0400, tracked
# via a zero-page position counter at $F0. Placing the buffer at $0400
# (the C64's own default screen RAM) is harmless here - nothing else in
# this simulated run reads or writes it.
mpu.memory[0xFFD2:0xFFD2 + 9] = [0xA6, 0xF0, 0x9D, 0x00, 0x04, 0xE8, 0x86, 0xF0, 0x60]
mpu.memory[0xF0] = 0

mpu.pc = base
for _ in range(2_000_000):
    pc_before = mpu.pc
    mpu.step()
    if mpu.pc == pc_before:
        break
else:
    raise SystemExit(f"{target}: simulator never reached the halt loop")

n = mpu.memory[0xF0]
out = bytes(mpu.memory[0x0400 + i] for i in range(n))
expected = bytes([147]) + b"HELLO, WORLD!" + bytes([13])
assert out == expected, f"{target}: expected {expected!r}, got {out!r}"
print(f"PASS c64_chrout_rom_wrapper_{target}")
PY
done

exit $fail
