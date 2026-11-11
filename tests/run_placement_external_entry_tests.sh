#!/usr/bin/env bash
# Unit tests for `@addr` fixed placement (Pdp7Assembler.cpp), pdp7 only - specifically, a placed
# function's address staying correctly relocatable regardless of how it's reached.
#
# Background: in object mode (real separate compilation, `allowExternal_=true`), the assembler's
# own `..` pseudo-symbol represents "this object's own base, not known until link time" - every
# *ordinary* address `.` accumulates by incrementing keeps the RELATIVE marker `.` itself starts
# with, so a reference to it gets a deferred link-time relocation instead of a baked-in literal.
# A fixed-placement directive (`@addr`, lowered to `. = ..+N`) computes a *new* value for `.` via
# `..` arithmetic instead of incrementing - and `..`'s own stored value used to be a plain,
# unflagged 0, which silently stripped that marker from the result. Every later reference to a
# symbol defined under a placement - its own self-returns, or another function's call into it -
# then baked in the literal placement offset (e.g. 2000) directly into the instruction word, never
# relocated, correct only by accident when the object happened to end up based at literal 0.
# Confirmed directly on the real `pdp7` simulator: before the fix, calling a placed function from
# another function in the same object (needing the same deferred "same-object" relocation every
# ordinary same-object call already gets) either baked in the wrong address outright or, under
# `--gc-sections`, silently vanished from the link entirely (the placed function's own true section
# boundary, computed from its corrupted non-relocatable value, didn't line up with where its bytes
# actually ended up). The fix: `..` itself carries the RELATIVE marker in object mode too (matching
# `.`'s own starting value, `BASE = 0 | RELATIVE`); `base()` (the public numeric accessor several
# callers - `buildObjectFile`'s own text-section walk among them - treat as a plain address) masks
# the marker back off, since `storeWord` already did everywhere else it reads `symbols_[".."]`.
#
# Two tests:
#   test_placed_function_called_normally - an `@addr`-placed function, called the ordinary way
#       from another function in the same object (not `main` itself, to exercise the same-object
#       "resolve by embedded address, not by name" relocation path `--gc-sections` depends on),
#       linked with `--gc-sections` - confirms the placed function's own section survives the link
#       and the call reaches it correctly (this exact shape silently dropped the placed function
#       before the fix).
#   test_placed_function_entered_externally - the real scenario this exists for: a SEPARATE
#       function calls the placed function via a raw `asm { jms <literal address> }` using its own
#       resolved address straight from a `--format=list` dump, with no symbolic reference at all -
#       standing in for a hand-assembled caller outside this link entirely (the real use case: a
#       low-memory hardware vector table built with `pdp-assembler`, jumping into a `--link`ed C
#       kernel at a known fixed address). Confirms the placed function's own body runs and its own
#       return correctly resumes right after that `jms`, not back inside some unrelated caller.
#       Linked WITHOUT `--gc-sections`, deliberately: nothing in this fixture's own C code ever
#       calls `placed()` by name (the raw `jms` is the only thing that reaches it, and it isn't a
#       symbolic reference `--gc-sections` can see at all) - that's the still-open, separate
#       "`--gc-sections` drops an `@addr`-placed function with no caller in the kept call graph"
#       gap (docs/known-bugs.md), not the bug this test file is about.
#
# Usage: tests/run_placement_external_entry_tests.sh [path/to/rcc]

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

