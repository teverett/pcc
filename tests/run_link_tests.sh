#!/usr/bin/env bash
# Exercises real separate compilation: every tests/link/<case>_a.c /
# tests/link/<case>_b.c pair is compiled independently to a real
# relocatable object (--format=coff), combined with --link into one
# program, then run for real under the chosen target's own hardware
# simulator and checked against the "// EXPECT: <decimal>" comment on
# the "_a.c" half (the one with main) - same convention as
# run_tests.sh's own single-file loop, just for the extern/--link path
# that loop can't reach (every one of its own files is a complete,
# single-TU program - see docs/pdp-c-compiler.md's "Extern
# declarations": extern only means anything under real separate
# compilation).
#
# Usage: tests/run_link_tests.sh [path/to/pcc] [--target=pdp7|8086|8088|i386]

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LINK_DIR="$SCRIPT_DIR/link"
PCC="$SCRIPT_DIR/../build/bin/pcc"
TARGET="pdp7"

for arg in "$@"; do
    case "$arg" in
        --target=*) TARGET="${arg#--target=}" ;;
        *) PCC="$arg" ;;
    esac
done

if [ ! -x "$PCC" ]; then
    echo "pcc binary not found at $PCC - build it first ('make')" >&2
    exit 1
fi

case "$TARGET" in
    pdp7)
        if ! command -v pdp7 >/dev/null 2>&1; then
            echo "pdp7 (open-simh) not found on PATH - install it (e.g. 'brew install open-simh')" >&2
            exit 1
        fi
        ;;
    8086|8088|i386)
        if ! command -v qemu-system-i386 >/dev/null 2>&1; then
            echo "qemu-system-i386 not found on PATH - install it (e.g. 'brew install qemu')" >&2
            exit 1
        fi
        if ! command -v python3 >/dev/null 2>&1; then
            echo "python3 not found on PATH - needed to talk to qemu's monitor" >&2
            exit 1
        fi
        ;;
    *)
        echo "unknown --target '$TARGET' for run_link_tests.sh (known: pdp7, 8086, 8088, i386 - the targets this harness's own link flow is wired up for so far)" >&2
        exit 1
        ;;
esac

fail=0
tmpdir="$(mktemp -d)"
# qemu monitor sockets live in their own short directory: a unix socket
# path is limited to about 104 bytes, which `$tmpdir` plus a long case
# name can exceed.
sockdir="$(mktemp -d /tmp/pccq.XXXXXX)"
qemu_count=0
trap 'rm -rf "$tmpdir" "$sockdir"' EXIT

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

# Compiles both halves of one case to real objects and links them -
# shared by every per-target check_* below, which differ only in how
# the resulting image is actually run.
link_case() {
    local a="$1" b="$2" name="$3" fmt="$4" linkfmt="$5" outfile="$6"
    local aobj="$tmpdir/$name.a.o" bobj="$tmpdir/$name.b.o"

    if ! "$PCC" --target="$TARGET" --format="$fmt" --out="$aobj" "$a" >"$tmpdir/$name.log" 2>&1; then
        echo "FAIL $name: compile error ($a)"
        sed 's/^/    /' "$tmpdir/$name.log"
        return 1
    fi
    if ! "$PCC" --target="$TARGET" --format="$fmt" --out="$bobj" "$b" >"$tmpdir/$name.log" 2>&1; then
        echo "FAIL $name: compile error ($b)"
        sed 's/^/    /' "$tmpdir/$name.log"
        return 1
    fi
    if ! "$PCC" --link --target="$TARGET" --format="$linkfmt" --out="$outfile" "$aobj" "$bobj" >"$tmpdir/$name.log" 2>&1; then
        echo "FAIL $name: link error"
        sed 's/^/    /' "$tmpdir/$name.log"
        return 1
    fi
    return 0
}

