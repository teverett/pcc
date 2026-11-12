#!/usr/bin/env bash
# Unit tests for the sparc wide-truthiness bug fix (SparcCodegen.cpp), sparc only.
#
# Background: `if`/`while`/`?:`/`&&`/`||`/`!` combine a wide (`long long`) condition's two
# 32-bit halves (t0 = low word, t1 = high word) into one truthiness test. The combining
# instruction was emitted as `or t0,t0,t1` - but SPARC's own `or rs1,rs2,rd` puts the
# destination *last*, so that line actually computed `t1 = t0|t0` (discarding the real high
# word) and left t0, the register every following `cmp t0,0` tests, completely unmodified.
# A wide value whose low word is zero but whose high word isn't (e.g. `1LL << 32`) tests
# false. The fix: `or t0,t1,t0` - the real OR, written into t0.
#
# Two tests, both compiled --format=raw and run directly under qemu-system-sparc (this
# target has no automated `run_tests.sh` entry yet - see docs/platforms.md's own sparc
# entry - so this script drives qemu itself, the same way run_tests.sh's own check_powerpc()
# does):
#   test_if_wide_truthiness   - `if (x)` with x = 1LL<<32 (low word 0, high word 1) must take
#       the true branch (the exact shape docs/known-bugs.md's own retired entry named).
#   test_logical_not_wide_truthiness - `!x`, same x, exercises a second combining site
#       (genLogicalNot) and the opposite expected truthiness (x is truthy, so !x is 0).
#
# Usage: tests/run_sparc_wide_truthiness_tests.sh [path/to/rcc]

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RCC="$SCRIPT_DIR/../build/bin/rcc"
[ $# -ge 1 ] && RCC="$1"

if [ ! -x "$RCC" ]; then
    echo "rcc binary not found at $RCC - build it first ('make')" >&2
    exit 1
fi
if ! command -v qemu-system-sparc >/dev/null 2>&1; then
    echo "qemu-system-sparc not found on PATH - install it (e.g. 'brew install qemu')" >&2
    exit 1
fi
if ! command -v python3 >/dev/null 2>&1; then
    echo "python3 not found on PATH - needed to talk to qemu's monitor" >&2
    exit 1
fi

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT
fail=0
qemu_count=0

to_signed() {
    local value="$1" bits="$2"
    local max=$((1 << (bits - 1)))
    local mod=$((1 << bits))
    if [ "$value" -ge "$max" ]; then
        echo $((value - mod))
    else
        echo "$value"
    fi
}

# Runs `src` under qemu-system-sparc and prints %o0 (this backend's `t0`, holding the
# program's own result at its `halt: ba halt` spin loop) as a signed decimal.
run_sparc() {
    local name="$1" src="$2"
    local binfile="$tmpdir/$name.bin"

    if ! "$RCC" --target=sparc --format=raw --out="$binfile" "$src" >"$tmpdir/$name.log" 2>&1; then
        echo "FAIL $name: compile error" >&2
        sed 's/^/    /' "$tmpdir/$name.log" >&2
        return 1
    fi

    qemu_count=$((qemu_count + 1))
    local monitor="$tmpdir/mon$qemu_count.sock"
    qemu-system-sparc -cpu TI-SuperSparc-II -M none -m 16M \
        -device "loader,file=$binfile,addr=0x00004000,cpu-num=0" \
        -device "loader,addr=0x00004000,cpu-num=0" \
        -monitor "unix:$monitor,server,nowait" -nographic -serial none -display none -no-reboot \
        >"$tmpdir/$name.qemu.log" 2>&1 &
    local qemu_pid=$!
    ( sleep 40; kill -9 "$qemu_pid" 2>/dev/null ) &
    local watcher=$!

    local outfile="$tmpdir/$name.out"
    local status=0
    python3 -I "$SCRIPT_DIR/qemu_wait.py" "$monitor" 30 >"$outfile" 2>"$tmpdir/$name.wait.log" || status=$?
    kill "$qemu_pid" 2>/dev/null; wait "$qemu_pid" 2>/dev/null
    kill "$watcher" 2>/dev/null; wait "$watcher" 2>/dev/null

    if [ "$status" -ne 0 ]; then
        echo "FAIL $name: qemu execution failed (monitor exit $status)" >&2
        sed 's/^/    /' "$tmpdir/$name.wait.log" "$outfile" "$tmpdir/$name.qemu.log" >&2
        return 1
    fi

    local o0_hex
    o0_hex="$(grep '^%o0-7:' "$outfile" | awk '{print $2}')"
    if [ -z "$o0_hex" ]; then
        echo "FAIL $name: qemu produced no %o0-7 row (timed out or crashed)" >&2
        return 1
    fi
    to_signed "$((16#$o0_hex))" 32
}

test_if_wide_truthiness() {
    local dir="$tmpdir/if_wide"
    mkdir -p "$dir"
    cat >"$dir/t.c" <<'EOF'
int main() {
    long long x;
    x = 1LL << 32;
    if (x) {
        return 1;
    }
    return 0;
}
EOF
    local result
    result="$(run_sparc if_wide "$dir/t.c")" || return 1
    if [ "$result" -ne 1 ]; then
        echo "FAIL test_if_wide_truthiness: expected 1 (a long long whose low word is 0 but" \
             "high word is 1 must still be truthy), got $result"
        return 1
    fi
    echo "PASS test_if_wide_truthiness (result=$result)"
    return 0
}

test_logical_not_wide_truthiness() {
    local dir="$tmpdir/not_wide"
    mkdir -p "$dir"
    cat >"$dir/t.c" <<'EOF'
int main() {
    long long x;
    x = 1LL << 32;
    return !x;
}
EOF
    local result
    result="$(run_sparc not_wide "$dir/t.c")" || return 1
    if [ "$result" -ne 0 ]; then
        echo "FAIL test_logical_not_wide_truthiness: expected 0 (x is truthy, so !x must be" \
             "false), got $result"
        return 1
    fi
    echo "PASS test_logical_not_wide_truthiness (result=$result)"
    return 0
}

test_if_wide_truthiness || fail=1
test_logical_not_wide_truthiness || fail=1

exit $fail
