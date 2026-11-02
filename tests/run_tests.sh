#!/usr/bin/env bash
# Compiles every tests/*.c file that carries a "// EXPECT: <decimal>"
# comment, runs it for real under the chosen target's own hardware
# simulator, and checks the result register it halts with against
# that expected value - the same source file, same expected value,
# on every target (see ../src/Backend.h: a .c file means the same
# thing regardless of --target).
#
# Usage: tests/run_tests.sh [path/to/pcc] [--target=pdp7|8086|8088|6502]

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
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
    8086|8088)
        if ! command -v qemu-system-i386 >/dev/null 2>&1; then
            echo "qemu-system-i386 not found on PATH - install it (e.g. 'brew install qemu')" >&2
            exit 1
        fi
        if ! command -v nc >/dev/null 2>&1; then
            echo "nc not found on PATH - needed to talk to qemu's monitor" >&2
            exit 1
        fi
        ;;
    6502)
        # No real simulator here (py65's own CLI monitor can't easily
        # be driven non-interactively - see check_6502's own comment)
        # - just python3 + the py65 package (`pip install py65`),
        # scripted directly via its MPU class, same idea as
        # ../src/codegen/m6502/M6502Codegen.h's own from-scratch
        # verification but using a real, independent 6502 core
        # instead of this project's own throwaway one.
        if ! python3 -c "import py65" >/dev/null 2>&1; then
            echo "py65 not found - install it (e.g. 'pip install py65')" >&2
            exit 1
        fi
        ;;
    *)
        echo "unknown --target '$TARGET' (known: pdp7, 8086, 8088, 6502)" >&2
        exit 1
        ;;
esac

fail=0
qemu_port_counter=0
tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

# Runs "$@" with a portable bash-only timeout: `timeout`/`gtimeout`
# aren't guaranteed to exist (notably, GitHub's macOS runners have
# neither on PATH by default), so a background process plus a
# sleep-then-kill watcher stands in for it instead.
#
# This isn't just a portability nicety for `pdp7` specifically: wrapping
# it directly in the real `timeout` command (confirmed on macOS,
# open-simh 3.12-3) makes a disk-touching run hang indefinitely at 0%
# CPU - genuinely blocked, not spinning - that a plain backgrounded
# `pdp7 ... &` of the exact same script does not. Root cause not
# identified (likely `timeout`'s own process-group/signal handling
# confusing SIMH's console/terminal setup); workaround confirmed
# reliable, investigated no further. Any manual verification outside
# this script (see `../../src-c/README.md`'s own disk-attached checks)
# should use this same background-plus-watchdog shape, never `timeout
# pdp7 ...` directly, or an apparent hang may just be this.
run_with_timeout() {
    local secs="$1"
    shift
    "$@" &
    local pid=$!
    (sleep "$secs" && kill -9 "$pid" 2>/dev/null) &
    local watcher=$!
    wait "$pid" 2>/dev/null
    local status=$?
    kill "$watcher" 2>/dev/null
    wait "$watcher" 2>/dev/null
    return $status
}

# Reports a 32-bit unsigned value as signed, matching README's
# two's-complement convention (used for both targets' result
# registers - 18-bit AC on pdp7, 16-bit AX read out of EAX on 8086).
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

