#!/usr/bin/env bash
# Unit tests for Pdp7Codegen.cpp's `__cmp_lt` shared comparison
# subroutine (emitSignedLtCells()/pdp7RuntimeAsm()): when NEITHER side of
# a signed `<` has a sign known at compile time, the full dispatch-then-
# subtract sequence (~21 words) used to be inlined again at every such
# comparison site, with no sharing across call sites - the exact "rcc has
# no cross-call-site code sharing" half of docs/known-bugs.md's former
# comparison-lowering entry. Now that case alone (the cheaper known-sign
# cases stay inlined, see run_pdp7_compare_constant_tests.sh) is routed
# through one `__cmp_lt` subroutine, emitted once in the trailer and
# called via `jms` from every such site instead.
#
# Two tests:
#   test_subroutine_shared_across_functions - two different functions
#       each doing an unknown-sign `<` comparison must both `jms
#       __cmp_lt`, and the `__cmp_lt:` label itself must appear exactly
#       once (the shared body, not a copy per call site).
#   test_subroutine_omitted_when_unused - a program with no unknown-sign
#       comparison at all must not emit `__cmp_lt` - it's only pulled
#       into the trailer when something actually needs it (same
#       conditional-emission convention as `__push`/`__pop`/`__divmod`).
#
# Usage: tests/run_pdp7_shared_compare_subroutine_tests.sh [path/to/rcc]

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

test_subroutine_shared_across_functions() {
    local dir="$tmpdir/shared"
    mkdir -p "$dir"
    cat >"$dir/t.c" <<'EOF'
int lt1(int a, int b) { return a < b; }
int lt2(int a, int b) { return a < b; }
int main() {
    return lt1(3, 7) + lt2(-5, -1);
}
EOF
    if ! "$RCC" --target=pdp7 --format=asm --out="$dir/t.s" "$dir/t.c" >"$dir/log" 2>&1; then
        echo "FAIL test_subroutine_shared_across_functions: fixture failed to compile"
        cat "$dir/log"
        return 1
    fi
    local jmsCount defCount
    jmsCount="$(grep -c 'jms __cmp_lt' "$dir/t.s")"
    defCount="$(grep -c '^__cmp_lt:' "$dir/t.s")"
    if [ "$jmsCount" -ne 2 ]; then
        echo "FAIL test_subroutine_shared_across_functions: expected 2 'jms __cmp_lt' calls" \
             "(one per function), found $jmsCount"
        return 1
    fi
    if [ "$defCount" -ne 1 ]; then
        echo "FAIL test_subroutine_shared_across_functions: expected exactly 1 '__cmp_lt:'" \
             "definition (one shared body), found $defCount"
        return 1
    fi

    if ! "$RCC" --target=pdp7 --format=rim --out="$dir/t.rim" "$dir/t.c" >"$dir/log" 2>&1; then
        echo "FAIL test_subroutine_shared_across_functions: rim build unexpectedly failed:"
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
        echo "FAIL test_subroutine_shared_across_functions: simulator produced no AC (timed out" \
             "or crashed)"
        return 1
    fi
    local acDec=$((8#$acOctal))
    if [ "$acDec" -ne 2 ]; then
        echo "FAIL test_subroutine_shared_across_functions: expected AC=2 (both comparisons" \
             "true: 3<7 and -5<-1), got AC=$acDec"
        return 1
    fi

    echo "PASS test_subroutine_shared_across_functions (1 shared '__cmp_lt:', 2 call sites, AC=$acDec)"
    return 0
}

test_subroutine_omitted_when_unused() {
    local dir="$tmpdir/unused"
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
        echo "FAIL test_subroutine_omitted_when_unused: fixture failed to compile"
        cat "$dir/log"
        return 1
    fi
    local cmpLtCount
    cmpLtCount="$(grep -c '__cmp_lt' "$dir/t.s")"
    if [ "$cmpLtCount" -ne 0 ]; then
        echo "FAIL test_subroutine_omitted_when_unused: expected no '__cmp_lt' at all (this" \
             "comparison's constant side already resolves one sign at compile time), found" \
             "$cmpLtCount occurrence(s)"
        return 1
    fi
    echo "PASS test_subroutine_omitted_when_unused (no '__cmp_lt' emitted)"
    return 0
}

test_subroutine_shared_across_functions || fail=1
test_subroutine_omitted_when_unused || fail=1

exit $fail
