#!/usr/bin/env bash
# Unit tests for `--link --gc-sections` (GenericLinker.cpp), pdp7 only (the only target with an
# AddrFieldExtractFn registered so far - see Pdp7LinkerFactory.cpp).
#
# Background: without this flag, `--link` always links every input object in full, even when a
# test driver only calls a handful of a large file's own functions - the exact wall documented in
# UnixV0-C's own src-c/README.md ("sysloc"/"pget_test"/etc. rows): several real kernel-port test
# drivers can't be linked at all because the pdp7's 8192-word direct-addressing limit is smaller
# than "every function in every file the driver touches, whether that driver actually calls it or
# not". `--gc-sections` instead carves each object's own `.text` into one section per top-level
# function/global (delimited by its own `G_`/`FN_`-prefixed label - see GenericLinker.cpp's own
# `sectionsOf`) and keeps only the sections transitively reachable from the entry point's own call
# graph, dropping the rest.
#
# The subtle correctness risk this guards against: a same-file address reference (calling another
# function in the same file, or reading/writing a plain global) is assembled as a raw local offset
# plus a generic "this object's own base" relocation (pseudo-symbol ".."), with the real target's
# *name* never preserved past assembly - unlike a genuine cross-file `extern` reference, which
# keeps its name as a real linker symbol. Naively patching every ".." relocation using the
# relocation *site's* own section (instead of figuring out which section the reference's real
# target landed in) would silently miscompute the address whenever gc-sections drops some
# unrelated section physically between the reference and its target, shifting them apart by a
# different amount than before. GenericLinker.cpp's own `targetOf`/AddrFieldExtractFn machinery
# re-derives the real target from the embedded raw offset instead of assuming same-section, which
# is exactly what test_cross_function_shared_global_survives_gap below exercises.
#
# Three tests:
#   test_dead_function_dropped_and_still_correct     - a file with one used and several unused
#       functions still runs correctly under the real pdp7 simulator with --gc-sections, AND the
#       linked output is strictly smaller than the same link without the flag - proving real
#       elimination happened, not just a no-op flag.
#   test_cross_function_shared_global_survives_gap   - two functions in the same file both
#       reference a third, unrelated same-file global by address, with an unused function
#       physically between them in the source (so it sits in its own section, between theirs, once
#       gc-sections carves the file up) - dropping it must not corrupt either function's own
#       access to the shared global once the gap closes.
#   test_address_taken_not_called_is_kept            - a function whose address is only taken
#       (stored in a global, via a real cross-file `extern`, the same pattern sys/sysloc.c's own
#       `locsw[]` dispatch table uses) and never directly called must still be kept, not dropped -
#       gc-sections has to treat "address taken" as a real use, the same way the entry point's own
#       `jms FN_main` is.
#
# Usage: tests/run_gc_sections_tests.sh [path/to/rcc]

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

run_and_read_ac() {
    local rim="$1" outfile="$2"
    local simfile="${rim%.rim}.simh"
    cat >"$simfile" <<SIMH
load $rim 10000
go 10000
e ac
exit
SIMH
    pdp7 <"$simfile" >"$outfile" 2>/dev/null
}