test_placed_function_called_normally() {
    local dir="$tmpdir/called_normally"
    mkdir -p "$dir"
    cat >"$dir/t.c" <<'EOF'
int placed() @ 2000 {
    return 99;
}
int keep;
int caller() {
    keep = placed();
    return 0;
}
int main() {
    caller();
    return keep;
}
EOF
    if ! "$RCC" --target=pdp7 --format=coff --out="$dir/t.o" "$dir/t.c" >"$dir/log" 2>&1; then
        echo "FAIL test_placed_function_called_normally: fixture itself failed to compile"
        cat "$dir/log"
        return 1
    fi
    if ! "$RCC" --link --target=pdp7 --format=list --gc-sections --out="$dir/t.lst" "$dir/t.o" \
            >"$dir/log" 2>&1; then
        echo "FAIL test_placed_function_called_normally: --gc-sections link unexpectedly failed:"
        cat "$dir/log"
        return 1
    fi
    if ! grep -q '^FN_place[[:space:]]' "$dir/t.lst"; then
        echo "FAIL test_placed_function_called_normally: FN_place missing from the linked image" \
             "(the bug this test guards against - dropped by --gc-sections)"
        return 1
    fi

    if ! "$RCC" --link --target=pdp7 --format=rim --gc-sections --out="$dir/t.rim" "$dir/t.o" \
            >"$dir/log" 2>&1; then
        echo "FAIL test_placed_function_called_normally: --gc-sections rim link unexpectedly failed:"
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
        echo "FAIL test_placed_function_called_normally: simulator produced no AC (timed out or crashed)"
        return 1
    fi
    local acDec=$((8#$acOctal))
    if [ "$acDec" -ne 99 ]; then
        echo "FAIL test_placed_function_called_normally: expected AC=99, got AC=$acDec"
        return 1
    fi

    echo "PASS test_placed_function_called_normally (AC=$acDec)"
    return 0
}

test_placed_function_entered_externally() {
    local dir="$tmpdir/entered_externally"
    mkdir -p "$dir"
    cat >"$dir/t.c" <<'EOF'
int placed() @ 2000 {
    return 99;
}
int rawresult;
int enteredExternally() {
    asm {
        jms PLACED_ADDR_PLACEHOLDER
        dac G_rawresult
    }
    return 0;
}
int main() {
    enteredExternally();
    return rawresult;
}
EOF
    # First pass: a harmless placeholder (`0`, a plain number, not yet the real address) just to
    # discover where this fixture's own link actually puts `placed()` - its own section layout
    # (and so its final address) depends on the surrounding code, so this can't be discovered
    # from a differently-shaped probe file. Never run under the simulator. No `--gc-sections`
    # (see this function's own header above - nothing in this fixture's C code ever calls
    # `placed()` by name, so `--gc-sections` would drop it regardless of this fix).
    sed "s/PLACED_ADDR_PLACEHOLDER/0/" "$dir/t.c" >"$dir/t1.c"
    if ! "$RCC" --target=pdp7 --format=coff --out="$dir/t1.o" "$dir/t1.c" >"$dir/log" 2>&1; then
        echo "FAIL test_placed_function_entered_externally: address-discovery fixture failed to compile"
        cat "$dir/log"
        return 1
    fi
    if ! "$RCC" --link --target=pdp7 --format=list --out="$dir/t1.lst" "$dir/t1.o" \
            >"$dir/log" 2>&1; then
        echo "FAIL test_placed_function_entered_externally: address-discovery link unexpectedly failed:"
        cat "$dir/log"
        return 1
    fi
    local placedAddrOctal
    placedAddrOctal="$(awk '/^FN_place[[:space:]]/{print $2}' "$dir/t1.lst")"
    if [ -z "$placedAddrOctal" ]; then
        echo "FAIL test_placed_function_entered_externally: FN_place missing from the linked image"
        return 1
    fi

    # Second pass: the real resolved address spliced into the raw `jms` - the whole point of this
    # test (a hardcoded numeric target, no symbolic reference at all, standing in for a
    # hand-assembled external caller that can't see rcc's own symbol table).
    sed "s/PLACED_ADDR_PLACEHOLDER/0${placedAddrOctal}/" "$dir/t.c" >"$dir/t2.c"
    if ! "$RCC" --target=pdp7 --format=coff --out="$dir/t2.o" "$dir/t2.c" >"$dir/log" 2>&1; then
        echo "FAIL test_placed_function_entered_externally: fixture with the real address failed to compile"
        cat "$dir/log"
        return 1
    fi
    if ! "$RCC" --link --target=pdp7 --format=rim --out="$dir/t2.rim" "$dir/t2.o" \
            >"$dir/log" 2>&1; then
        echo "FAIL test_placed_function_entered_externally: rim link unexpectedly failed:"
        cat "$dir/log"
        return 1
    fi
    local simfile="$dir/run.simh" outfile="$dir/run.out"
    cat >"$simfile" <<SIMH
load $dir/t2.rim 10000
go 10000
e ac
exit
SIMH
    pdp7 <"$simfile" >"$outfile" 2>/dev/null

    local acOctal
    acOctal="$(grep '^AC:' "$outfile" | awk '{print $2}')"
    if [ -z "$acOctal" ]; then
        echo "FAIL test_placed_function_entered_externally: simulator produced no AC (timed out or crashed)"
        return 1
    fi
    local acDec=$((8#$acOctal))
    if [ "$acDec" -ne 99 ]; then
        echo "FAIL test_placed_function_entered_externally: expected AC=99 (the placed function's" \
             "own body ran and returned correctly to right after the raw jms), got AC=$acDec"
        return 1
    fi

    echo "PASS test_placed_function_entered_externally (AC=$acDec)"
    return 0
}

test_placed_function_called_normally || fail=1
test_placed_function_entered_externally || fail=1

exit $fail
