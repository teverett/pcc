#!/usr/bin/env bash
# Checks --target=6502/65c02 --link --format=atari (AtariExeFormat.cpp)
# byte-for-byte against the same program's own --format=raw link (see
# src/fileformat/atari/AtariExeFormat.h's own header comment for exactly
# what the segment headers and the RUNAD poke mean), then runs the
# --format=raw link of a src/rt/atari400/atari400.c-based "hello world"
# in py65's 6502 MPU core, with CIO's CIOV ($E456) patched with a tiny
# hand-written stub (checks IOCB 0's own ICCOM cell for PUTCHR and, if
# so, appends A into a buffer - see below), to confirm atari400.c's real
# `asm { jsr ... }`/`ldxi` calls actually produce the right bytes, not
# just that they assemble.
#
# Usage: tests/run_atari400_format_tests.sh [path/to/rcc]

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
    "$RCC" --target="$target" --format=coff "$ROOT_DIR/src/rt/atari400/atari400.c" >/dev/null
    "$RCC" --link --target="$target" --format=raw --base=8192 --out="prog_$target.raw" \
        "build/main.o" "build/atari400.o"
    "$RCC" --link --target="$target" --format=atari --base=8192 --out="prog_$target.atari" \
        "build/main.o" "build/atari400.o"

    python3 - "$PWD" "$target" <<'PY' || fail=1
import sys
d, target = sys.argv[1] + "/", sys.argv[2]
flat = open(d + "prog_" + target + ".raw", "rb").read()
exe = open(d + "prog_" + target + ".atari", "rb").read()
assert exe[0] == 0xFF and exe[1] == 0xFF, f"{target}: missing leading $FFFF marker"
start = exe[2] | (exe[3] << 8)
end = exe[4] | (exe[5] << 8)
assert start == 8192, f"{target}: segment start: expected 8192, got {start}"
assert end - start + 1 == len(flat), f"{target}: segment length doesn't match the raw link"
assert exe[6:6 + len(flat)] == flat, f"{target}: segment payload doesn't match the raw link"
tail = exe[6 + len(flat):]
runad_lo = tail[0] | (tail[1] << 8)
runad_hi = tail[2] | (tail[3] << 8)
assert runad_lo == 0x02E0 and runad_hi == 0x02E1, f"{target}: RUNAD segment addresses are wrong"
runad_value = tail[4] | (tail[5] << 8)
assert runad_value == 8192, f"{target}: RUNAD poke should be 8192, got {runad_value}"
print(f"PASS atari400_exe_format_{target} (start={start}, runad={runad_value})")
PY
done

cat >hello.c <<'EOF'
#include "atari400.h"

int main() {
    atari_clrscr();
    atari_puts("HELLO, WORLD!");
    atari_crout();
    return 0;
}
EOF
cp "$ROOT_DIR/src/rt/atari400/atari400.h" .

for target in 6502 65c02; do
    "$RCC" --target="$target" --format=coff --out="hello_$target.o" hello.c >/dev/null
    "$RCC" --target="$target" --format=coff --out="atari400_$target.o" "$ROOT_DIR/src/rt/atari400/atari400.c" >/dev/null
    "$RCC" --link --target="$target" --format=raw --base=8192 --out="hello_$target.raw" \
        "hello_$target.o" "atari400_$target.o"

    python3 - "$PWD" "$target" <<'PY' || fail=1
import sys
from py65.devices.mpu65c02 import MPU as MPU65C02
from py65.devices.mpu6502 import MPU as MPU6502

d, target = sys.argv[1] + "/", sys.argv[2]
data = open(d + "hello_" + target + ".raw", "rb").read()
base = 0x2000
mpu = (MPU65C02 if target == "65c02" else MPU6502)()
mpu.memory[base:base + len(data)] = data

# CIOV ($E456) stub: LDY $0342 (read ICCOM without disturbing A); CPY #11
# (PUTCHR); if not PUTCHR, skip; otherwise append A verbatim into a
# buffer at $0400, tracked via a zero-page position counter at $F0.
stub = [0xAC, 0x42, 0x03, 0xC0, 0x0B, 0xD0, 0x08, 0xA6, 0xF0, 0x9D, 0x00, 0x04, 0xE8, 0x86, 0xF0, 0x60]
mpu.memory[0xE456:0xE456 + len(stub)] = stub
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
expected = bytes([125]) + b"HELLO, WORLD!" + bytes([155])
assert out == expected, f"{target}: expected {expected!r}, got {out!r}"
print(f"PASS atari400_cio_putchr_wrapper_{target}")
PY
done

exit $fail
