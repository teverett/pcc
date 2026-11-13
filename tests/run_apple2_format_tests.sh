#!/usr/bin/env bash
# Checks --target=6502 --link --format=dos33/prodos (Dos33Format.cpp,
# ProDosFormat.cpp) byte-for-byte against the same program's own
# --format=raw link - see src/fileformat/dos33/Dos33Format.h's and
# src/fileformat/prodos/ProDosFormat.h's own header comments for exactly
# what each format's extra bytes mean. No 6502 simulator needed: both
# formats are pure byte-layout conventions around an already-linked image,
# not something that runs.
#
# Usage: tests/run_apple2_format_tests.sh [path/to/rcc]

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

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT
cd "$tmpdir"

cat >main.c <<'EOF'
int main() {
    return 1;
}
EOF

"$RCC" --target=6502 --format=coff main.c >/dev/null
"$RCC" --target=6502 --format=coff "$ROOT_DIR/src/rt/apple2/apple2.c" >/dev/null
"$RCC" --link --target=6502 --format=raw --out=prog.flat build/main.o build/apple2.o
"$RCC" --link --target=6502 --format=dos33 --out=prog.dos33 build/main.o build/apple2.o
"$RCC" --link --target=6502 --format=prodos --out=prog.prodos build/main.o build/apple2.o

fail=0

python3 - "$PWD" <<'PY' || fail=1
import sys
d = sys.argv[1] + "/"
flat = open(d + "prog.flat", "rb").read()

dos33 = open(d + "prog.dos33", "rb").read()
load = dos33[0] | (dos33[1] << 8)
length = dos33[2] | (dos33[3] << 8)
assert load == 1536, f"dos33 load address: expected 1536, got {load}"
assert length == len(flat), f"dos33 length header: expected {len(flat)}, got {length}"
assert dos33[4:] == flat, "dos33 payload doesn't match the flat link"
print("PASS apple2_dos33_format (load=%d len=%d)" % (load, length))

prodos = open(d + "prog.prodos", "rb").read()
file_type = prodos[-3]
aux_type = prodos[-2] | (prodos[-1] << 8)
assert file_type == 6, f"prodos file_type: expected 6 (BIN), got {file_type}"
assert aux_type == 1536, f"prodos aux_type: expected 1536, got {aux_type}"
assert prodos[:-3] == flat, "prodos payload doesn't match the flat link"
print("PASS apple2_prodos_format (file_type=%d aux_type=%d)" % (file_type, aux_type))
PY

# --base: a ProDOS SYS file is always entered at $2000 (8192) by ProDOS's
# own loader, regardless of aux_type - see src/linker/factory/
# M6502LinkerFactory.h's own comment - so --link --base must be able to
# move where the program actually runs, not just what its directory-entry
# aux_type claims.
for target in 6502 65c02; do
    "$RCC" --target="$target" --format=coff main.c >/dev/null
    "$RCC" --target="$target" --format=coff "$ROOT_DIR/src/rt/apple2/apple2.c" >/dev/null
    "$RCC" --link --target="$target" --format=prodos --base=8192 --out="prog_$target.prodos" \
        "build/main.o" "build/apple2.o"
    "$RCC" --link --target="$target" --format=raw --base=8192 --out="prog_$target.flat" \
        "build/main.o" "build/apple2.o"

    python3 - "$PWD" "$target" <<'PY' || fail=1
import sys
from py65.devices.mpu65c02 import MPU as MPU65C02
from py65.devices.mpu6502 import MPU as MPU6502

d, target = sys.argv[1] + "/", sys.argv[2]
prodos = open(d + "prog_" + target + ".prodos", "rb").read()
aux_type = prodos[-2] | (prodos[-1] << 8)
assert aux_type == 8192, f"{target}: --base=8192 should set aux_type=8192, got {aux_type}"

data = open(d + "prog_" + target + ".flat", "rb").read()
base = 0x2000
mpu = (MPU65C02 if target == "65c02" else MPU6502)()
mpu.memory[base:base + len(data)] = data
mpu.pc = base
for _ in range(200000):
    pc_before = mpu.pc
    mpu.step()
    if mpu.pc == pc_before:
        break
else:
    raise SystemExit(f"{target}: simulator never reached the halt loop with --base=8192")
val = mpu.memory[2] | (mpu.memory[3] << 8)
if val >= 0x8000:
    val -= 0x10000
assert val == 1, f"{target}: expected 1 running at --base=8192, got {val}"
print(f"PASS apple2_base_8192_{target} (aux_type={aux_type} result={val})")
PY
done

exit $fail
