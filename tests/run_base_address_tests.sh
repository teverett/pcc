#!/usr/bin/env bash
# Unit tests for `--link --base=<word-addr>` (GenericLinker.cpp/Pdp7LinkerFactory.cpp), pdp7 only
# (the only target `--base` is wired up for so far).
#
# Background: every program this project links normally starts at word address 4096
# (`kPdp7BaseAddress`), the real machine's own swappable "user" region - fine for an ordinary
# whole-program link, but wrong for code meant to live in the real kernel's own always-resident
# low-memory region (word address 0 up - `.. = 0` in a real kernel source file), which a caller
# needing that distinction (e.g. keeping a disk-completion poll loop outside whatever address
# range a swappable region's own transfers might overwrite) needs linked there instead.
# `--base` makes that address configurable instead of hardcoded.
#
# Building `--base` surfaced a real, independent bug in `--gc-sections` itself (not something
# `--base` causes - see test_gc_sections_negative_shift below): `gcSections`'s own per-section
# relocation math can need a *negative* shift (when the sections before a given one shrink more
# than the base address itself, moving that section to a *lower* address than its own pre-link
# offset) - this always existed, but every prior `--gc-sections` test happened not to trigger it
# with the ordinary 4096-word base acting as an unintentional cushion large enough to keep the
# arithmetic positive. `--base=0` removes that cushion, surfacing it directly, but the fix (plain
# unsigned division no longer corrupting a negative two's-complement shift - see
# GenericLinker.cpp's own comment at the fix site) benefits any base, not just 0.
#
# Two tests:
#   test_base_runs_correctly        - a plain two-function program, linked with `--base=16` alone
#       (no `--gc-sections`), actually runs correctly under the real `pdp7` simulator, and its own
#       `FN_main` resolves near word address 16, not 4096 - confirms the flag really moves where
#       the program runs, not just that linking succeeds. Deliberately not `--base=0`: word
#       addresses 8/9 are real PDP-7 hardware autoindex registers (any *indirect* reference
#       auto-increments them first - see `../../UnixV0-C/src-c/sys/s4.c`'s own header, confirmed
#       independently here too), and this runtime's own `__addr` scratch cell (used for every
#       computed pointer dereference) sits at relative offset 9 from the entry - harmless at the
#       ordinary 4096 base (lands nowhere near 8/9), but landing exactly on hardware register 9
#       at `--base=0`, which genuinely hangs real function-call code (confirmed directly: a
#       no-call-at-all program runs fine at `--base=0`, the same shape of program with one
#       function call does not, and the same program at `--base=16` runs correctly). Not a bug
#       `--base` introduces - a real kernel link would never use literal 0 either, since the real
#       interrupt vector table/CAL-trap entry already reserves that same low range for hardware
#       reasons (see `../../UnixV0-C/src-assem/sys/s1.s`'s own `.. = 0`) - ordinary code was
#       always going to start after it, consistent with this test's own `--base=16`.
#   test_gc_sections_negative_shift - a program shaped like the real regression (several unused
#       functions before a real disk-IOT-style function using the same `asm{}` idiom
#       `sys/s4.c`'s own `dsktrans` does - bare expression statements, `asm{}` blocks, a label
#       defined and jumped to from inside one), linked with `--gc-sections --base=0` together -
#       previously failed with a bogus "address past the 8192-word limit" error from a corrupted
#       negative shift; now links cleanly, and every resolved address (`--format=list`) lands
#       within the real 8192-word address space, not a huge wrapped value that happened to clear
#       the overflow check by luck.
#
# Usage: tests/run_base_address_tests.sh [path/to/rcc]

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

