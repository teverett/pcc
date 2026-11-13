#!/usr/bin/env bash
# Exercises three of src/rt/apple2/apple2.c's Monitor-ROM wrapper functions
# for real under py65's 6502 core. None of the three needs a real Apple ][
# ROM image: the halt-loop/result convention (`ac` landing at memory[2]/[3])
# is the same one tests/run_tests.sh's own check_6502() uses, and every ROM
# entry point a wrapper here actually JSRs into is patched with a single
# RTS byte first - this only verifies that each wrapper stages its
# argument(s) into the right memory cell(s) before making that call, not
# anything the real ROM routine would itself do (see
# .github/workflows/platformtests.yml's own 6502-dos33 step for the same
# "patch the ROM entry point with a stub" idea, applied there to COUT/
# CROUT/HOME instead).
#
# Usage: tests/run_apple2_rom_wrapper_tests.sh [path/to/rcc]

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
if ! python3 -c "import py65" >/dev/null 2>&1; then
    echo "py65 not found - install it (e.g. 'pip install py65')" >&2
    exit 1
fi

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT
cd "$tmpdir"

fail=0

# a2_vtab(row) stages `row` straight into rt_a before JSRing to VTAB
# ($FC22, patched here with a bare RTS stub).
cat >vtab_main.c <<'EOF'
extern unsigned char rt_a;
extern void a2_vtab(unsigned char row);
int main() {
    a2_vtab(17);
    return rt_a;
}
EOF
"$RCC" --target=6502 --format=coff vtab_main.c >/dev/null
"$RCC" --target=6502 --format=coff "$ROOT_DIR/src/rt/apple2/apple2.c" >/dev/null
"$RCC" --link --target=6502 --format=raw --out=vtab.flat build/vtab_main.o build/apple2.o

python3 - "$PWD" <<'PY' || fail=1
import sys
from py65.devices.mpu6502 import MPU

data = open(sys.argv[1] + "/vtab.flat", "rb").read()
base = 0x0600
mpu = MPU()
mpu.memory[base:base + len(data)] = data
mpu.memory[0xFC22] = 0x60  # RTS - stand in for the real VTAB routine
mpu.pc = base
for _ in range(200000):
    pc_before = mpu.pc
    mpu.step()
    if mpu.pc == pc_before:
        break
else:
    raise SystemExit("a2_vtab: simulator never reached the halt loop")
val = mpu.memory[2] | (mpu.memory[3] << 8)
assert val == 17, f"a2_vtab: expected rt_a == 17, got {val}"
print("PASS apple2_vtab_stages_row (rt_a=%d)" % val)
PY

# a2_cout1(c) ORs in bit 7 (the monitor's normal-text convention, same as
# a2_cout()'s own input) before JSRing to COUT1 ($FDF0, patched here with a
# bare RTS stub).
cat >cout1_main.c <<'EOF'
extern unsigned char rt_a;
extern void a2_cout1(unsigned char c);
int main() {
    a2_cout1(65);
    return rt_a;
}
EOF
"$RCC" --target=6502 --format=coff cout1_main.c >/dev/null
"$RCC" --link --target=6502 --format=raw --out=cout1.flat build/cout1_main.o build/apple2.o

python3 - "$PWD" <<'PY' || fail=1
import sys
from py65.devices.mpu6502 import MPU

data = open(sys.argv[1] + "/cout1.flat", "rb").read()
base = 0x0600
mpu = MPU()
mpu.memory[base:base + len(data)] = data
mpu.memory[0xFDF0] = 0x60  # RTS - stand in for the real COUT1 routine
mpu.pc = base
for _ in range(200000):
    pc_before = mpu.pc
    mpu.step()
    if mpu.pc == pc_before:
        break
else:
    raise SystemExit("a2_cout1: simulator never reached the halt loop")
val = mpu.memory[2] | (mpu.memory[3] << 8)
assert val == 65 | 128, f"a2_cout1: expected rt_a == 65|128, got {val}"
print("PASS apple2_cout1_sets_high_bit (rt_a=%d)" % val)
PY

# a2_move(from, to, dest) stages the three address pairs into MOVE's own
# zero-page cells (A1L/A1H=$3C/$3D, A2L/A2H=$3E/$3F, A4L/A4H=$42/$43)
# before JSRing to MOVE ($FE2C, patched here with a bare RTS stub).
cat >move_main.c <<'EOF'
extern void a2_move(int from, int to, int dest);
int main() {
    a2_move(770, 1023, 1024);
    return 0;
}
EOF
"$RCC" --target=6502 --format=coff move_main.c >/dev/null
"$RCC" --link --target=6502 --format=raw --out=move.flat build/move_main.o build/apple2.o

python3 - "$PWD" <<'PY' || fail=1
import sys
from py65.devices.mpu6502 import MPU

data = open(sys.argv[1] + "/move.flat", "rb").read()
base = 0x0600
mpu = MPU()
mpu.memory[base:base + len(data)] = data
mpu.memory[0xFE2C] = 0x60  # RTS - stand in for the real MOVE routine
mpu.pc = base
for _ in range(200000):
    pc_before = mpu.pc
    mpu.step()
    if mpu.pc == pc_before:
        break
else:
    raise SystemExit("a2_move: simulator never reached the halt loop")

def word(lo_addr):
    return mpu.memory[lo_addr] | (mpu.memory[lo_addr + 1] << 8)

assert word(0x3C) == 770, f"a2_move: A1L/A1H expected 770, got {word(0x3C)}"
assert word(0x3E) == 1023, f"a2_move: A2L/A2H expected 1023, got {word(0x3E)}"
assert word(0x42) == 1024, f"a2_move: A4L/A4H expected 1024, got {word(0x42)}"
print("PASS apple2_move_zero_page_staging")
PY

exit $fail
