#!/usr/bin/env bash
# Checks --target=2a03: that --link --format=nes produces a correctly-sized,
# correctly-headered, correctly-vectored iNES ROM image, that a real program
# actually runs correctly under its own memory map (in py65's bare 6502 core
# - no PPU/APU/mapper hardware is simulated, so this only confirms the
# compiler's own codegen/linker correctness, not real hardware behavior),
# and that --link rejects mixing a 2a03 object with a plain 6502 one - the
# real point of giving 2a03 its own distinct machine ID
# (kM6502Nes2A03CoffMachine).
#
# Usage: tests/run_2a03_tests.sh [path/to/rcc]

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

fail=0

cat >main.c <<'EOF'
int main() {
    return 42;
}
EOF
"$RCC" --target=2a03 --format=coff --out=main.o main.c >/dev/null

# --format=nes: a correctly-sized iNES image (16-byte header + 32768-byte
# PRG-ROM bank), correct signature/PRG-size header fields, and every one of
# NMI/RESET/IRQ (the trailing 6 bytes of the PRG-ROM bank) pointing at the
# program's own base.
"$RCC" --link --target=2a03 --format=nes --out=main.nes main.o

python3 - "$PWD" <<'PY' || fail=1
import sys
d = sys.argv[1] + "/"
data = open(d + "main.nes", "rb").read()
assert len(data) == 16 + 32768, f"expected {16 + 32768} bytes, got {len(data)}"
assert data[0:4] == b"NES\x1a", f"missing iNES signature, got {data[0:4]!r}"
assert data[4] == 2, f"PRG-ROM size field should be 2 (32768/16384), got {data[4]}"
nmi = data[-6] | (data[-5] << 8)
reset = data[-4] | (data[-3] << 8)
irq = data[-2] | (data[-1] << 8)
assert nmi == reset == irq == 0x8000, f"vectors should all equal 0x8000, got NMI={nmi:04X} RESET={reset:04X} IRQ={irq:04X}"
print("PASS 2a03_nes_format (size=%d)" % len(data))
PY

# A real program actually executes correctly under this memory map (py65
# bare core - no PPU/APU/mapper hardware simulated, so this exercises the
# compiler's own codegen/linker output, not real NES hardware behavior).
"$RCC" --link --target=2a03 --format=raw --out=main.raw main.o
python3 - "$PWD" <<'PY' || fail=1
import sys
from py65.devices.mpu6502 import MPU

d = sys.argv[1] + "/"
data = open(d + "main.raw", "rb").read()
base = 0x8000
mpu = MPU()
mpu.memory[base:base + len(data)] = data
mpu.pc = base
for _ in range(200000):
    pc_before = mpu.pc
    mpu.step()
    if mpu.pc == pc_before:
        break
else:
    raise SystemExit("simulator never reached the halt loop")
val = mpu.memory[0x02] | (mpu.memory[0x03] << 8)  # ac/ac+1 at zeroPageBase()+2
if val >= 0x8000:
    val -= 0x10000
assert val == 42, f"expected 42, got {val}"
print("PASS 2a03_runs_correctly (result=%d)" % val)
PY

# The real point of 2a03's own distinct machine ID: --link must reject
# mixing a 2a03 object with a plain 6502 one, instead of silently linking
# two objects whose own fixed base addresses (and, for any future mapper
# with its own RAM reservation, memory-map assumptions) disagree.
"$RCC" --target=6502 --format=coff --out=main6502.o main.c >/dev/null
if "$RCC" --link --target=6502 --format=raw --out=bad.raw main.o 2>/tmp/2a03_mismatch_err.txt; then
    echo "FAIL 2a03_rejects_cross_target_link: expected this link to fail" >&2
    fail=1
elif ! grep -q "machine type" /tmp/2a03_mismatch_err.txt; then
    echo "FAIL 2a03_rejects_cross_target_link: wrong error:" >&2
    cat /tmp/2a03_mismatch_err.txt >&2
    fail=1
else
    echo "PASS 2a03_rejects_cross_target_link"
fi
rm -f /tmp/2a03_mismatch_err.txt

exit $fail