test_dead_function_dropped_and_still_correct() {
    local dir="$tmpdir/dead_fn"
    mkdir -p "$dir"
    cat >"$dir/a.c" <<'EOF'
int used(int x) { return x + 1; }
int main() {
    return used(41);
}
EOF
    cat >"$dir/b.c" <<'EOF'
int unused1(int x) { return x * 2; }
int unused2(int x) { return x * 3; }
int unused3(int x) { return x * 4; }
int unused4(int x) { return x * 5; }
EOF
    if ! "$RCC" --target=pdp7 --format=coff --out="$dir/a.o" "$dir/a.c" >"$dir/log" 2>&1 ||
       ! "$RCC" --target=pdp7 --format=coff --out="$dir/b.o" "$dir/b.c" >>"$dir/log" 2>&1; then
        echo "FAIL test_dead_function_dropped_and_still_correct: fixture itself failed to compile"
        cat "$dir/log"
        return 1
    fi
    if ! "$RCC" --link --target=pdp7 --format=rim --out="$dir/nogc.rim" "$dir/a.o" "$dir/b.o" >"$dir/log" 2>&1; then
        echo "FAIL test_dead_function_dropped_and_still_correct: plain (non-gc) link unexpectedly failed:"
        cat "$dir/log"
        return 1
    fi
    if ! "$RCC" --link --target=pdp7 --format=rim --gc-sections --out="$dir/gc.rim" "$dir/a.o" "$dir/b.o" \
            >"$dir/log" 2>&1; then
        echo "FAIL test_dead_function_dropped_and_still_correct: --gc-sections link unexpectedly failed:"
        cat "$dir/log"
        return 1
    fi

    local nogcSize gcSize
    nogcSize="$(wc -c <"$dir/nogc.rim" | tr -d ' ')"
    gcSize="$(wc -c <"$dir/gc.rim" | tr -d ' ')"
    if [ "$gcSize" -ge "$nogcSize" ]; then
        echo "FAIL test_dead_function_dropped_and_still_correct: --gc-sections output ($gcSize bytes) isn't" \
             "smaller than the plain link ($nogcSize bytes) - the four unused functions weren't dropped"
        return 1
    fi

    run_and_read_ac "$dir/gc.rim" "$dir/run.out"
    local acOctal
    acOctal="$(grep '^AC:' "$dir/run.out" | awk '{print $2}')"
    if [ -z "$acOctal" ]; then
        echo "FAIL test_dead_function_dropped_and_still_correct: simulator produced no AC (timed out or crashed)"
        return 1
    fi
    local acDec=$((8#$acOctal))
    if [ "$acDec" -ne 42 ]; then
        echo "FAIL test_dead_function_dropped_and_still_correct: expected AC=42, got AC=$acDec"
        return 1
    fi

    echo "PASS test_dead_function_dropped_and_still_correct (AC=$acDec, $gcSize < $nogcSize bytes)"
    return 0
}

test_cross_function_shared_global_survives_gap() {
    local dir="$tmpdir/shared_global"
    mkdir -p "$dir"
    # `unused_mid` sits physically between `setIt` and `getIt` in the source, so it gets its own
    # section between theirs once gc-sections carves this file up; both `setIt` and `getIt`
    # reference `shared` by name, resolved locally at assembly time (same file) as a raw ".."
    # relocation, not a named one - exactly the case GenericLinker.cpp's own `targetOf` has to get
    # right once `unused_mid`'s own section is dropped and the gap between `setIt` and `getIt`
    # closes.
    cat >"$dir/c.c" <<'EOF'
int shared;
int setIt() { shared = 7; return shared; }
int unused_mid(int x) { return x * 2; }
int getIt() { return shared; }
int main() {
    setIt();
    return getIt();
}
EOF
    if ! "$RCC" --target=pdp7 --format=coff --out="$dir/c.o" "$dir/c.c" >"$dir/log" 2>&1; then
        echo "FAIL test_cross_function_shared_global_survives_gap: fixture itself failed to compile"
        cat "$dir/log"
        return 1
    fi
    if ! "$RCC" --link --target=pdp7 --format=rim --gc-sections --out="$dir/gc.rim" "$dir/c.o" \
            >"$dir/log" 2>&1; then
        echo "FAIL test_cross_function_shared_global_survives_gap: --gc-sections link unexpectedly failed:"
        cat "$dir/log"
        return 1
    fi

    run_and_read_ac "$dir/gc.rim" "$dir/run.out"
    local acOctal
    acOctal="$(grep '^AC:' "$dir/run.out" | awk '{print $2}')"
    if [ -z "$acOctal" ]; then
        echo "FAIL test_cross_function_shared_global_survives_gap: simulator produced no AC (timed out or crashed)"
        return 1
    fi
    local acDec=$((8#$acOctal))
    if [ "$acDec" -ne 7 ]; then
        echo "FAIL test_cross_function_shared_global_survives_gap: expected AC=7, got AC=$acDec" \
             "(shared global's address was miscomputed once the dropped function's gap closed)"
        return 1
    fi

    echo "PASS test_cross_function_shared_global_survives_gap (AC=$acDec)"
    return 0
}

test_address_taken_not_called_is_kept() {
    local dir="$tmpdir/addr_taken"
    mkdir -p "$dir"
    # `never_called` is never invoked anywhere - its only use is its own address stored into
    # `dispatch` via a plain bare function name (rcc's own address-of-function convention), the
    # same pattern sys/sysloc.c's own `locsw[]` dispatch table uses for functions like `namei`
    # that sysloc itself never calls directly either.
    cat >"$dir/main.c" <<'EOF'
extern int dispatch[1];
int getDispatch() { return dispatch[0]; }
int main() {
    return getDispatch();
}
EOF
    cat >"$dir/funcs.c" <<'EOF'
int never_called() { return 99; }
int dispatch[1] = {never_called};
EOF
    if ! "$RCC" --target=pdp7 --format=coff --out="$dir/main.o" "$dir/main.c" >"$dir/log" 2>&1 ||
       ! "$RCC" --target=pdp7 --format=coff --out="$dir/funcs.o" "$dir/funcs.c" >>"$dir/log" 2>&1; then
        echo "FAIL test_address_taken_not_called_is_kept: fixture itself failed to compile"
        cat "$dir/log"
        return 1
    fi
    if ! "$RCC" --link --target=pdp7 --format=rim --gc-sections --out="$dir/gc.rim" "$dir/main.o" "$dir/funcs.o" \
            >"$dir/log" 2>&1; then
        echo "FAIL test_address_taken_not_called_is_kept: --gc-sections link unexpectedly failed:"
        cat "$dir/log"
        return 1
    fi

    run_and_read_ac "$dir/gc.rim" "$dir/run.out"
    local acOctal
    acOctal="$(grep '^AC:' "$dir/run.out" | awk '{print $2}')"
    if [ -z "$acOctal" ]; then
        echo "FAIL test_address_taken_not_called_is_kept: simulator produced no AC (timed out or crashed)"
        return 1
    fi
    local acDec=$((8#$acOctal))
    if [ "$acDec" -eq 0 ]; then
        echo "FAIL test_address_taken_not_called_is_kept: AC=0 - 'dispatch' resolved to a null/missing" \
             "address, meaning never_called's own section was wrongly dropped"
        return 1
    fi

    echo "PASS test_address_taken_not_called_is_kept (AC=$acDec, never_called's own address was kept)"
    return 0
}

test_dead_function_dropped_and_still_correct || fail=1
test_cross_function_shared_global_survives_gap || fail=1
test_address_taken_not_called_is_kept || fail=1

exit $fail