check_pdp7() {
    local a="$1" b="$2" name="$3"
    local rimfile="$tmpdir/$name.rim"
    link_case "$a" "$b" "$name" coff rim "$rimfile" || return 1

    local simfile="$tmpdir/$name.simh" outfile="$tmpdir/$name.out"
    cat >"$simfile" <<SIMH
load $rimfile 10000
go 10000
e ac
SIMH

    pdp7 "$simfile" </dev/null >"$outfile" 2>/dev/null

    local ac_octal
    ac_octal="$(grep '^AC:' "$outfile" | awk '{print $2}')"
    if [ -z "$ac_octal" ]; then
        echo "FAIL $name: simulator produced no AC (timed out or crashed)"
        return 1
    fi

    local ac_dec
    ac_dec="$(to_signed "$((8#$ac_octal))" 18)"
    if [ "$ac_dec" -eq "$expect" ]; then
        echo "PASS $name (AC=$ac_dec)"
        return 0
    fi
    echo "FAIL $name: expected $expect, got $ac_dec (AC octal: $ac_octal)"
    return 1
}

# Shared by 8086/8088 (--format=raw bootable floppy image) and i386
# (--format=flat, still a real Multiboot image here - see
# docs/using.md's own "flat" row) - both read back the same way
# run_tests.sh's own check_8086/check_i386 do.
check_x86() {
    local a="$1" b="$2" name="$3" fmt="$4" linkfmt="$5" loadkernel="$6"
    local binfile="$tmpdir/$name.bin"
    link_case "$a" "$b" "$name" "$fmt" "$linkfmt" "$binfile" || return 1

    qemu_count=$((qemu_count + 1))
    local monitor="$sockdir/$qemu_count"
    local outfile="$tmpdir/$name.out"

    if [ "$loadkernel" = "1" ]; then
        qemu-system-i386 -kernel "$binfile" -monitor "unix:$monitor,server,nowait" \
            -display none -no-reboot >/dev/null 2>&1 &
    else
        qemu-system-i386 -fda "$binfile" -monitor "unix:$monitor,server,nowait" \
            -display none -no-reboot >/dev/null 2>&1 &
    fi
    local qemu_pid=$!
    ( sleep 45; kill -9 "$qemu_pid" 2>/dev/null ) &
    local watcher=$!

    # Returns once the program reaches its final spin loop (see qemu_wait.py);
    # an 8086 boot image's program runs in code segment 0, not the BIOS's.
    if [ "$loadkernel" = "1" ]; then
        python3 -I "$SCRIPT_DIR/qemu_wait.py" "$monitor" 40 >"$outfile"
    else
        python3 -I "$SCRIPT_DIR/qemu_wait.py" "$monitor" 40 'CS =0000 ' >"$outfile"
    fi

    kill "$qemu_pid" 2>/dev/null
    wait "$qemu_pid" 2>/dev/null
    kill "$watcher" 2>/dev/null
    wait "$watcher" 2>/dev/null

    local eax_hex
    eax_hex="$(grep -oE 'EAX=[0-9a-fA-F]+' "$outfile" | head -1 | cut -d= -f2)"
    if [ -z "$eax_hex" ]; then
        echo "FAIL $name: qemu produced no EAX (timed out or crashed)"
        return 1
    fi

    local val_dec
    if [ "$loadkernel" = "1" ]; then
        val_dec="$(to_signed "$((16#$eax_hex))" 32)"
    else
        val_dec="$(to_signed "$((16#$eax_hex & 0xFFFF))" 16)"
    fi
    if [ "$val_dec" -eq "$expect" ]; then
        echo "PASS $name (EAX=$val_dec)"
        return 0
    fi
    echo "FAIL $name: expected $expect, got $val_dec (EAX hex: $eax_hex)"
    return 1
}

# A handful of (target, case) pairs are architecturally incapable, not
# broken, the same situation run_tests.sh's own is_known_unsupported()
# covers for the single-file suite: 8086/8088's --format=raw output is
# always exactly one 512-byte boot sector (see RawFormat.cpp's
# kSectorSize), with no multi-sector loader, so any case whose linked
# code+data crosses that line fails this way on those two targets
# regardless of how compact pcc's own codegen is. stack_safety_headroom
# is sized against pdp7's own 8192-word address space, to exercise
# Pdp7LinkerFactory.cpp's pdp7-only kStackReserve margin - a concept
# with no 8086/8088 counterpart - and that sizing is what crosses the
# unrelated 512-byte sector line on those two targets. It is skipped
# there; tests/run_stack_safety_tests.sh is what actually exercises
# kStackReserve, on pdp7.
is_known_unsupported() {
    case "$TARGET:$1" in
        8086:stack_safety_headroom) return 0 ;;
        8088:stack_safety_headroom) return 0 ;;
        *) return 1 ;;
    esac
}

for a in "$LINK_DIR"/*_a.c; do
    base="$(basename "$a" _a.c)"
    b="$LINK_DIR/${base}_b.c"
    if [ ! -f "$b" ]; then
        continue
    fi
    expect="$(grep -m1 -oE 'EXPECT: *-?[0-9]+' "$a" | grep -oE '\-?[0-9]+$' || true)"
    if [ -z "$expect" ]; then
        continue
    fi
    if is_known_unsupported "$base"; then
        echo "SKIP $base (known unsupported on $TARGET)"
        continue
    fi

    case "$TARGET" in
        pdp7) check_pdp7 "$a" "$b" "$base" || fail=1 ;;
        8086) check_x86 "$a" "$b" "$base" coff raw 0 || fail=1 ;;
        8088) check_x86 "$a" "$b" "$base" coff raw 0 || fail=1 ;;
        i386) check_x86 "$a" "$b" "$base" coff flat 1 || fail=1 ;;
    esac
done

exit $fail
