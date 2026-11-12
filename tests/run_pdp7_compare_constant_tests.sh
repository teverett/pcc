#!/usr/bin/env bash
# Unit tests for Pdp7Codegen.cpp's same-sign-subtract fix in
# emitSignedLtCells()/genCompareCells(): when the comparison's "B" side
# is itself a compile-time constant, its negation is now a precomputed
# literal cell (the same "add a negative literal directly" idiom
# narrowAC() etc. already use elsewhere in this file), instead of
# negating B's cell at runtime (`lac B`/`cma`/`tad __one`/`dac`) every
# time the comparison executes - see docs/known-bugs.md's former
# "pattern 2" entry (`i < 10` in a `for` loop).
#
# Two tests:
#   test_constant_rhs_skips_runtime_negation - a comparison against a
#       literal constant (`i < 10`, `i` of unknown sign) must contain no
#       `cma` at all - the only one the old lowering needed was for
#       negating that literal at runtime, which this fix now does at
#       compile time instead.
#   test_variable_rhs_still_negates_at_runtime - a comparison between two
#       plain variables (neither a compile-time constant) must still use
#       the original runtime negation (`cma` present) - confirms the fix
#       is additive, not a correctness change to the general case it
#       doesn't apply to.
#
# Usage: tests/run_pdp7_compare_constant_tests.sh [path/to/rcc]

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

test_constant_rhs_skips_runtime_negation() {
    local dir="$tmpdir/const_rhs"
    mkdir -p "$dir"
    cat >"$dir/t.c" <<'EOF'
int main() {
    int i;
    int count;
    count = 0;
    for (i = 0; i < 10; i++) {
        count = count + 1;
    }
    return count;
}
EOF
    if ! "$RCC" --target=pdp7 --format=asm --optimize --out="$dir/t.s" "$dir/t.c" >"$dir/log" 2>&1; then
        echo "FAIL test_constant_rhs_skips_runtime_negation: fixture failed to compile"
        cat "$dir/log"
        return 1
    fi
    local cmaCount
    cmaCount="$(grep -c 'cma' "$dir/t.s")"
    if [ "$cmaCount" -ne 0 ]; then
        echo "FAIL test_constant_rhs_skips_runtime_negation: expected no 'cma' (the literal's" \
             "negation should be precomputed), found $cmaCount occurrence(s)"
        return 1
    fi
    echo "PASS test_constant_rhs_skips_runtime_negation (0 occurrences of 'cma')"
    return 0
}

test_variable_rhs_still_negates_at_runtime() {
    local dir="$tmpdir/var_rhs"
    mkdir -p "$dir"
    cat >"$dir/t.c" <<'EOF'
int lt(int a, int b) {
    return a < b;
}
int main() {
    return lt(3, 7);
}
EOF
    if ! "$RCC" --target=pdp7 --format=asm --optimize --out="$dir/t.s" "$dir/t.c" >"$dir/log" 2>&1; then
        echo "FAIL test_variable_rhs_still_negates_at_runtime: fixture failed to compile"
        cat "$dir/log"
        return 1
    fi
    local cmaCount
    cmaCount="$(grep -c 'cma' "$dir/t.s")"
    if [ "$cmaCount" -eq 0 ]; then
        echo "FAIL test_variable_rhs_still_negates_at_runtime: expected the original runtime" \
             "negation ('cma') since neither operand is a compile-time constant, found none"
        return 1
    fi
    echo "PASS test_variable_rhs_still_negates_at_runtime ($cmaCount occurrence(s) of 'cma')"
    return 0
}

test_constant_rhs_skips_runtime_negation || fail=1
test_variable_rhs_still_negates_at_runtime || fail=1

exit $fail
