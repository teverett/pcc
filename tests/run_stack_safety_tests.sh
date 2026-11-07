#!/usr/bin/env bash
# Unit tests for Pdp7LinkerFactory.cpp's `kStackReserve` fix.
#
# Background: the pdp7 runtime call stack (`__sp`, see
# Pdp7LinkerFactory.cpp's own entry-object header comment) is a
# hardcoded constant starting at 8191 - the very top of the pdp7's
# 8192-word address space - growing *downward* on every function
# call/argument push, with no reserved region of its own and no
# bounds check. The linker lays every object's own code/data/trailer
# out growing *upward* from a low base address, with no notion that a
# stack exists at all. A program whose own highest-addressed symbol
# ends up too close to 8191 gets silently corrupted the moment real
# call depth reaches down into it - confirmed directly against the
# real `pdp7` simulator, not just reasoned about: two otherwise byte-
# identical programs (literally the same compiled instructions for
# the one function that misbehaves), built only to leave a different
# amount of headroom below the stack's own start, with the narrow-
# headroom one hanging at runtime and the wide one not.
#
# The fix refuses to link any program whose highest-addressed symbol
# lands within `kStackReserve` (512) words of 8191, the same "refuse
# rather than silently corrupt" policy the pre-existing
# `kAddressFieldLimit` (8192-word direct-addressing field) check
# already uses - see Pdp7LinkerFactory.cpp's own header comment on
# `kStackReserve` for the full story.
#
# Two tests:
#   test_overflow_refused               - a program engineered to
#       leave almost no headroom below the stack must be REFUSED at
#       link time, with an error naming the real cause (the runtime
#       stack), not silently linked into something that would
#       misbehave at runtime.
#   test_headroom_accepted_and_runs     - an otherwise-identical
#       program, just with a smaller filler leaving comfortable
#       headroom, must still link AND run correctly under the real
#       `pdp7` simulator - the fix must not refuse a program just for
#       being nontrivially sized, only one that's actually unsafe.
#
# Usage: tests/run_stack_safety_tests.sh [path/to/pcc]

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PCC="$SCRIPT_DIR/../build/bin/pcc"
[ $# -ge 1 ] && PCC="$1"

if [ ! -x "$PCC" ]; then
    echo "pcc binary not found at $PCC - build it first ('make')" >&2
    exit 1
fi
if ! command -v pdp7 >/dev/null 2>&1; then
    echo "pdp7 (open-simh) not found on PATH - install it (e.g. 'brew install open-simh')" >&2
    exit 1
fi

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT
fail=0

# `filler`/`tailMarker` split across two files the same shape as the
# real bug: `tailMarker` (and `setTailMarker`, which writes it) are
# defined in the *second* file, after `filler` - so `tailMarker`'s
# own linked address lands right after `filler`'s, and the write to
# it from the *first* file's `main()` is a genuine cross-object
# relocation (exactly the relocation kind `kStackReserve`'s own check
# runs on - see Pdp7LinkerFactory.cpp). `filler`'s size is the only
# thing that differs between the two tests below.
write_fixture() {
    local dir="$1" fillerSize="$2"
    cat >"$dir/main.c" <<'EOF'
extern int tailMarker;
extern int setTailMarker(int v);

int main() {
    setTailMarker(7);
    return tailMarker;
}
EOF
    cat >"$dir/data.c" <<EOF
int filler[$fillerSize];
int tailMarker;

int setTailMarker(int v) {
    tailMarker = v;
    return 0;
}
EOF
}

test_overflow_refused() {
    local dir="$tmpdir/overflow"
    mkdir -p "$dir"
    # 3600 words of filler leaves well under kStackReserve (512)
    # words of headroom below 8191 once linked after main.c and the
    # shared entry object - confirmed directly (lands within the
    # danger zone, not just theoretically close).
    write_fixture "$dir" 3600

    if ! "$PCC" --target=pdp7 --format=coff --out="$dir/main.o" "$dir/main.c" >"$dir/log" 2>&1; then
        echo "FAIL test_overflow_refused: fixture itself failed to compile (bug in the test, not the fix)"
        cat "$dir/log"
        return 1
    fi
    if ! "$PCC" --target=pdp7 --format=coff --out="$dir/data.o" "$dir/data.c" >"$dir/log" 2>&1; then
        echo "FAIL test_overflow_refused: fixture itself failed to compile (bug in the test, not the fix)"
        cat "$dir/log"
        return 1
    fi

    local linkOut
    linkOut="$("$PCC" --link --target=pdp7 --format=rim --out="$dir/out.rim" "$dir/main.o" "$dir/data.o" 2>&1)"
    local linkStatus=$?

    if [ "$linkStatus" -eq 0 ]; then
        echo "FAIL test_overflow_refused: link unexpectedly SUCCEEDED - the stack-safety check didn't trigger" \
             "for a program engineered to leave almost no headroom below the runtime stack"
        return 1
    fi
    if ! echo "$linkOut" | grep -q "runtime call stack"; then
        echo "FAIL test_overflow_refused: link failed as expected, but not for the stack-safety reason" \
             "(error didn't mention the runtime call stack):"
        echo "$linkOut" | sed 's/^/    /'
        return 1
    fi

    echo "PASS test_overflow_refused (refused: $(echo "$linkOut" | head -1))"
    return 0
}

test_headroom_accepted_and_runs() {
    local dir="$tmpdir/headroom"
    mkdir -p "$dir"
    # 1000 words of filler leaves hundreds of words of headroom below
    # the stack-safety limit - the exact same program shape as
    # test_overflow_refused above, just small enough to be safe.
    write_fixture "$dir" 1000

    if ! "$PCC" --target=pdp7 --format=coff --out="$dir/main.o" "$dir/main.c" >"$dir/log" 2>&1; then
        echo "FAIL test_headroom_accepted_and_runs: compile error (main.c)"
        cat "$dir/log"
        return 1
    fi
    if ! "$PCC" --target=pdp7 --format=coff --out="$dir/data.o" "$dir/data.c" >"$dir/log" 2>&1; then
        echo "FAIL test_headroom_accepted_and_runs: compile error (data.c)"
        cat "$dir/log"
        return 1
    fi
    if ! "$PCC" --link --target=pdp7 --format=rim --out="$dir/out.rim" "$dir/main.o" "$dir/data.o" >"$dir/log" 2>&1; then
        echo "FAIL test_headroom_accepted_and_runs: link unexpectedly refused a program with real headroom:"
        cat "$dir/log"
        return 1
    fi

    local simfile="$dir/run.simh" outfile="$dir/run.out"
    cat >"$simfile" <<SIMH
load $dir/out.rim 10000
go 10000
e ac
exit
SIMH
    pdp7 <"$simfile" >"$outfile" 2>/dev/null

    local acOctal
    acOctal="$(grep '^AC:' "$outfile" | awk '{print $2}')"
    if [ -z "$acOctal" ]; then
        echo "FAIL test_headroom_accepted_and_runs: simulator produced no AC (timed out or crashed)"
        return 1
    fi
    local acDec=$((8#$acOctal))
    if [ "$acDec" -ne 7 ]; then
        echo "FAIL test_headroom_accepted_and_runs: expected AC=7, got AC=$acDec"
        return 1
    fi

    echo "PASS test_headroom_accepted_and_runs (AC=$acDec)"
    return 0
}

test_overflow_refused || fail=1
test_headroom_accepted_and_runs || fail=1

exit $fail