check_pdp7() {
    local src="$1" name="$2"
    local rimfile="$tmpdir/$name.rim"

    if ! "$PCC" --target=pdp7 --format=rim \
            --dump-ir --out="$rimfile" "$src" >"$tmpdir/$name.log" 2>&1; then
        echo "FAIL $name: compile error"
        sed 's/^/    /' "$tmpdir/$name.log"
        return 1
    fi

    local simfile="$tmpdir/$name.simh" outfile="$tmpdir/$name.out"
    cat >"$simfile" <<SIMH
load $rimfile 10000
go 10000
e ac
SIMH

    run_with_timeout 10 bash -c "pdp7 <'$simfile' >'$outfile' 2>/dev/null"

    local ac_octal
    ac_octal="$(grep '^AC:' "$outfile" | awk '{print $2}')"
    if [ -z "$ac_octal" ]; then
        echo "FAIL $name: simulator produced no AC (timed out or crashed)"
        return 1
    fi

    local ac_dec=$((8#$ac_octal))
    ac_dec="$(to_signed "$ac_dec" 18)"
    if [ "$ac_dec" -eq "$expect" ]; then
        echo "PASS $name (AC=$ac_dec)"
        return 0
    fi
    echo "FAIL $name: expected $expect, got $ac_dec (AC octal: $ac_octal)"
    return 1
}

check_8086() {
    # `target`: "8086" or "8088" - a software-identical alias for it
    # (see src/main.cpp's own makeBackend() comment) - this just
    # exercises both all the way through a real boot, even though the
    # generated bytes are already confirmed identical either way.
    local target="$1" src="$2" name="$3"
    local binfile="$tmpdir/$name.bin"

    # Clean slate: an earlier test's qemu instance lingering (even
    # briefly) has caused cross-test TCP mixups here before.
    pkill -f qemu-system-i386 2>/dev/null
    sleep 1

    if ! "$PCC" --target="$target" --format=bin \
            --dump-ir --out="$binfile" "$src" >"$tmpdir/$name.log" 2>&1; then
        echo "FAIL $name: compile error"
        sed 's/^/    /' "$tmpdir/$name.log"
        return 1
    fi

    # A fresh port per test: reusing one risks talking to a still-
    # lingering previous qemu instance (TCP teardown isn't instant).
    qemu_port_counter=$((qemu_port_counter + 1))
    local port=$((20000 + ($$ % 10000) + qemu_port_counter))
    local outfile="$tmpdir/$name.out"

    qemu-system-i386 -fda "$binfile" -monitor "tcp:127.0.0.1:$port,server,nowait" \
        -display none -no-reboot >/dev/null 2>&1 &
    local qemu_pid=$!
    ( sleep 30; kill -9 "$qemu_pid" 2>/dev/null ) &
    local watcher=$!

    # Poll for HLT=1 instead of a single fixed sleep: a flat delay
    # tuned on a fast dev machine isn't enough on a slower/unaccelerated
    # CI runner (qemu-system-i386 there has no HVF/KVM, so BIOS POST +
    # program runtime both take much longer wall-clock time) - seen in
    # practice as mix.c/neg.c reading back EAX=0 (the CPU's power-on
    # value, i.e. the query raced ahead of the machine actually
    # halting) on GitHub's macOS runners while the same tests pass
    # locally. Up to ~25s total before giving up and taking whatever
    # the last query returned (the existing "no EAX" / wrong-value
    # checks below still catch a genuine failure).
    # Only "info registers" here, deliberately no "quit" - "quit" is
    # an HMP command that shuts the whole qemu process down, not just
    # the nc connection, so sending it on every poll iteration would
    # kill qemu after the very first one and strand every later
    # iteration with nothing to connect to. The explicit kill below,
    # once polling is done, is what actually ends this qemu instance.
    #
    # Poll until EAX reads the same value twice in a row, rather than
    # waiting for HLT=1: a background timer interrupt wakes the CPU
    # back out of `hlt` into the `jmp _hang` spin loop within a
    # couple of seconds (see ../docs/pdp-c-compiler.md's "The 8086
    # backend"), so HLT can easily have already flipped back to 0 by
    # the time of the very first poll - waiting for it can spin for
    # the entire timeout for no reason. EAX stops changing either way
    # once main has returned, which a fixed sleep tuned on a fast dev
    # machine isn't guaranteed to outlast on a slower/unaccelerated CI
    # runner (seen in practice as mix.c/neg.c reading back EAX=0, the
    # CPU's power-on value, on GitHub's macOS runners).
    local waited=0 prev_eax="" cur_eax=""
    while [ "$waited" -lt 25 ]; do
        echo "info registers" | nc -w 2 127.0.0.1 "$port" >"$outfile" 2>/dev/null
        cur_eax="$(grep -oE 'EAX=[0-9a-fA-F]+' "$outfile" | head -1)"
        if [ -n "$cur_eax" ] && [ "$cur_eax" = "$prev_eax" ]; then
            break
        fi
        prev_eax="$cur_eax"
        sleep 1
        waited=$((waited + 1))
    done

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

    local ax_dec=$((16#$eax_hex & 0xFFFF))  # only AX (the low word) is meaningful here
    ax_dec="$(to_signed "$ax_dec" 16)"
    if [ "$ax_dec" -eq "$expect" ]; then
        echo "PASS $name (AX=$ax_dec)"
        return 0
    fi
    echo "FAIL $name: expected $expect, got $ax_dec (EAX hex: $eax_hex)"
    return 1
}

# py65's own CLI monitor is built for an interactive session (load,
# then `.goto` - which blocks forever on this backend's own infinite
# halt loop, with no clean non-interactive way to break back out and
# read registers - see ../src/codegen/m6502/M6502Codegen.h's own
# header comment on that halt convention), so this scripts py65's
# MPU class directly instead: step one instruction at a time and
# detect the halt loop itself (a JMP whose target is its own
# address) rather than relying on any wall-clock timeout.
check_6502() {
    local src="$1" name="$2"
    local binfile="$tmpdir/$name.bin"

    if ! "$PCC" --target=6502 --format=bin \
            --dump-ir --out="$binfile" "$src" >"$tmpdir/$name.log" 2>&1; then
        echo "FAIL $name: compile error"
        sed 's/^/    /' "$tmpdir/$name.log"
        return 1
    fi

    local result
    result="$(python3 - "$binfile" <<'PY'
import sys
from py65.devices.mpu6502 import MPU

path = sys.argv[1]
with open(path, "rb") as f:
    data = f.read()

base = 0x0600
mpu = MPU()
mpu.memory[base:base + len(data)] = data
mpu.pc = base

for _ in range(2_000_000):
    pc_before = mpu.pc
    mpu.step()
    if mpu.pc == pc_before:  # "halt: jmp halt" - a JMP targeting itself
        break
else:
    print("TIMEOUT")
    sys.exit(0)

lo = mpu.memory[2]
hi = mpu.memory[3]
val = lo | (hi << 8)
if val >= 0x8000:
    val -= 0x10000
print(val)
PY
    )"
    if [ "$result" = "TIMEOUT" ]; then
        echo "FAIL $name: simulator never reached the halt loop (step limit exceeded)"
        return 1
    fi
    if [ "$result" -eq "$expect" ] 2>/dev/null; then
        echo "PASS $name (ac=$result)"
        return 0
    fi
    echo "FAIL $name: expected $expect, got $result"
    return 1
}

for src in "$SCRIPT_DIR"/*.c; do
    expect="$(grep -m1 -oE 'EXPECT: *-?[0-9]+' "$src" | grep -oE '\-?[0-9]+$' || true)"
    if [ -z "$expect" ]; then
        continue
    fi
    name="$(basename "$src")"

    case "$TARGET" in
        pdp7) check_pdp7 "$src" "$name" || fail=1 ;;
        8086) check_8086 8086 "$src" "$name" || fail=1 ;;
        8088) check_8086 8088 "$src" "$name" || fail=1 ;;
        6502) check_6502 "$src" "$name" || fail=1 ;;
    esac
done

exit $fail
