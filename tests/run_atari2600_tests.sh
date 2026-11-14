#!/usr/bin/env bash
# Checks --target=atari2600: that it actually relocates its zero-page
# pseudo-registers/frame-storage region clear of the Atari 2600's TIA
# register mirror (see M6502Codegen.h's own `zeroPageBase()`/
# `frameStackBase()` comments), that --link --format=a26 produces a
# correctly-sized, correctly-vectored cartridge image at both standard
# sizes, that a real program actually runs correctly under that memory
# map (in py65's bare 6502 core - no TIA hardware is simulated, so this
# only confirms the compiler's own codegen/linker correctness, not real
# hardware behavior), and that --link rejects mixing an atari2600 object
# with a plain 6502 one - the real point of giving atari2600 its own
# distinct machine ID (kM6502Atari2600CoffMachine).
#
# Usage: tests/run_atari2600_tests.sh [path/to/rcc]

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
"$RCC" --target=atari2600 --format=coff --out=main.o main.c >/dev/null

# --format=a26 at both standard cartridge sizes: correct total size, and
# every one of NMI/RESET/IRQ (the trailing 6 bytes) pointing at the
# program's own base.
for pair in "F000:4096" "F800:2048"; do
    base="${pair%%:*}"
    size="${pair##*:}"
    "$RCC" --link --target=atari2600 --format=a26 --base="\$$base" --out="main_$base.a26" main.o

    python3 - "$PWD" "$base" "$size" <<'PY' || fail=1
import sys
d, base, size = sys.argv[1] + "/", int(sys.argv[2], 16), int(sys.argv[3])
data = open(d + "main_%04X.a26" % base, "rb").read()
assert len(data) == size, f"${base:04X}: expected {size} bytes, got {len(data)}"
nmi = data[-6] | (data[-5] << 8)
reset = data[-4] | (data[-3] << 8)
irq = data[-2] | (data[-1] << 8)
assert nmi == reset == irq == base, f"${base:04X}: vectors should all equal {base:04X}, got NMI={nmi:04X} RESET={reset:04X} IRQ={irq:04X}"
print(f"PASS atari2600_a26_format_{base:04X} (size={size})")
PY
done

# The zero-page pseudo-registers/frame-storage region: codegen for
# atari2600 must place every one of them at $0080 or above, never inside
# TIA's own $0000-$003F write-register mirror - checked directly against
# the assembled bytes (the `sta sp`/`sta sp+1` initialization in _start),
# not just by inference from the source.
"$RCC" --link --target=atari2600 --format=raw --base='$F000' --out=main.raw main.o
python3 - "$PWD" <<'PY' || fail=1
import sys
d = sys.argv[1] + "/"
data = open(d + "main.raw", "rb").read()
# _start: ldai <sp_lo>; sta sp; ldai <sp_hi>; sta sp+1 - opcodes A9 <lo> 8D <addr_lo> <addr_hi> A9 <hi> 8D <addr_lo+1> <addr_hi>
assert data[0] == 0xA9, "expected _start to begin with LDA #imm (A9)"
sp_lo_value = data[1]
assert data[2] == 0x8D, "expected STA abs (8D) after the first LDA"
sp_addr = data[3] | (data[4] << 8)
assert sp_addr >= 0x0080, f"sp's own zero-page address ({sp_addr:#x}) must be at $0080 or above - $0000-$003F is TIA's own write-register mirror"
print(f"PASS atari2600_zero_page_clear_of_tia (sp seeded to {sp_addr:#x}, initial value {sp_lo_value:#x})")
PY

# A real program actually executes correctly under this memory map (py65
# bare core - no TIA registers simulated, so this exercises the
# compiler's own codegen/linker output, not real 2600 hardware behavior).
python3 - "$PWD" <<'PY' || fail=1
import sys
from py65.devices.mpu6502 import MPU

d = sys.argv[1] + "/"
data = open(d + "main.raw", "rb").read()
base = 0xF000
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
val = mpu.memory[0x82] | (mpu.memory[0x83] << 8)  # ac/ac+1 at zeroPageBase()+2
if val >= 0x8000:
    val -= 0x10000
assert val == 42, f"expected 42, got {val}"
print("PASS atari2600_runs_correctly (result=%d)" % val)
PY

# The real point of atari2600's own distinct machine ID: --link must
# reject mixing an atari2600 object with a plain 6502 one, instead of
# silently linking two objects whose zero-page/frame-storage layouts
# disagree.
"$RCC" --target=6502 --format=coff --out=main6502.o main.c >/dev/null
if "$RCC" --link --target=6502 --format=raw --out=bad.raw main.o 2>/tmp/atari2600_mismatch_err.txt; then
    echo "FAIL atari2600_rejects_cross_target_link: expected this link to fail" >&2
    fail=1
elif ! grep -q "machine type" /tmp/atari2600_mismatch_err.txt; then
    echo "FAIL atari2600_rejects_cross_target_link: wrong error:" >&2
    cat /tmp/atari2600_mismatch_err.txt >&2
    fail=1
else
    echo "PASS atari2600_rejects_cross_target_link"
fi
rm -f /tmp/atari2600_mismatch_err.txt

exit $fail
