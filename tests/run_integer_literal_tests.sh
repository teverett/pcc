#!/usr/bin/env bash
# Unit tests for the integer-literal-parsing fix (src/ast/BuildAst.cpp), target-agnostic
# (literal parsing happens before any backend sees the program).
#
# Background: a decimal literal's digits were parsed with `std::stol` - signed 64-bit, range
# [-2^63, 2^63-1] - so any literal at or above 2^63 (e.g. `18446744065119617025ULL`, legal C:
# it fits in 64 bits unsigned) threw an unhandled `std::out_of_range` instead of compiling.
# The fix parses with `std::stoull` (unsigned 64-bit, range [0, 2^64-1]) and reinterprets the
# bits as `Expr::intValue`'s own 64-bit storage - the same wraparound C itself defines for an
# unsigned type - and now reports a real too-large-for-64-bits literal (more than 2^64-1)
# as an ordinary compile error instead of an unhandled exception.
#
# tests/unsigned_literal_above_long_max.c (run across every target via run_tests.sh's own
# "// EXPECT:" convention) covers the main fix - a literal between 2^63 and 2^64-1 now parses
# and round-trips correctly. The two tests here cover the frontend's own error handling,
# which that convention can't express:
#   test_oversized_literal_rejected_cleanly - a literal beyond 2^64-1 (genuinely
#       unrepresentable in any width this compiler supports) must fail with an ordinary
#       compile error naming the literal, not crash with an unhandled C++ exception.
#   test_max_uint64_literal_accepted        - 2^64-1 itself (UINT64_MAX, the largest value
#       that *is* representable) must still compile successfully - confirms the fix didn't
#       just move the rejection threshold rather than removing it.
#
# Usage: tests/run_integer_literal_tests.sh [path/to/rcc]

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RCC="$SCRIPT_DIR/../build/bin/rcc"
[ $# -ge 1 ] && RCC="$1"

if [ ! -x "$RCC" ]; then
    echo "rcc binary not found at $RCC - build it first ('make')" >&2
    exit 1
fi

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT
fail=0

test_oversized_literal_rejected_cleanly() {
    local dir="$tmpdir/oversized"
    mkdir -p "$dir"
    cat >"$dir/t.c" <<'EOF'
int main() {
    unsigned long long x;
    x = 99999999999999999999ULL;
    return 0;
}
EOF
    local log="$dir/log"
    if "$RCC" --target=pdp7 --format=rim --out="$dir/t.rim" "$dir/t.c" >"$log" 2>&1; then
        echo "FAIL test_oversized_literal_rejected_cleanly: compile unexpectedly succeeded"
        return 1
    fi
    if ! grep -q "too large to represent in 64 bits" "$log"; then
        echo "FAIL test_oversized_literal_rejected_cleanly: expected a clean 'too large'" \
             "compile error, got:"
        sed 's/^/    /' "$log"
        return 1
    fi
    echo "PASS test_oversized_literal_rejected_cleanly"
    return 0
}

test_max_uint64_literal_accepted() {
    local dir="$tmpdir/maxu64"
    mkdir -p "$dir"
    cat >"$dir/t.c" <<'EOF'
int main() {
    unsigned long long x;
    x = 18446744073709551615ULL;
    if (x == 18446744073709551615ULL) return 1;
    return 0;
}
EOF
    local log="$dir/log"
    if ! "$RCC" --target=pdp7 --format=rim --out="$dir/t.rim" "$dir/t.c" >"$log" 2>&1; then
        echo "FAIL test_max_uint64_literal_accepted: compile unexpectedly failed:"
        sed 's/^/    /' "$log"
        return 1
    fi

    if ! command -v pdp7 >/dev/null 2>&1; then
        echo "PASS test_max_uint64_literal_accepted (compile-only - pdp7 simulator not on" \
             "PATH to verify the runtime value)"
        return 0
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
        echo "FAIL test_max_uint64_literal_accepted: simulator produced no AC (timed out or crashed)"
        return 1
    fi
    local acDec=$((8#$acOctal))
    if [ "$acDec" -ne 1 ]; then
        echo "FAIL test_max_uint64_literal_accepted: expected AC=1, got AC=$acDec"
        return 1
    fi
    echo "PASS test_max_uint64_literal_accepted (AC=$acDec)"
    return 0
}

test_oversized_literal_rejected_cleanly || fail=1
test_max_uint64_literal_accepted || fail=1

exit $fail
