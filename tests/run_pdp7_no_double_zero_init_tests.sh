#!/usr/bin/env bash
# Unit tests for Pdp7Codegen.cpp's `LocalDecl` fix: an uninitialized scalar
# local used to be zeroed twice - once by a blanket defensive `lac __zero`
# store Pdp7Codegen itself inserted for every scalar Local with no
# initializer (regardless of whether anything later actually depended on
# that zero), and again by whatever real initializer followed (e.g. a
# `for` loop's own `i = 0`). A leaf function's Local already gets a
# statically zero-initialized cell (the assembler's own "LF1: 0" trailer
# entry); a non-leaf function's frame slot is just uninitialized memory
# the same way a real C local is, on every other backend (i386, 6502, ...)
# - neither case needs Pdp7Codegen's own extra store. See
# docs/known-bugs.md's former "double-zeroing" entry.
#
# Two tests:
#   test_leaf_no_defensive_zero     - a leaf function (no calls) with an
#       uninitialized scalar local that's immediately overwritten must
#       emit no `lac __zero` at all - the static cell is already zero,
#       so there is nothing left needing a store before the overwrite.
#   test_nonleaf_no_defensive_zero  - same shape, but the function makes a
#       call (forcing the frame-based, non-leaf path) - must also emit no
#       `lac __zero`, and must still run correctly under the real `pdp7`
#       simulator.
#
# Usage: tests/run_pdp7_no_double_zero_init_tests.sh [path/to/rcc]

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RCC="$SCRIPT_DIR/../build/bin/rcc"
[ $# -ge 1 ] && RCC="$1"

if [ ! -x "$RCC" ]; then
    echo "rcc binary not found at $RCC - build it first ('make')" >&2
    exit 1
fi
if ! command -v pdp7 >/dev/null 2>&1; then
    echo "pdp7 (open-simh) not found on PATH - install it (e.g. 'brew install open-simh')" >&2
    exit 1
fi

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT
fail=0

test_leaf_no_defensive_zero() {
    local dir="$tmpdir/leaf"
    mkdir -p "$dir"
    cat >"$dir/t.c" <<'EOF'
int main() {
    int x;
    x = 5;
    return x;
}
EOF
    if ! "$RCC" --target=pdp7 --format=asm --out="$dir/t.s" "$dir/t.c" >"$dir/log" 2>&1; then
        echo "FAIL test_leaf_no_defensive_zero: fixture failed to compile"
        cat "$dir/log"
        return 1
    fi
    local zeroCount
    zeroCount="$(grep -c 'lac __zero' "$dir/t.s")"
    if [ "$zeroCount" -ne 0 ]; then
        echo "FAIL test_leaf_no_defensive_zero: expected no 'lac __zero' (nothing in this" \
             "program ever needs a zero), found $zeroCount - the declaration's own defensive" \
             "zero-store is back"
        return 1
    fi
    echo "PASS test_leaf_no_defensive_zero (0 occurrences of 'lac __zero')"
    return 0
}

test_nonleaf_no_defensive_zero() {
    local dir="$tmpdir/nonleaf"
    mkdir -p "$dir"
    cat >"$dir/t.c" <<'EOF'
int helper() {
    return 5;
}
int main() {
    int x;
    x = helper();
    return x;
}
EOF
    if ! "$RCC" --target=pdp7 --format=asm --out="$dir/t.s" "$dir/t.c" >"$dir/log" 2>&1; then
        echo "FAIL test_nonleaf_no_defensive_zero: fixture failed to compile"
        cat "$dir/log"
        return 1
    fi
    local zeroCount
    zeroCount="$(grep -c 'lac __zero' "$dir/t.s")"
    if [ "$zeroCount" -ne 0 ]; then
        echo "FAIL test_nonleaf_no_defensive_zero: expected no 'lac __zero' (nothing in this" \
             "program ever needs a zero), found $zeroCount - the declaration's own defensive" \
             "zero-store is back"
        return 1
    fi

    if ! "$RCC" --target=pdp7 --format=rim --out="$dir/t.rim" "$dir/t.c" >"$dir/log" 2>&1; then
        echo "FAIL test_nonleaf_no_defensive_zero: rim build unexpectedly failed:"
        cat "$dir/log"
        return 1
    fi
    local simfile="$dir/run.simh" outfile="$dir/run.out"
    cat >"$simfile" <<SIMH
load $dir/t.rim 10000
go 10000
e ac
exit
SIMH
    pdp7 <"$simfile" >"$outfile" 2>/dev/null

    local acOctal
    acOctal="$(grep '^AC:' "$outfile" | awk '{print $2}')"
    if [ -z "$acOctal" ]; then
        echo "FAIL test_nonleaf_no_defensive_zero: simulator produced no AC (timed out or crashed)"
        return 1
    fi
    local acDec=$((8#$acOctal))
    if [ "$acDec" -ne 5 ]; then
        echo "FAIL test_nonleaf_no_defensive_zero: expected AC=5, got AC=$acDec"
        return 1
    fi

    echo "PASS test_nonleaf_no_defensive_zero (0 occurrences of 'lac __zero', AC=$acDec)"
    return 0
}

test_leaf_no_defensive_zero || fail=1
test_nonleaf_no_defensive_zero || fail=1

exit $fail