test_base_runs_correctly() {
    local dir="$tmpdir/base_sixteen"
    mkdir -p "$dir"
    cat >"$dir/t.c" <<'EOF'
int helper(int x) { return x + 1; }
int main() { return helper(41); }
EOF
    if ! "$RCC" --target=pdp7 --format=coff --out="$dir/t.o" "$dir/t.c" >"$dir/log" 2>&1; then
        echo "FAIL test_base_runs_correctly: fixture itself failed to compile"
        cat "$dir/log"
        return 1
    fi
    if ! "$RCC" --link --target=pdp7 --format=list --base=16 --out="$dir/t.lst" "$dir/t.o" >"$dir/log" 2>&1; then
        echo "FAIL test_base_runs_correctly: --base=16 link unexpectedly failed:"
        cat "$dir/log"
        return 1
    fi
    local mainAddrOctal
    mainAddrOctal="$(awk '/^FN_main[[:space:]]/{print $2}' "$dir/t.lst")"
    if [ -z "$mainAddrOctal" ]; then
        echo "FAIL test_base_runs_correctly: FN_main not found in --format=list output"
        return 1
    fi
    local mainAddrDec=$((8#$mainAddrOctal))
    # Generous headroom (not a tight bound on this fixture's own exact
    # size): the real thing this guards against is falling back to the
    # ordinary 4096 base, not the shared runtime's own size growing as
    # routines are added to it (e.g. __cmp_lt, pdp7RuntimeAsm()).
    if [ "$mainAddrDec" -ge 1000 ]; then
        echo "FAIL test_base_runs_correctly: FN_main resolved to $mainAddrDec, too far from word" \
             "address 16 for a --base=16 link (still looks like the ordinary 4096 base was used)"
        return 1
    fi

    if ! "$RCC" --link --target=pdp7 --format=rim --base=16 --out="$dir/t.rim" "$dir/t.o" >"$dir/log" 2>&1; then
        echo "FAIL test_base_runs_correctly: --base=16 rim link unexpectedly failed:"
        cat "$dir/log"
        return 1
    fi
    local simfile="$dir/run.simh" outfile="$dir/run.out"
    cat >"$simfile" <<SIMH
load $dir/t.rim 20
go 20
e ac
exit
SIMH
    pdp7 <"$simfile" >"$outfile" 2>/dev/null

    local acOctal
    acOctal="$(grep '^AC:' "$outfile" | awk '{print $2}')"
    if [ -z "$acOctal" ]; then
        echo "FAIL test_base_runs_correctly: simulator produced no AC (timed out or crashed)"
        return 1
    fi
    local acDec=$((8#$acOctal))
    if [ "$acDec" -ne 42 ]; then
        echo "FAIL test_base_runs_correctly: expected AC=42, got AC=$acDec"
        return 1
    fi

    echo "PASS test_base_runs_correctly (FN_main at word $mainAddrDec, AC=$acDec)"
    return 0
}

test_gc_sections_negative_shift() {
    local dir="$tmpdir/neg_shift"
    mkdir -p "$dir"
    # The real regression this fixes, verbatim (minus comments): UnixV0-C's own
    # `src-c/sys/s4.c`, up through its own `dsktrans` - a real hardware IOT sequence using the
    # same `asm{}` idiom (a bare expression statement to load AC, raw opcode words, a label
    # defined and jumped to from inside one `asm{}` block) `sys/s1.c`'s own `swap` work actually
    # needed this fixed for. `--gc-sections` drops `betwen`/`copy`/`copyz`/`toBcd` (unreached from
    # `main`'s own call to `dsktrans` alone); with `--base=0` removing the cushion a nonzero base
    # used to provide by accident, `dsktrans`'s own new address lands *before* its own pre-link
    # offset - the exact negative-shift case the fix handles. A hand-shortened fixture (a handful
    # of trivial `unused1()`-style functions instead of `betwen`/`copy`/etc.) was tried first and
    # did NOT reproduce this - confirmed directly, not assumed - so this uses the real file's own
    # shape instead of a guessed-smaller stand-in. Not run under the simulator (`dsktrans`'s own
    # busy-poll would spin forever with no real IOT device behind it, the same genuine limitation
    # sys/s1.c's own `swap` work ran into for real) - checking the link succeeds and every
    # resolved address is sane is the right level here, same as rcc's own tests/disk.c stopping
    # at "compiles and links", not "runs".
    cat >"$dir/t.c" <<'EOF'
int betwen(int value, int low, int high) {
    return value >= low && value <= high;
}
int copy(int src, int dest, int count) {
    int i;
    for (i = 0; i < count; i = i + 1) {
        dest[i] = src[i];
    }
    return 0;
}
int copyz(int dest, int count) {
    int i;
    for (i = 0; i < count; i = i + 1) {
        dest[i] = 0;
    }
    return 0;
}
int dskbuf[64];
int dskaddr;
int dsktrans(int wordCount, int bufAddr, int bcdTrackSector, int opCode) {
    asm { 0707141 }
    wordCount;
    asm { 0707124 }
    bufAddr = bufAddr & 8191;
    bufAddr;
    asm { 0707142 }
    bcdTrackSector;
    asm { 0707104 }
    opCode;
    asm { 0707144 }
    asm {
RCCDSKWAIT:
        0707121
        jmp RCCDSKWAIT
        0707132
    }
    return 0;
}
int toBcd(int value) {
    int tens;
    int ones;
    tens = value / 10;
    ones = value % 10;
    return (tens << 4) | ones;
}
int main() {
    return dsktrans(-64, 100, 8, 3584);
}
EOF
    if ! "$RCC" --target=pdp7 --format=coff --out="$dir/t.o" "$dir/t.c" >"$dir/log" 2>&1; then
        echo "FAIL test_gc_sections_negative_shift: fixture itself failed to compile"
        cat "$dir/log"
        return 1
    fi
    if ! "$RCC" --link --target=pdp7 --format=list --gc-sections --base=0 --out="$dir/t.lst" "$dir/t.o" \
            >"$dir/log" 2>&1; then
        echo "FAIL test_gc_sections_negative_shift: --gc-sections --base=0 link unexpectedly failed" \
             "(the bug this test guards against):"
        cat "$dir/log"
        return 1
    fi

    # Every resolved address must be a real pdp7 word address (0..8191) - a corrupted negative
    # shift produces a huge wrapped value, caught here directly rather than just trusting the
    # link didn't error (the bug this guards against failed loudly, but a *different* corruption
    # might not). Plain `$((8#...))` (not awk's own `strtonum`, a gawk-only extension this
    # project can't assume) for the octal conversion, same as every other test here already uses.
    local line name octalAddr decAddr
    while IFS= read -r line; do
        name="${line%% *}"
        octalAddr="${line##* }"
        [ "$name" = "Labels:" ] && continue
        [ -z "$name" ] && continue
        decAddr=$((8#$octalAddr))
        if [ "$decAddr" -lt 0 ] || [ "$decAddr" -ge 8192 ]; then
            echo "FAIL test_gc_sections_negative_shift: '$name' resolved to $decAddr, outside the" \
                 "real pdp7 address space (0..8191)"
            return 1
        fi
    done <"$dir/t.lst"

    echo "PASS test_gc_sections_negative_shift (every resolved address within 0..8191)"
    return 0
}

test_base_runs_correctly || fail=1
test_gc_sections_negative_shift || fail=1

exit $fail
