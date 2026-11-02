#!/usr/bin/env bash
# Compiles every tests/*.c file that carries a "// EXPECT: <decimal>"
# comment, runs it for real under the chosen target's own hardware
# simulator, and checks the result register it halts with against
# that expected value - the same source file, same expected value,
# on every target (see ../src/Backend.h: a .c file means the same
# thing regardless of --target).
#
# Usage: tests/run_tests.sh [path/to/pcc] [--target=pdp7|8086|8088|6502|65c02|68000]

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
    65c02)
        # Same idea as 6502 above, but on py65's own separate CMOS
        # core (py65.devices.mpu65c02.MPU - see check_65c02's own
        # comment) so the one real CMOS-only instruction this backend
        # emits (STZ - see ../src/codegen/m65c02/M65C02Codegen.h's own
        # header comment) is actually exercised on real 65C02
        # semantics, not just decoded as a NOP/illegal opcode the way
        # the plain NMOS core above would.
        if ! python3 -c "import py65" >/dev/null 2>&1; then
            echo "py65 not found - install it (e.g. 'pip install py65')" >&2
            exit 1
        fi
        if ! python3 -c "from py65.devices.mpu65c02 import MPU" >/dev/null 2>&1; then
            echo "py65's 65C02 core (py65.devices.mpu65c02) not found - update py65 ('pip install -U py65')" >&2
            exit 1
        fi
        ;;
    68000)
        if ! command -v qemu-system-m68k >/dev/null 2>&1; then
            echo "qemu-system-m68k not found on PATH - install it (e.g. 'brew install qemu')" >&2
            exit 1
        fi
        if ! command -v nc >/dev/null 2>&1; then
            echo "nc not found on PATH - needed to talk to qemu's monitor" >&2
            exit 1
        fi
        ;;
    8080)
        # No real simulator here either - the 8080 backend emits
        # real, standard Intel 8080 machine code (see
        # ../src/codegen/i8080/I8080Assembler.h's own header comment),
        # and the Z80 is object-code compatible with the 8080 (Zilog
        # designed it that way), so a real independent Z80 core run
        # against this binary is just as valid a check as a real 8080
        # core would be - and a pure-Python Z80 core is what's
        # actually available on PyPI (`pip install z80-python`),
        # unlike the 8080 itself (see check_8080's own comment).
        if ! python3 -c "import z80_python" >/dev/null 2>&1; then
            echo "z80-python not found - install it (e.g. 'pip install z80-python')" >&2
            exit 1
        fi
        ;;
    z80)
        if ! python3 -c "import z80_python" >/dev/null 2>&1; then
            echo "z80-python not found - install it (e.g. 'pip install z80-python')" >&2
            exit 1
        fi
        ;;
    8008)
        # No PyPI package exists for this one (the 8008 isn't object-
        # code compatible with anything later - see
        # ../src/codegen/i8008/I8008Codegen.h's own header comment on
        # just how different its instruction set really is), so
        # check_8008() below drives a small from-scratch 8008 core
        # written just for this - standard library only, nothing to
        # install.
        ;;
    *)
        echo "unknown --target '$TARGET' (known: pdp7, 8086, 8088, 6502, 65c02, 68000, 8080, z80, 8008)" >&2
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

