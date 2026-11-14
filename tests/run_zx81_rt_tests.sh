#!/usr/bin/env bash
# Exercises every function in src/rt/zx81/zx81.c for real under a real, independent Z80 core
# (`z80-python`, same package tests/run_trs80_rt_tests.sh/run_coleco_rt_tests.sh/
# run_zxspectrum_tests.sh/run_cpc_rt_tests.sh use). This only confirms the port sequence this
# runtime documents is what actually executes - see zx81.h's own header comment for why there's
# no emulator or hardware reference available to check that documented sequence itself against.
#
# Usage: tests/run_zx81_rt_tests.sh [path/to/rcc]

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

cp "$ROOT_DIR/src/rt/zx81/zx81.h" "$ROOT_DIR/src/rt/zx81/zx81.c" .
"$RCC" --target=z80 --format=coff zx81.c >/dev/null

# Shared harness: runs a .flat image (linked at $8000, plain z80's own default base) on a real
# Z80 core, with a configurable read_port (defaulting to 0xFF) and a record of every write.
# Prints "HL=<int>|WRITES=<port>:<value>,...".
run_z80() {
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
sys.stdout.write(str(hl) + "|" + ",".join(f"{p}:{v}" for p, v in writes))
PY
}

# zx81_display_on()/zx81_display_off() issue exactly one OUT each, to $FE
# then $FD.
cat >display_main.c <<'EOF'
#include "zx81.h"
int main() {
    zx81_display_on();
    zx81_display_off();
    return 0;
}
EOF
"$RCC" --target=z80 --format=coff display_main.c >/dev/null
"$RCC" --link --target=z80 --format=raw --out=display.raw build/display_main.o build/zx81.o
result="$(run_z80 "$PWD/display.raw")" || { echo "FAIL zx81_display_on/off: $result" >&2; fail=1; }
IFS='|' read -r _ writes <<<"$result"
python3 - "$writes" <<'PY' || fail=1
import sys
writes = sys.argv[1]
assert writes == "254:0,253:0", f"expected 254:0,253:0, got {writes!r}"
print("PASS zx81_display_on_and_off_write_fe_then_fd")
PY

# zx81_key_row(5) selects half-row 5 ($DF) on the upper address byte and
# reads port $FE - z80-python's own IN A,(n) models the real A:n port
# address (same mechanism tests/run_zxspectrum_tests.sh's own
# zx_key_row test checks), so supplying read_port only for port
# (0xDF<<8)|0xFE confirms the half-row select byte actually reached the
# bus.
cat >key_main.c <<'EOF'
#include "zx81.h"
int main() {
    return zx81_key_row(5);
}
EOF
"$RCC" --target=z80 --format=coff key_main.c >/dev/null
"$RCC" --link --target=z80 --format=raw --out=key.raw build/key_main.o build/zx81.o
want_port=$(( (0xDF << 8) | 0xFE ))
result="$(run_z80 "$PWD/key.raw" "$want_port:17")" || { echo "FAIL zx81_key_row: $result" >&2; fail=1; }
IFS='|' read -r hl _ <<<"$result"
python3 - "$hl" <<'PY' || fail=1
import sys
hl = int(sys.argv[1])
assert hl == 17, f"expected zx81_key_row(5) == 17 (the row-5-only read_port answer), got {hl}"
print("PASS zx81_key_row_selects_half_row_on_upper_address_byte")
PY

exit $fail