check_68000() {
    local src="$1" name="$2"
    local binfile="$tmpdir/$name.bin"

    pkill -f qemu-system-m68k 2>/dev/null
    sleep 1

    if ! "$PCC" --target=68000 --format=bin \
            --dump-ir --out="$binfile" "$src" >"$tmpdir/$name.log" 2>&1; then
        echo "FAIL $name: compile error"
        sed 's/^/    /' "$tmpdir/$name.log"
        return 1
    fi

    qemu_port_counter=$((qemu_port_counter + 1))
    local port=$((21000 + ($$ % 10000) + qemu_port_counter))
    local outfile="$tmpdir/$name.out"

    # `-M none -cpu m68000`: a bare CPU, no boot ROM/vector table at
    # all - the `loader` device (no `file=`, just `addr=`) sets the
    # CPU's own PC directly to where the other `loader` device (with
    # `file=`) placed the program, bypassing the real 68000 reset
    # exception's own SSP/PC-from-vector-table dance entirely (see
    # src/codegen/m68k/M68kCodegen.cpp's own `kBaseAddress` comment).
    qemu-system-m68k -M none -cpu m68000 -m 16M \
        -device "loader,file=$binfile,addr=0x1000,cpu-num=0" \
        -device "loader,addr=0x1000,cpu-num=0" \
        -monitor "tcp:127.0.0.1:$port,server,nowait" -display none -no-reboot >/dev/null 2>&1 &
    local qemu_pid=$!
    ( sleep 30; kill -9 "$qemu_pid" 2>/dev/null ) &
    local watcher=$!

    # Poll D0 until it reads the same value twice in a row (same
    # convention, and the same reason, as check_8086's own EAX poll -
    # the program's own result sits in D0 the instant it reaches its
    # `halt: bra halt` spin loop, see M68kCodegen.h's own comment).
    local waited=0 prev_d0="" cur_d0=""
    while [ "$waited" -lt 25 ]; do
        echo "info registers" | nc -w 2 127.0.0.1 "$port" >"$outfile" 2>/dev/null
        cur_d0="$(grep -oE 'D0 = [0-9a-fA-F]+' "$outfile" | head -1)"
        if [ -n "$cur_d0" ] && [ "$cur_d0" = "$prev_d0" ]; then
            break
        fi
        prev_d0="$cur_d0"
        sleep 1
        waited=$((waited + 1))
    done

    kill "$qemu_pid" 2>/dev/null
    wait "$qemu_pid" 2>/dev/null
    kill "$watcher" 2>/dev/null
    wait "$watcher" 2>/dev/null

    local d0_hex
    d0_hex="$(grep -oE 'D0 = [0-9a-fA-F]+' "$outfile" | head -1 | awk '{print $3}')"
    if [ -z "$d0_hex" ]; then
        echo "FAIL $name: qemu produced no D0 (timed out or crashed)"
        return 1
    fi

    local d0_dec=$((16#$d0_hex))
    d0_dec="$(to_signed "$d0_dec" 32)"
    if [ "$d0_dec" -eq "$expect" ]; then
        echo "PASS $name (D0=$d0_dec)"
        return 0
    fi
    echo "FAIL $name: expected $expect, got $d0_dec (D0 hex: $d0_hex)"
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

# Identical to check_6502() above (same halt-loop-self-jump
# detection, same reasoning), just --target=65c02 against py65's own
# separate CMOS core (py65.devices.mpu65c02.MPU) instead of its NMOS
# one - a real, independent check that this backend's STZ usage (see
# ../src/codegen/m65c02/M65C02Codegen.h's own header comment) decodes
# and executes exactly as a real WDC 65C02 would, not just as
# whatever py65's NMOS core happens to do with that opcode (on real
# NMOS silicon, and in py65's own mpu6502 core, $9C/$64 are illegal/
# undefined - decoding this binary there at all would already be a
# bug, since M65C02Assembler.cpp is the only assembler this backend
# ever runs its own output through).
check_65c02() {
    local src="$1" name="$2"
    local binfile="$tmpdir/$name.bin"

    if ! "$PCC" --target=65c02 --format=bin \
            --dump-ir --out="$binfile" "$src" >"$tmpdir/$name.log" 2>&1; then
        echo "FAIL $name: compile error"
        sed 's/^/    /' "$tmpdir/$name.log"
        return 1
    fi

    local result
    result="$(python3 - "$binfile" <<'PY'
import sys
from py65.devices.mpu65c02 import MPU

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

# Single-steps the assembled binary on a real, independent Z80 core
# (`z80-python`, pure Python - same single-step/detect-the-self-jump
# idea as check_6502() above, for the same reason: there's no clean
# non-interactive way to run a CLI monitor to a blocking halt loop
# and then break out of it) - valid for this backend specifically
# because it emits real, standard Intel 8080 machine code (see
# ../src/codegen/i8080/I8080Assembler.h's own header comment) and the
# Z80 is a genuine object-code-compatible superset of the 8080, so
# every opcode this backend could ever emit means exactly the same
# thing on either chip. The result sits in HL (`h`/`l`) the moment
# execution reaches `halt: jmp halt` - see
# ../src/codegen/i8080/I8080Codegen.cpp's own entry-sequence comment.
check_8080() {
    local src="$1" name="$2"
    local binfile="$tmpdir/$name.bin"

    if ! "$PCC" --target=8080 --format=bin \
            --dump-ir --out="$binfile" "$src" >"$tmpdir/$name.log" 2>&1; then
        echo "FAIL $name: compile error"
        sed 's/^/    /' "$tmpdir/$name.log"
        return 1
    fi

    local result
    result="$(python3 - "$binfile" <<'PY'
import sys
from z80_python import Z80CPU

path = sys.argv[1]
with open(path, "rb") as f:
    data = f.read()

base = 0x0100
memory = bytearray(0x10000)
memory[base:base + len(data)] = data

cpu = Z80CPU(memory.__getitem__, memory.__setitem__)
cpu.pc = base

for _ in range(2_000_000):
    pc_before = cpu.pc
    cpu.step()
    if cpu.pc == pc_before:  # "halt: jmp halt" - a JMP targeting itself
        break
else:
    print("TIMEOUT")
    sys.exit(0)

val = (cpu.h << 8) | cpu.l
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
        echo "PASS $name (hl=$result)"
        return 0
    fi
    echo "FAIL $name: expected $expect, got $result"
    return 1
}

# Same single-step/detect-the-self-jump idea as check_8080() above,
# against a real Z80 core instead of using it as an 8080 stand-in -
# this backend emits real, standard Z80 machine code (see
# ../src/codegen/z80/Z80Assembler.h's own header comment), including
# instructions (ED/CB-prefixed) the 8080 doesn't have at all, so it
# gets checked on its own rather than reusing check_8080(). The
# result sits in HL the moment execution reaches `halt: jp halt` -
# see ../src/codegen/z80/Z80Codegen.cpp's own entry-sequence comment.
check_z80() {
    local src="$1" name="$2"
    local binfile="$tmpdir/$name.bin"

    if ! "$PCC" --target=z80 --format=bin \
            --dump-ir --out="$binfile" "$src" >"$tmpdir/$name.log" 2>&1; then
        echo "FAIL $name: compile error"
        sed 's/^/    /' "$tmpdir/$name.log"
        return 1
    fi

    local result
    result="$(python3 - "$binfile" <<'PY'
import sys
from z80_python import Z80CPU

path = sys.argv[1]
with open(path, "rb") as f:
    data = f.read()

base = 0x8000
memory = bytearray(0x10000)
memory[base:base + len(data)] = data

cpu = Z80CPU(memory.__getitem__, memory.__setitem__)
cpu.pc = base

for _ in range(2_000_000):
    pc_before = cpu.pc
    cpu.step()
    if cpu.pc == pc_before:  # "halt: jp halt" - a JP targeting itself
        break
else:
    print("TIMEOUT")
    sys.exit(0)

val = (cpu.h << 8) | cpu.l
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
        echo "PASS $name (hl=$result)"
        return 0
    fi
    echo "FAIL $name: expected $expect, got $result"
    return 1
}

# No real 8008 simulator and no PyPI package exists for one (the
# 8008 isn't object-code compatible with anything later, unlike the
# 8080/Z80 pairing above - see ../src/codegen/i8008/I8008Codegen.h's
# own header comment on just how different its instruction set is) -
# so this is a small from-scratch 8008 core, standard library only,
# covering exactly the opcode subset I8008Assembler.cpp ever emits
# (see that file's own header comment on where every opcode byte came
# from). Same single-step/detect-the-self-jump idea as check_6502()/
# check_8080()/check_z80() above. The result sits in the `ac` pseudo-
# register's own fixed, known address (see
# ../src/codegen/i8008/I8008Codegen.cpp's own `kPseudoBase` comment) -
# read directly out of simulated memory, since the 8008 has no
# register wide enough to hold a 16-bit result the way every other
# backend's own halt sequence leaves one.
check_8008() {
    local src="$1" name="$2"
    local binfile="$tmpdir/$name.bin"

    if ! "$PCC" --target=8008 --format=bin \
            --dump-ir --out="$binfile" "$src" >"$tmpdir/$name.log" 2>&1; then
        echo "FAIL $name: compile error"
        sed 's/^/    /' "$tmpdir/$name.log"
        return 1
    fi

    local result
    result="$(python3 - "$binfile" <<'PY'
import sys

path = sys.argv[1]
with open(path, "rb") as f:
    data = f.read()

MEMSIZE = 0x4000  # the 8008's real 14-bit (16KB) address space
mem = bytearray(MEMSIZE)
mem[0:len(data)] = data

R = [0, 0, 0, 0, 0, 0, 0]  # a,b,c,d,e,h,l - register codes 0..6 (see
                           # I8008Assembler.cpp's own regCode() comment)
pc = 0
Z = True
C = False
stack = []


def hl():
    return (R[5] << 8) | R[6]


def get_r(code):
    return mem[hl()] if code == 7 else R[code]


def set_r(code, val):
    val &= 0xFF
    if code == 7:
        mem[hl()] = val
    else:
        R[code] = val


def set_z(result):
    global Z
    Z = (result & 0xFF) == 0


def add8(a, b, carry_in):
    global C
    total = a + b + carry_in
    C = total > 0xFF
    result = total & 0xFF
    set_z(result)
    return result


def sub8(a, b, borrow_in):
    global C
    total = a - b - borrow_in
    C = total < 0
    result = total & 0xFF
    set_z(result)
    return result


def fetch8():
    global pc
    b = mem[pc]
    pc = (pc + 1) & 0x3FFF
    return b


def fetch16():
    lo = fetch8()
    hi = fetch8()
    return (lo | (hi << 8)) & 0x3FFF


# ccc field (bits 5-3 of a conditional jump/call) this backend ever
# emits: 0=carry-clear,1=zero-clear,4=carry-set,5=zero-set - the
# other four real 8008 conditions (sign/parity) are never emitted and
# so are never recognized here.
def cond_true(ccc):
    return {0: not C, 1: not Z, 4: C, 5: Z}.get(ccc)


halted = False
for _ in range(2_000_000):
    start_pc = pc
    op = fetch8()

    if op in (0x00, 0x01, 0xFF):  # HLT (never emitted, handled defensively)
        pc = start_pc
        halted = True
        break
    elif op == 0x07:  # RET
        pc = stack.pop()
    elif op == 0x44:  # JMP
        pc = fetch16()
    elif op == 0x46:  # CAL
        target = fetch16()
        stack.append(pc)
        pc = target
    elif op in (0x02, 0x0A, 0x12, 0x1A):  # RLC/RRC/RAL/RAR
        a = R[0]
        if op == 0x02:
            C = (a & 0x80) != 0
            R[0] = ((a << 1) | (1 if C else 0)) & 0xFF
        elif op == 0x0A:
            C = (a & 1) != 0
            R[0] = ((a >> 1) | (0x80 if C else 0)) & 0xFF
        elif op == 0x12:
            newC = (a & 0x80) != 0
            R[0] = ((a << 1) | (1 if C else 0)) & 0xFF
            C = newC
        else:
            newC = (a & 1) != 0
            R[0] = ((a >> 1) | (0x80 if C else 0)) & 0xFF
            C = newC
    elif (op & 0b11000111) == 0b01000000:  # 01 ccc 000 - conditional jump
        target = fetch16()
        if cond_true((op >> 3) & 7):
            pc = target
    elif (op & 0b11000111) == 0b01000010:  # 01 ccc 010 - conditional call
        target = fetch16()
        if cond_true((op >> 3) & 7):
            stack.append(pc)
            pc = target
    elif (op >> 6) == 3:  # 11 DDD SSS - MOV
        set_r((op >> 3) & 7, get_r(op & 7))
    elif (op >> 6) == 0 and (op & 7) == 6:  # 00 DDD 110 - MVI
        set_r((op >> 3) & 7, fetch8())
    elif (op >> 6) == 2:  # 10 ALU SSS - ALU register form
        alu, val, a = (op >> 3) & 7, get_r(op & 7), R[0]
        if alu == 0:
            R[0] = add8(a, val, 0)
        elif alu == 1:
            R[0] = add8(a, val, 1 if C else 0)
        elif alu == 2:
            R[0] = sub8(a, val, 0)
        elif alu == 3:
            R[0] = sub8(a, val, 1 if C else 0)
        elif alu == 4:
            R[0] = a & val
            C = False
            set_z(R[0])
        elif alu == 5:
            R[0] = a ^ val
            C = False
            set_z(R[0])
        elif alu == 6:
            R[0] = a | val
            C = False
            set_z(R[0])
        else:
            sub8(a, val, 0)  # CMP - flags only
    elif (op >> 6) == 0 and (op & 7) == 4:  # 00 ALU 100 - ALU immediate form
        alu, imm, a = (op >> 3) & 7, fetch8(), R[0]
        if alu == 0:
            R[0] = add8(a, imm, 0)
        elif alu == 1:
            R[0] = add8(a, imm, 1 if C else 0)
        elif alu == 2:
            R[0] = sub8(a, imm, 0)
        elif alu == 3:
            R[0] = sub8(a, imm, 1 if C else 0)
        elif alu == 4:
            R[0] = a & imm
            C = False
            set_z(R[0])
        elif alu == 5:
            R[0] = a ^ imm
            C = False
            set_z(R[0])
        elif alu == 6:
            R[0] = a | imm
            C = False
            set_z(R[0])
        else:
            sub8(a, imm, 0)
    elif (op >> 6) == 0 and (op & 7) in (0, 1):  # 00 rrr 000/001 - INR/DCR (never a or m)
        rrr = (op >> 3) & 7
        if not (1 <= rrr <= 6):
            raise RuntimeError(f"unsupported opcode {op:#04x} at {start_pc:#06x}")
        delta = -1 if (op & 1) else 1
        R[rrr] = (R[rrr] + delta) & 0xFF
        set_z(R[rrr])
    else:
        raise RuntimeError(f"unsupported opcode {op:#04x} at {start_pc:#06x}")

    if pc == start_pc:
        halted = True
        break
else:
    print("TIMEOUT")
    sys.exit(0)

if not halted:
    print("TIMEOUT")
    sys.exit(0)

val = mem[0x3000] | (mem[0x3001] << 8)
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
        65c02) check_65c02 "$src" "$name" || fail=1 ;;
        68000) check_68000 "$src" "$name" || fail=1 ;;
        8080) check_8080 "$src" "$name" || fail=1 ;;
        z80) check_z80 "$src" "$name" || fail=1 ;;
        8008) check_8008 "$src" "$name" || fail=1 ;;
    esac
done

exit $fail
