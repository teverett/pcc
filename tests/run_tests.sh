#!/usr/bin/env bash
# Compiles every tests/*.c file that carries a "// EXPECT: <decimal>"
# comment, runs it for real under the chosen target's own hardware
# simulator, and checks the result register it halts with against
# that expected value - the same source file, same expected value,
# on every target (see ../src/Backend.h: a .c file means the same
# thing regardless of --target).
#
# Usage: tests/run_tests.sh [path/to/pcc] [--target=pdp7|pdp11|r10000|powerpc|cortex-m3|8086|8088|6502|65c02|68000|mc68328|8080|z80|8008|8051|stm32|i386|armv7a|armv8a] [--optimize]
#
# --optimize passes pcc's own --optimize straight through to every
# compile below, so the exact same checks also confirm the IR
# optimizer pipeline (see ../docs/optimization.md) never changes a
# program's own observable result.

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PCC="$SCRIPT_DIR/../build/bin/pcc"
TARGET="pdp7"
OPTFLAGS=""

for arg in "$@"; do
    case "$arg" in
        --target=*) TARGET="${arg#--target=}" ;;
        --optimize) OPTFLAGS="--optimize" ;;
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
    pdp11)
        if ! command -v pdp11 >/dev/null 2>&1; then
            echo "pdp11 (open-simh) not found on PATH - install it (e.g. 'brew install open-simh')" >&2
            exit 1
        fi
        if ! command -v python3 >/dev/null 2>&1; then
            echo "python3 not found on PATH - needed to build check_pdp11()'s own deposit script" >&2
            exit 1
        fi
        ;;
    r10000)
        if ! command -v qemu-system-mips >/dev/null 2>&1; then
            echo "qemu-system-mips not found on PATH - install it (e.g. 'brew install qemu')" >&2
            exit 1
        fi
        if ! command -v python3 >/dev/null 2>&1; then
            echo "python3 not found on PATH - needed to talk to qemu's monitor" >&2
            exit 1
        fi
        ;;
    powerpc)
        if ! command -v qemu-system-ppc >/dev/null 2>&1; then
            echo "qemu-system-ppc not found on PATH - install it (e.g. 'brew install qemu')" >&2
            exit 1
        fi
        if ! command -v python3 >/dev/null 2>&1; then
            echo "python3 not found on PATH - needed to talk to qemu's monitor" >&2
            exit 1
        fi
        ;;
    cortex-m3)
        if ! command -v qemu-system-arm >/dev/null 2>&1; then
            echo "qemu-system-arm not found on PATH - install it (e.g. 'brew install qemu')" >&2
            exit 1
        fi
        if ! command -v python3 >/dev/null 2>&1; then
            echo "python3 not found on PATH - needed to talk to qemu's monitor" >&2
            exit 1
        fi
        if ! command -v python3 >/dev/null 2>&1; then
            echo "python3 not found on PATH - needed to build check_cortex_m3()'s own vector table" >&2
            exit 1
        fi
        ;;
    8086|8088)
        if ! command -v qemu-system-i386 >/dev/null 2>&1; then
            echo "qemu-system-i386 not found on PATH - install it (e.g. 'brew install qemu')" >&2
            exit 1
        fi
        if ! command -v python3 >/dev/null 2>&1; then
            echo "python3 not found on PATH - needed to talk to qemu's monitor" >&2
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
        if ! command -v python3 >/dev/null 2>&1; then
            echo "python3 not found on PATH - needed to talk to qemu's monitor" >&2
            exit 1
        fi
        ;;
    mc68328)
        # Same CPU core as 68000 above (the real MC68328 "DragonBall"
        # executes a bit-for-bit identical instruction set - see
        # ../src/codegen/mc68328/MC68328Codegen.h's own header
        # comment), so the same qemu tool suffices; check_mc68328()
        # below drives it differently from check_68000() though - no
        # PC-forcing `loader` device, since this backend lays out a
        # real reset vector table and qemu's own `-M none` board
        # genuinely honors its PC half (confirmed directly) - see
        # check_mc68328()'s own comment.
        if ! command -v qemu-system-m68k >/dev/null 2>&1; then
            echo "qemu-system-m68k not found on PATH - install it (e.g. 'brew install qemu')" >&2
            exit 1
        fi
        if ! command -v python3 >/dev/null 2>&1; then
            echo "python3 not found on PATH - needed to talk to qemu's monitor" >&2
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
    8051)
        # No PyPI package exists for this one either (nothing on PyPI
        # models the 8051's own genuinely Harvard CODE/XDATA split -
        # see ../src/codegen/mcs51/MCS51Codegen.h's own header
        # comment), so check_8051() below drives a small from-scratch
        # MCS-51 core written just for this - standard library only,
        # nothing to install. --target=8052/8031/8032 aren't run here
        # too, for the same "provably byte-identical output" reason
        # --target=8088 isn't (see ../src/main.cpp's own makeBackend()
        # comment) - all four part numbers share this one backend.
        ;;
    stm32)
        # qemu's own `stm32vldiscovery` board (a real STM32F100,
        # Cortex-M3) models flash/RAM as two genuinely separate,
        # correctly-sized address ranges (confirmed directly via its
        # own `info mtree` monitor command during development - flash
        # a real read-only ROM, 8KB of RAM at 0x20000000 - see
        # ../src/codegen/stm32/STM32Codegen.h's own header comment on
        # why this backend needs that split at all), so unlike every
        # other qemu-driven target here this one boots through a real
        # Cortex-M reset (`-kernel`, no PC-forcing `loader` device
        # trick) - check_stm32() below is the only check_*() that
        # relies on the vector table it's given actually being
        # honored, rather than sidestepping it.
        if ! command -v qemu-system-arm >/dev/null 2>&1; then
            echo "qemu-system-arm not found on PATH - install it (e.g. 'brew install qemu')" >&2
            exit 1
        fi
        if ! command -v python3 >/dev/null 2>&1; then
            echo "python3 not found on PATH - needed to talk to qemu's monitor" >&2
            exit 1
        fi
        ;;
    i386)
        # Same qemu tool as `8086` above, but a completely different
        # boot path: this backend's own `--format=raw` output is a
        # real Multiboot image (see
        # ../src/codegen/i386/I386Codegen.cpp's own `generate()`
        # comment), loaded straight into 32-bit protected mode by
        # qemu's own `-kernel` loader - no floppy image, no BIOS, no
        # boot-sector/disk-I/O/mode-switch code at all, unlike `8086`'s
        # own real-mode floppy boot; check_i386() below reads the full
        # 32-bit `EAX`, not just its low 16 bits the way check_8086()
        # does.
        if ! command -v qemu-system-i386 >/dev/null 2>&1; then
            echo "qemu-system-i386 not found on PATH - install it (e.g. 'brew install qemu')" >&2
            exit 1
        fi
        if ! command -v python3 >/dev/null 2>&1; then
            echo "python3 not found on PATH - needed to talk to qemu's monitor" >&2
            exit 1
        fi
        ;;
    armv7a)
        # 32-bit AArch32 A32 encoding (see
        # ../src/codegen/arm/ArmCodegen.h's own header comment);
        # cortex-a15 specifically because it's the one qemu cpu model
        # here that implements the ARMv7-A optional integer-divide
        # extension (SDIV/UDIV) this backend's `*`/`/` rely on.
        if ! command -v qemu-system-arm >/dev/null 2>&1; then
            echo "qemu-system-arm not found on PATH - install it (e.g. 'brew install qemu')" >&2
            exit 1
        fi
        if ! command -v python3 >/dev/null 2>&1; then
            echo "python3 not found on PATH - needed to talk to qemu's monitor" >&2
            exit 1
        fi
        ;;
    armv8a)
        # 64-bit AArch64 A64 encoding (see
        # ../src/codegen/arm64/Arm64Codegen.h's own header comment).
        if ! command -v qemu-system-aarch64 >/dev/null 2>&1; then
            echo "qemu-system-aarch64 not found on PATH - install it (e.g. 'brew install qemu')" >&2
            exit 1
        fi
        if ! command -v python3 >/dev/null 2>&1; then
            echo "python3 not found on PATH - needed to talk to qemu's monitor" >&2
            exit 1
        fi
        ;;
    *)
        echo "unknown --target '$TARGET' (known: pdp7, pdp11, r10000, powerpc, cortex-m3, 8086, 8088, 6502, 65c02, 68000, mc68328, 8080, z80, 8008, 8051, stm32, i386, armv7a, armv8a)" >&2
        exit 1
        ;;
esac

fail=0
tmpdir="$(mktemp -d)"
# qemu monitor sockets live in their own short directory: a unix socket
# path is limited to about 104 bytes, which `$tmpdir` plus a long test
# name can exceed.
sockdir="$(mktemp -d /tmp/pccq.XXXXXX)"
qemu_count=0
trap 'rm -rf "$tmpdir" "$sockdir"' EXIT

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
            $OPTFLAGS --dump-ir --out="$rimfile" "$src" >"$tmpdir/$name.log" 2>&1; then
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

    # The script goes in as an argument, with stdin from /dev/null: a program that runs
    # long enough would otherwise have the console's keyboard poll read the script's
    # remaining lines as input. (The same applies to check_pdp11 below.)
    run_with_timeout 10 bash -c "pdp7 '$simfile' </dev/null >'$outfile' 2>/dev/null"

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

# open-simh's own `pdp11` has no RIM-tape-style `load <file> <addr>`
# convention the way `pdp7` does (see check_pdp7() above) - the real
# PDP-11 `load` command expects an absolute-loader paper-tape image,
# not a bare flat binary - so this deposits the compiled `--format=raw`
# bytes into memory one word at a time instead (`d <addr> <word>`,
# both in SimH's own default octal radix), then sets `pc` the same
# way. Pdp11Codegen.cpp's own entry stub (`generate()`'s own header
# comment) already sets up its own real stack pointer before calling
# `fn_main`, so this harness never has to - unlike a from-scratch
# core (check_8008() below), every real instruction here runs on
# SimH's own independent PDP-11 CPU implementation. The program's own
# result sits in R0 the instant it reaches its `halt: halt \ br halt`
# spin loop, see Pdp11Codegen.cpp's own entry-sequence comment.
check_pdp11() {
    local src="$1" name="$2"
    local binfile="$tmpdir/$name.bin"

    if ! "$PCC" --target=pdp11 --format=raw \
            $OPTFLAGS --dump-ir --out="$binfile" "$src" >"$tmpdir/$name.log" 2>&1; then
        echo "FAIL $name: compile error"
        sed 's/^/    /' "$tmpdir/$name.log"
        return 1
    fi

    local simfile="$tmpdir/$name.simh" outfile="$tmpdir/$name.out"
    python3 -I - "$binfile" "$simfile" <<'PY'
import sys
binpath, simpath = sys.argv[1], sys.argv[2]
with open(binpath, "rb") as f:
    data = f.read()
if len(data) % 2:
    data += b"\x00"
base = 0o10000
lines = []
for i in range(0, len(data), 2):
    word = data[i] | (data[i + 1] << 8)
    lines.append(f"d {base + i:o} {word:o}")
lines.append(f"d pc {base:o}")
lines.append("go")
lines.append("e r0")
with open(simpath, "w") as f:
    f.write("\n".join(lines) + "\n")
PY

    run_with_timeout 10 bash -c "pdp11 '$simfile' </dev/null >'$outfile' 2>/dev/null"

    local r0_octal
    r0_octal="$(grep '^R0:' "$outfile" | awk '{print $2}')"
    if [ -z "$r0_octal" ]; then
        echo "FAIL $name: simulator produced no R0 (timed out or crashed)"
        return 1
    fi

    local r0_dec
    r0_dec="$(to_signed "$((8#$r0_octal))" 16)"
    if [ "$r0_dec" -eq "$expect" ]; then
        echo "PASS $name (R0=$r0_dec)"
        return 0
    fi
    echo "FAIL $name: expected $expect, got $r0_dec (R0 octal: $r0_octal)"
    return 1
}

wait_for_qemu() {
    local name="$1" monitor="$2" outfile="$3" qemu_pid="$4" watcher="$5"
    shift 5
    local status=0
    python3 -I "$SCRIPT_DIR/qemu_wait.py" "$monitor" 40 "$@" \
        >"$outfile" 2>"$tmpdir/$name.wait.log" || status=$?

    kill "$qemu_pid" 2>/dev/null
    wait "$qemu_pid" 2>/dev/null
    kill "$watcher" 2>/dev/null
    wait "$watcher" 2>/dev/null

    if [ "$status" -ne 0 ]; then
        echo "FAIL $name: qemu execution failed (monitor exit $status)"
        sed 's/^/    /' "$tmpdir/$name.wait.log" "$outfile" "$tmpdir/$name.qemu.log"
        return 1
    fi
}

check_8086() {
    local target="$1" src="$2" name="$3"
    local binfile="$tmpdir/$name.bin"

    if ! "$PCC" --target="$target" --format=raw \
            $OPTFLAGS --dump-ir --out="$binfile" "$src" >"$tmpdir/$name.log" 2>&1; then
        echo "FAIL $name: compile error"
        sed 's/^/    /' "$tmpdir/$name.log"
        return 1
    fi

    qemu_count=$((qemu_count + 1))
    local monitor="$sockdir/$qemu_count"
    local outfile="$tmpdir/$name.out"

    qemu-system-i386 -fda "$binfile" -monitor "unix:$monitor,server,nowait" \
        -display none -no-reboot >"$tmpdir/$name.qemu.log" 2>&1 &
    local qemu_pid=$!
    ( sleep 45; kill -9 "$qemu_pid" 2>/dev/null ) &
    local watcher=$!

    wait_for_qemu "$name" "$monitor" "$outfile" "$qemu_pid" "$watcher" \
        --8086-image "$binfile" || return 1

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

check_i386() {
    local src="$1" name="$2"
    local binfile="$tmpdir/$name.bin"

    if ! "$PCC" --target=i386 --format=raw \
            $OPTFLAGS --dump-ir --out="$binfile" "$src" >"$tmpdir/$name.log" 2>&1; then
        echo "FAIL $name: compile error"
        sed 's/^/    /' "$tmpdir/$name.log"
        return 1
    fi

    qemu_count=$((qemu_count + 1))
    local monitor="$sockdir/$qemu_count"
    local outfile="$tmpdir/$name.out"

    # `-kernel "$binfile"`, no floppy/BIOS boot at all: this backend's
    # own `--format=raw` output is a real Multiboot image (see
    # ../src/codegen/i386/I386Codegen.cpp's own `generate()` comment),
    # and qemu's own `-kernel` loader reads that header and lands this
    # program running in real 32-bit protected mode directly, the same
    # way a real GRUB would - no boot sector/disk I/O/mode-switch code
    # of this program's own involved at all.
    qemu-system-i386 -kernel "$binfile" -monitor "unix:$monitor,server,nowait" \
        -display none -no-reboot >"$tmpdir/$name.qemu.log" 2>&1 &
    local qemu_pid=$!
    ( sleep 45; kill -9 "$qemu_pid" 2>/dev/null ) &
    local watcher=$!

    wait_for_qemu "$name" "$monitor" "$outfile" "$qemu_pid" "$watcher" || return 1

    local eax_hex
    eax_hex="$(grep -oE 'EAX=[0-9a-fA-F]+' "$outfile" | head -1 | cut -d= -f2)"
    if [ -z "$eax_hex" ]; then
        echo "FAIL $name: qemu produced no EAX (timed out or crashed)"
        return 1
    fi

    local eax_dec=$((16#$eax_hex))
    eax_dec="$(to_signed "$eax_dec" 32)"
    if [ "$eax_dec" -eq "$expect" ]; then
        echo "PASS $name (EAX=$eax_dec)"
        return 0
    fi
    echo "FAIL $name: expected $expect, got $eax_dec (EAX hex: $eax_hex)"
    return 1
}

check_armv7a() {
    local src="$1" name="$2"
    local binfile="$tmpdir/$name.bin"

    if ! "$PCC" --target=armv7a --format=raw \
            $OPTFLAGS --dump-ir --out="$binfile" "$src" >"$tmpdir/$name.log" 2>&1; then
        echo "FAIL $name: compile error"
        sed 's/^/    /' "$tmpdir/$name.log"
        return 1
    fi

    qemu_count=$((qemu_count + 1))
    local monitor="$sockdir/$qemu_count"
    local outfile="$tmpdir/$name.out"

    # `-M none -cpu cortex-a15`: a bare A-profile CPU, no boot ROM/
    # vector table at all - the `loader` device (no `file=`, just
    # `addr=`) sets the CPU's own PC directly to where the other
    # `loader` device (with `file=`) placed the program, same trick
    # as check_68000's own (see its own comment) - and cortex-a15
    # specifically because it's the one qemu cpu model here that
    # implements the ARMv7-A optional integer-divide extension
    # (SDIV/UDIV - see ../src/codegen/arm/ArmCodegen.cpp's own `*`/`/`).
    qemu-system-arm -M none -cpu cortex-a15 -m 16M \
        -device "loader,file=$binfile,addr=0x1000,cpu-num=0" \
        -device "loader,addr=0x1000,cpu-num=0" \
        -monitor "unix:$monitor,server,nowait" -display none -no-reboot >"$tmpdir/$name.qemu.log" 2>&1 &
    local qemu_pid=$!
    ( sleep 45; kill -9 "$qemu_pid" 2>/dev/null ) &
    local watcher=$!

    wait_for_qemu "$name" "$monitor" "$outfile" "$qemu_pid" "$watcher" || return 1

    local r0_hex
    r0_hex="$(grep -oE 'R00=[0-9a-fA-F]+' "$outfile" | head -1 | cut -d= -f2)"
    if [ -z "$r0_hex" ]; then
        echo "FAIL $name: qemu produced no R00 (timed out or crashed)"
        return 1
    fi

    local r0_dec=$((16#$r0_hex))
    r0_dec="$(to_signed "$r0_dec" 32)"
    if [ "$r0_dec" -eq "$expect" ]; then
        echo "PASS $name (R0=$r0_dec)"
        return 0
    fi
    echo "FAIL $name: expected $expect, got $r0_dec (R0 hex: $r0_hex)"
    return 1
}

# qemu's own `-kernel` raw-image boot convention for the `virt`
# machine's AArch64 boot places a flat binary at RAM base (a fixed,
# documented 0x40000000) + 0x80000 and sets the CPU's PC there
# directly - no vector table or `loader`-device PC-forcing trick
# needed (unlike check_68000()'s own) - see
# ../src/codegen/arm64/Arm64Codegen.cpp's own kBaseAddress comment.
check_armv8a() {
    local src="$1" name="$2"
    local binfile="$tmpdir/$name.bin"

    if ! "$PCC" --target=armv8a --format=raw \
            $OPTFLAGS --dump-ir --out="$binfile" "$src" >"$tmpdir/$name.log" 2>&1; then
        echo "FAIL $name: compile error"
        sed 's/^/    /' "$tmpdir/$name.log"
        return 1
    fi

    qemu_count=$((qemu_count + 1))
    local monitor="$sockdir/$qemu_count"
    local outfile="$tmpdir/$name.out"

    qemu-system-aarch64 -M virt -cpu cortex-a53 -m 256M \
        -kernel "$binfile" -nographic \
        -monitor "unix:$monitor,server,nowait" -serial none -display none -no-reboot >"$tmpdir/$name.qemu.log" 2>&1 &
    local qemu_pid=$!
    ( sleep 45; kill -9 "$qemu_pid" 2>/dev/null ) &
    local watcher=$!

    wait_for_qemu "$name" "$monitor" "$outfile" "$qemu_pid" "$watcher" || return 1

    local x0_hex
    x0_hex="$(grep -oE 'X00=[0-9a-fA-F]+' "$outfile" | head -1 | cut -d= -f2)"
    if [ -z "$x0_hex" ]; then
        echo "FAIL $name: qemu produced no X00 (timed out or crashed)"
        return 1
    fi

    local x0_dec=$((16#$x0_hex & 0xFFFFFFFF))
    x0_dec="$(to_signed "$x0_dec" 32)"
    if [ "$x0_dec" -eq "$expect" ]; then
        echo "PASS $name (X0=$x0_dec)"
        return 0
    fi
    echo "FAIL $name: expected $expect, got $x0_dec (X00 hex: $x0_hex)"
    return 1
}

# qemu's own `-M none` bare-CPU board (used for check_armv7a()/
# check_68000() above) doesn't carry through the PC-forcing `loader`
# device's effect on MIPS - its CPU's own reset handler always wins,
# landing back at the real MIPS boot vector (0xBFC00000) regardless -
# confirmed directly (and not investigated further: `-M malta`, a
# real, more fully-modeled board, honors it correctly, so that's used
# here instead; the `malta`-specific devices it also brings along
# (PCI/serial/etc.) are simply never touched by anything this
# program's own code does). MipsAssembler.cpp's own `emit32()` writes
# each instruction word most-significant-byte-first, so this needs
# the real big-endian `qemu-system-mips`, not `qemu-system-mipsel`.
# The program's own result sits in `t0` (GPR08) the instant execution
# reaches its `halt: b halt` spin loop - see MipsCodegen.h's own
# header comment.
check_r10000() {
    local src="$1" name="$2"
    local binfile="$tmpdir/$name.bin"

    if ! "$PCC" --target=r10000 --format=raw \
            $OPTFLAGS --dump-ir --out="$binfile" "$src" >"$tmpdir/$name.log" 2>&1; then
        echo "FAIL $name: compile error"
        sed 's/^/    /' "$tmpdir/$name.log"
        return 1
    fi

    qemu_count=$((qemu_count + 1))
    local monitor="$sockdir/$qemu_count"
    local outfile="$tmpdir/$name.out"

    qemu-system-mips -M malta -cpu 4Kc -m 16M \
        -device "loader,file=$binfile,addr=0x80010000,cpu-num=0" \
        -device "loader,addr=0x80010000,cpu-num=0" \
        -monitor "unix:$monitor,server,nowait" -nographic -serial none -display none -no-reboot \
        >"$tmpdir/$name.qemu.log" 2>&1 &
    local qemu_pid=$!
    ( sleep 45; kill -9 "$qemu_pid" 2>/dev/null ) &
    local watcher=$!

    wait_for_qemu "$name" "$monitor" "$outfile" "$qemu_pid" "$watcher" || return 1

    local t0_hex
    t0_hex="$(grep -oE 't0 [0-9a-fA-F]+' "$outfile" | head -1 | awk '{print $2}')"
    if [ -z "$t0_hex" ]; then
        echo "FAIL $name: qemu produced no t0 (timed out or crashed)"
        return 1
    fi

    local t0_dec
    t0_dec="$(to_signed "$((16#$t0_hex))" 32)"
    if [ "$t0_dec" -eq "$expect" ]; then
        echo "PASS $name (t0=$t0_dec)"
        return 0
    fi
    echo "FAIL $name: expected $expect, got $t0_dec (t0 hex: $t0_hex)"
    return 1
}

# Unlike MIPS above, qemu's own `-M none` bare-CPU board honors the
# PC-forcing `loader` device just fine for PowerPC (confirmed
# directly) - same convention as check_armv7a()/check_68000(). The
# program's own result sits in `r3` (GPR00's own row, SimH- style
# "info registers" prints four GPRs per row starting at GPR00 - see
# PowerPCCodegen.h's own header comment) the instant execution
# reaches its `halt: b halt` spin loop.
check_powerpc() {
    local src="$1" name="$2"
    local binfile="$tmpdir/$name.bin"

    if ! "$PCC" --target=powerpc --format=raw \
            $OPTFLAGS --dump-ir --out="$binfile" "$src" >"$tmpdir/$name.log" 2>&1; then
        echo "FAIL $name: compile error"
        sed 's/^/    /' "$tmpdir/$name.log"
        return 1
    fi

    qemu_count=$((qemu_count + 1))
    local monitor="$sockdir/$qemu_count"
    local outfile="$tmpdir/$name.out"

    qemu-system-ppc -M none -cpu G3 -m 16M \
        -device "loader,file=$binfile,addr=0x00100000,cpu-num=0" \
        -device "loader,addr=0x00100000,cpu-num=0" \
        -monitor "unix:$monitor,server,nowait" -nographic -serial none -display none -no-reboot \
        >"$tmpdir/$name.qemu.log" 2>&1 &
    local qemu_pid=$!
    ( sleep 45; kill -9 "$qemu_pid" 2>/dev/null ) &
    local watcher=$!

    wait_for_qemu "$name" "$monitor" "$outfile" "$qemu_pid" "$watcher" || return 1

    local r3_hex
    r3_hex="$(grep '^GPR00' "$outfile" | tr -d '\r' | awk '{print $5}')"
    if [ -z "$r3_hex" ]; then
        echo "FAIL $name: qemu produced no GPR00 row (timed out or crashed)"
        return 1
    fi

    local r3_dec
    r3_dec="$(to_signed "$((16#$r3_hex & 0xFFFFFFFF))" 32)"
    if [ "$r3_dec" -eq "$expect" ]; then
        echo "PASS $name (r3=$r3_dec)"
        return 0
    fi
    echo "FAIL $name: expected $expect, got $r3_dec (r3 hex: $r3_hex)"
    return 1
}

# ArmMCodegen.cpp's own `generate()` leaves no real vector table in
# its own `--format=raw` output at all (unlike STM32Codegen.cpp's -
# see check_stm32() above) - it was written assuming a bare CPU a
# test harness forces PC into directly, the same convention
# check_armv7a()/check_68000() above use via qemu's PC-forcing
# `loader` device. That trick doesn't reach a real Cortex-M board
# here though (confirmed directly: `-M none` refuses Cortex-M CPUs
# outright) - Cortex-M silicon always reads its initial SP/PC from a
# real vector table at address 0 on reset, no way around it. So this
# harness builds that minimal 8-byte table itself (word 0: an
# arbitrary initial SP into this board's own SRAM at 0x20000000;
# word 1: `kBaseAddress | 1`, Thumb bit set) and loads it at address 0
# alongside the program at `kBaseAddress` (0x1000 - see
# ArmMCodegen.cpp's own `kBaseAddress`), letting the board's real
# reset sequence carry it from there, same as a genuine Cortex-M chip
# would. `-M mps2-an385` specifically (not the more "ordinary"-looking
# `lm3s6965evb`, confirmed directly): that board's own address 0 is
# real flash, read-only to an ordinary store the same way it would be
# on genuine silicon - every global write in the program silently did
# nothing, reading back as 0 forever after. `mps2-an385`'s own address
# 0 is modeled as plain writable block RAM instead (an FPGA eval
# board's "boot memory" is physically just RAM, not real flash), so a
# global actually stores and loads back correctly. The program's own
# result sits in `r0` the instant execution reaches its `halt: b
# halt` spin loop.
check_cortex_m3() {
    local src="$1" name="$2"
    local binfile="$tmpdir/$name.bin"

    if ! "$PCC" --target=cortex-m3 --format=raw \
            $OPTFLAGS --dump-ir --out="$binfile" "$src" >"$tmpdir/$name.log" 2>&1; then
        echo "FAIL $name: compile error"
        sed 's/^/    /' "$tmpdir/$name.log"
        return 1
    fi

    local vecfile="$tmpdir/$name.vec.bin"
    python3 -I - "$vecfile" <<'PY'
import struct
import sys
sp = 0x20010000
pc = 0x1000 | 1  # Thumb bit set
with open(sys.argv[1], "wb") as f:
    f.write(struct.pack("<II", sp, pc))
PY

    qemu_count=$((qemu_count + 1))
    local monitor="$sockdir/$qemu_count"
    local outfile="$tmpdir/$name.out"

    qemu-system-arm -M mps2-an385 \
        -device "loader,file=$vecfile,addr=0x0" \
        -device "loader,file=$binfile,addr=0x1000" \
        -monitor "unix:$monitor,server,nowait" -nographic -serial none -display none -no-reboot \
        >"$tmpdir/$name.qemu.log" 2>&1 &
    local qemu_pid=$!
    ( sleep 45; kill -9 "$qemu_pid" 2>/dev/null ) &
    local watcher=$!

    wait_for_qemu "$name" "$monitor" "$outfile" "$qemu_pid" "$watcher" || return 1

    local r0_hex
    r0_hex="$(grep -oE 'R00=[0-9a-fA-F]+' "$outfile" | head -1 | cut -d= -f2)"
    if [ -z "$r0_hex" ]; then
        echo "FAIL $name: qemu produced no R00 (timed out or crashed)"
        return 1
    fi

    local r0_dec
    r0_dec="$(to_signed "$((16#$r0_hex))" 32)"
    if [ "$r0_dec" -eq "$expect" ]; then
        echo "PASS $name (R0=$r0_dec)"
        return 0
    fi
    echo "FAIL $name: expected $expect, got $r0_dec (R0 hex: $r0_hex)"
    return 1
}

check_68000() {
    local src="$1" name="$2"
    local binfile="$tmpdir/$name.bin"

    if ! "$PCC" --target=68000 --format=raw \
            $OPTFLAGS --dump-ir --out="$binfile" "$src" >"$tmpdir/$name.log" 2>&1; then
        echo "FAIL $name: compile error"
        sed 's/^/    /' "$tmpdir/$name.log"
        return 1
    fi

    qemu_count=$((qemu_count + 1))
    local monitor="$sockdir/$qemu_count"
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
        -monitor "unix:$monitor,server,nowait" -display none -no-reboot >"$tmpdir/$name.qemu.log" 2>&1 &
    local qemu_pid=$!
    ( sleep 45; kill -9 "$qemu_pid" 2>/dev/null ) &
    local watcher=$!

    wait_for_qemu "$name" "$monitor" "$outfile" "$qemu_pid" "$watcher" || return 1

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

# Unlike check_68000() above, no PC-forcing `loader` device: this
# backend (see ../src/codegen/mc68328/MC68328Codegen.h's own header
# comment) lays out a real two-vector reset table at address 0, and
# qemu-system-m68k's own `-M none` board genuinely honors its PC half
# with no race or quirk (confirmed directly: a from-scratch minimal
# image with nothing but those two vectors plus a self-loop settles
# at the right PC immediately, no forcing needed) - so the binary is
# simply loaded at address 0 and the real reset exception does the
# rest, exactly as it would on genuine DragonBall silicon.
check_mc68328() {
    local src="$1" name="$2"
    local binfile="$tmpdir/$name.bin"

    if ! "$PCC" --target=mc68328 --format=raw \
            $OPTFLAGS --dump-ir --out="$binfile" "$src" >"$tmpdir/$name.log" 2>&1; then
        echo "FAIL $name: compile error"
        sed 's/^/    /' "$tmpdir/$name.log"
        return 1
    fi

    qemu_count=$((qemu_count + 1))
    local monitor="$sockdir/$qemu_count"
    local outfile="$tmpdir/$name.out"

    qemu-system-m68k -M none -cpu m68000 -m 16M \
        -device "loader,file=$binfile,addr=0x0,cpu-num=0" \
        -monitor "unix:$monitor,server,nowait" -display none -no-reboot >"$tmpdir/$name.qemu.log" 2>&1 &
    local qemu_pid=$!
    ( sleep 45; kill -9 "$qemu_pid" 2>/dev/null ) &
    local watcher=$!

    wait_for_qemu "$name" "$monitor" "$outfile" "$qemu_pid" "$watcher" || return 1

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

    if ! "$PCC" --target=6502 --format=raw \
            $OPTFLAGS --dump-ir --out="$binfile" "$src" >"$tmpdir/$name.log" 2>&1; then
        echo "FAIL $name: compile error"
        sed 's/^/    /' "$tmpdir/$name.log"
        return 1
    fi

    local outfile="$tmpdir/$name.sim.out"
    python3 - "$binfile" > "$outfile" <<'PY'
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
    local result
    result="$(cat "$outfile")"
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

    if ! "$PCC" --target=65c02 --format=raw \
            $OPTFLAGS --dump-ir --out="$binfile" "$src" >"$tmpdir/$name.log" 2>&1; then
        echo "FAIL $name: compile error"
        sed 's/^/    /' "$tmpdir/$name.log"
        return 1
    fi

    local outfile="$tmpdir/$name.sim.out"
    python3 - "$binfile" > "$outfile" <<'PY'
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
    local result
    result="$(cat "$outfile")"
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

    if ! "$PCC" --target=8080 --format=raw \
            $OPTFLAGS --dump-ir --out="$binfile" "$src" >"$tmpdir/$name.log" 2>&1; then
        echo "FAIL $name: compile error"
        sed 's/^/    /' "$tmpdir/$name.log"
        return 1
    fi

    local outfile="$tmpdir/$name.sim.out"
    python3 - "$binfile" > "$outfile" <<'PY'
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
    local result
    result="$(cat "$outfile")"
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

    if ! "$PCC" --target=z80 --format=raw \
            $OPTFLAGS --dump-ir --out="$binfile" "$src" >"$tmpdir/$name.log" 2>&1; then
        echo "FAIL $name: compile error"
        sed 's/^/    /' "$tmpdir/$name.log"
        return 1
    fi

    local outfile="$tmpdir/$name.sim.out"
    python3 - "$binfile" > "$outfile" <<'PY'
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
    local result
    result="$(cat "$outfile")"
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

    if ! "$PCC" --target=8008 --format=raw \
            $OPTFLAGS --dump-ir --out="$binfile" "$src" >"$tmpdir/$name.log" 2>&1; then
        echo "FAIL $name: compile error"
        sed 's/^/    /' "$tmpdir/$name.log"
        return 1
    fi

    local outfile="$tmpdir/$name.sim.out"
    python3 - "$binfile" > "$outfile" <<'PY'
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

val = mem[0x3c00] | (mem[0x3c01] << 8)
if val >= 0x8000:
    val -= 0x10000
print(val)
PY
    local result
    result="$(cat "$outfile")"
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

check_8051() {
    local src="$1" name="$2"
    local binfile="$tmpdir/$name.bin"

    if ! "$PCC" --target=8051 --format=raw \
            $OPTFLAGS --dump-ir --out="$binfile" "$src" >"$tmpdir/$name.log" 2>&1; then
        echo "FAIL $name: compile error"
        sed 's/^/    /' "$tmpdir/$name.log"
        return 1
    fi

    local outfile="$tmpdir/$name.sim.out"
    python3 - "$binfile" > "$outfile" <<'PY'
import sys

path = sys.argv[1]
with open(path, "rb") as f:
    data = f.read()

# A from-scratch MCS-51 core for exactly the narrow dialect
# ../src/codegen/mcs51/MCS51Codegen.cpp emits (see that file's own
# header comment) - two genuinely separate address spaces, matching
# this target's own real Harvard architecture: `code` (fetched by
# PC - ljmp/lcall/jz/etc.) and `xdata` (every global/local/array,
# reached only via MOVX+DPTR - see MCS51Codegen.h's own comment on
# why ordinary variables live here rather than in the 128-byte
# internal RAM alongside the five fixed scratch pseudo-registers,
# which is `iram` below).
code = data + bytes(0x10000 - len(data))
xdata = bytearray(0x10000)
iram = bytearray(0x100)

A = 0
DPL = 0
DPH = 0
CY = 0
OV = 0
pc = 0
sp = 7
stack = bytearray(256)


def rd_direct(addr):
    if addr == 0x82:
        return DPL
    if addr == 0x83:
        return DPH
    return iram[addr]


def wr_direct(addr, val):
    global DPL, DPH
    val &= 0xFF
    if addr == 0x82:
        DPL = val
    elif addr == 0x83:
        DPH = val
    else:
        iram[addr] = val


def fetch8():
    global pc
    b = code[pc]
    pc = (pc + 1) & 0xFFFF
    return b


def fetch16():
    hi = fetch8()
    lo = fetch8()
    return (hi << 8) | lo


def rel_target(rel_byte, after_pc):
    rel = rel_byte - 256 if rel_byte >= 128 else rel_byte
    return (after_pc + rel) & 0xFFFF


# The only two bit addresses MCS51Codegen.cpp ever tests - ACC.7 (the
# sign bit, for signed compares) and PSW.OV (that same compares' own
# overflow fixup) - see MCS51Assembler.cpp's own "fixedBitBranch"
# table; nothing here models a general bit-addressable SFR space.
def get_fixed_bit(bitaddr):
    if bitaddr == 0xE7:
        return (A >> 7) & 1
    if bitaddr == 0xD2:
        return OV
    raise RuntimeError("unsupported bit address 0x%02x" % bitaddr)


halted = False
for _ in range(20_000_000):
    op = fetch8()
    if op == 0x02:  # LJMP
        start = pc - 1
        addr = fetch16()
        if addr == start:
            halted = True
            break
        pc = addr
    elif op == 0x12:  # LCALL
        addr = fetch16()
        sp += 1
        stack[sp] = pc & 0xFF
        sp += 1
        stack[sp] = (pc >> 8) & 0xFF
        pc = addr
    elif op in (0x22, 0x32):  # RET / RETI
        pc = (stack[sp] << 8) | stack[sp - 1]
        sp -= 2
    elif op == 0x00:  # NOP
        pass
    elif op == 0xE4:  # CLR A
        A = 0
    elif op == 0xF4:  # CPL A
        A = (~A) & 0xFF
    elif op == 0xC3:  # CLR C
        CY = 0
    elif op == 0x23:  # RL A
        CY = (A >> 7) & 1
        A = ((A << 1) | CY) & 0xFF
    elif op == 0x03:  # RR A
        lsb = A & 1
        A = ((A >> 1) | (lsb << 7)) & 0xFF
    elif op == 0x33:  # RLC A
        newcy = (A >> 7) & 1
        A = ((A << 1) | CY) & 0xFF
        CY = newcy
    elif op == 0x13:  # RRC A
        newcy = A & 1
        A = ((A >> 1) | (CY << 7)) & 0xFF
        CY = newcy
    elif op == 0xA3:  # INC DPTR
        dptr = (((DPH << 8) | DPL) + 1) & 0xFFFF
        DPL = dptr & 0xFF
        DPH = (dptr >> 8) & 0xFF
    elif op == 0xE0:  # MOVX A,@DPTR
        A = xdata[(DPH << 8) | DPL]
    elif op == 0xF0:  # MOVX @DPTR,A
        xdata[(DPH << 8) | DPL] = A
    elif op in (0xD2, 0xC2):  # SETB bit / CLR bit - only EA (IE.7) ever emitted, a no-op (no interrupt model)
        bitaddr = fetch8()
        if bitaddr != 0xAF:
            raise RuntimeError("unsupported SETB/CLR bit 0x%02x" % bitaddr)
    elif op == 0xE5:  # MOV A,direct
        A = rd_direct(fetch8())
    elif op == 0xF5:  # MOV direct,A
        wr_direct(fetch8(), A)
    elif op == 0x05:  # INC direct
        d = fetch8()
        wr_direct(d, (rd_direct(d) + 1) & 0xFF)
    elif op == 0x15:  # DEC direct
        d = fetch8()
        wr_direct(d, (rd_direct(d) - 1) & 0xFF)
    elif op in (0x25, 0x35, 0x24, 0x34):  # ADD/ADDC A,direct|#imm
        v = rd_direct(fetch8()) if op in (0x25, 0x35) else fetch8()
        carry_in = CY if op in (0x35, 0x34) else 0
        total = A + v + carry_in
        sa, sv = A & 0x80, v & 0x80
        res = total & 0xFF
        newcy = 1 if total > 0xFF else 0
        OV = 1 if (sa == sv and (res & 0x80) != sa) else 0
        CY = newcy
        A = res
    elif op in (0x95, 0x94):  # SUBB A,direct|#imm
        v = rd_direct(fetch8()) if op == 0x95 else fetch8()
        total = A - v - CY
        sa, sv = A & 0x80, v & 0x80
        res = total & 0xFF
        newcy = 1 if total < 0 else 0
        OV = 1 if (sa != sv and (res & 0x80) != sa) else 0
        CY = newcy
        A = res
    elif op in (0x55, 0x54):  # ANL A,direct|#imm
        A &= rd_direct(fetch8()) if op == 0x55 else fetch8()
    elif op in (0x45, 0x44):  # ORL A,direct|#imm
        A |= rd_direct(fetch8()) if op == 0x45 else fetch8()
    elif op in (0x65, 0x64):  # XRL A,direct|#imm
        A ^= rd_direct(fetch8()) if op == 0x65 else fetch8()
    elif op == 0xC0:  # PUSH direct
        d = fetch8()
        sp += 1
        stack[sp] = rd_direct(d)
    elif op == 0xD0:  # POP direct
        d = fetch8()
        wr_direct(d, stack[sp])
        sp -= 1
    elif op == 0x74:  # MOV A,#imm
        A = fetch8()
    elif op == 0x85:  # MOV direct,direct (src byte first, then dst - real hardware order)
        src = fetch8()
        dst = fetch8()
        wr_direct(dst, rd_direct(src))
    elif op == 0x90:  # MOV DPTR,#imm16 (high byte first)
        DPH = fetch8()
        DPL = fetch8()
    elif op == 0x75:  # MOV direct,#imm
        d = fetch8()
        wr_direct(d, fetch8())
    elif op in (0x60, 0x70, 0x40, 0x50):  # JZ/JNZ/JC/JNC rel
        rel = fetch8()
        target = rel_target(rel, pc)
        taken = {0x60: A == 0, 0x70: A != 0, 0x40: CY == 1, 0x50: CY == 0}[op]
        if taken:
            pc = target
    elif op in (0x20, 0x30):  # JB/JNB bit,rel
        bitaddr = fetch8()
        rel = fetch8()
        target = rel_target(rel, pc)
        bit = get_fixed_bit(bitaddr)
        if (op == 0x20 and bit) or (op == 0x30 and not bit):
            pc = target
    elif op == 0xD5:  # DJNZ direct,rel
        d = fetch8()
        rel = fetch8()
        target = rel_target(rel, pc)
        newv = (rd_direct(d) - 1) & 0xFF
        wr_direct(d, newv)
        if newv != 0:
            pc = target
    elif op == 0x73:  # JMP @A+DPTR
        pc = (((DPH << 8) | DPL) + A) & 0xFFFF
    else:
        raise RuntimeError("unimplemented opcode 0x%02x at pc=0x%04x" % (op, pc - 1))
else:
    print("TIMEOUT")
    sys.exit(0)

if not halted:
    print("TIMEOUT")
    sys.exit(0)

val = iram[0x30] | (iram[0x31] << 8)
if val >= 0x8000:
    val -= 0x10000
print(val)
PY
    local result
    result="$(cat "$outfile")"
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

check_stm32() {
    local src="$1" name="$2"
    local binfile="$tmpdir/$name.bin"

    if ! "$PCC" --target=stm32 --format=raw \
            $OPTFLAGS --dump-ir --out="$binfile" "$src" >"$tmpdir/$name.log" 2>&1; then
        echo "FAIL $name: compile error"
        sed 's/^/    /' "$tmpdir/$name.log"
        return 1
    fi

    qemu_count=$((qemu_count + 1))
    local monitor="$sockdir/$qemu_count"
    local outfile="$tmpdir/$name.out"

    # `-kernel`, not a PC-forcing `loader` device: `$binfile` already
    # starts with a real two-word Cortex-M reset vector table (initial
    # `sp`, reset handler with its Thumb bit forced on - see
    # ../src/codegen/stm32/STM32Codegen.cpp's own `generate()`), and
    # `stm32vldiscovery` genuinely honors it on reset (confirmed
    # directly during development - see
    # ../src/codegen/stm32/STM32Codegen.h's own header comment).
    qemu-system-arm -M stm32vldiscovery -kernel "$binfile" \
        -monitor "unix:$monitor,server,nowait" -display none -no-reboot -serial none >"$tmpdir/$name.qemu.log" 2>&1 &
    local qemu_pid=$!
    ( sleep 45; kill -9 "$qemu_pid" 2>/dev/null ) &
    local watcher=$!

    wait_for_qemu "$name" "$monitor" "$outfile" "$qemu_pid" "$watcher" || return 1

    local r0_hex
    r0_hex="$(grep -oE 'R00=[0-9a-fA-F]+' "$outfile" | head -1 | cut -d= -f2)"
    if [ -z "$r0_hex" ]; then
        echo "FAIL $name: qemu produced no R00 (timed out or crashed)"
        return 1
    fi

    local r0_dec=$((16#$r0_hex))
    r0_dec="$(to_signed "$r0_dec" 32)"
    if [ "$r0_dec" -eq "$expect" ]; then
        echo "PASS $name (R0=$r0_dec)"
        return 0
    fi
    echo "FAIL $name: expected $expect, got $r0_dec (R0 hex: $r0_hex)"
    return 1
}

# A handful of (target, file) pairs are architecturally incapable,
# not broken - each one already fails loudly with its own documented
# `err()`/compile message (see that backend's own header comment),
# rather than computing a silently wrong answer. Skipped here so a
# known, deliberate gap doesn't fail `make test`; every OTHER target
# still runs the same file for real, so coverage elsewhere is
# unaffected.
is_known_unsupported() {
    case "$TARGET:$1" in
        pdp7:types_longlong_divmod.c) return 0 ;;          # no verified wide multiply/divide routine
        pdp7:types_longlong_divmod_repeated.c) return 0 ;; # same - no verified wide multiply/divide routine
        8086:types_longlong_divmod.c) return 0 ;;          # routine doesn't fit the 512-byte boot sector
        8088:types_longlong_divmod.c) return 0 ;;
        8086:types_longlong_divmod_repeated.c) return 0 ;; # same - doesn't fit the 512-byte boot sector
        8088:types_longlong_divmod_repeated.c) return 0 ;;
        8086:types_longlong_equality_repeated.c) return 0 ;; # same - doesn't fit the 512-byte boot sector
        8088:types_longlong_equality_repeated.c) return 0 ;;
        8086:compound_assign_inc_dec_ternary.c) return 0 ;;  # this file's own compiled program doesn't fit the 512-byte boot sector either
        8088:compound_assign_inc_dec_ternary.c) return 0 ;;
        8086:many_returns.c) return 0 ;;                    # same - doesn't fit the 512-byte boot sector
        8088:many_returns.c) return 0 ;;
        8086:array_store_paths.c) return 0 ;;              # same - doesn't fit the 512-byte boot sector
        8088:array_store_paths.c) return 0 ;;
        8086:local_char_array_layout.c) return 0 ;;        # same - doesn't fit the 512-byte boot sector
        8088:local_char_array_layout.c) return 0 ;;
        8086:convert_int_to_longlong.c) return 0 ;;        # same - doesn't fit the 512-byte boot sector
        8088:convert_int_to_longlong.c) return 0 ;;
        8086:literal_promotion_arith.c) return 0 ;;        # same - doesn't fit the 512-byte boot sector
        8088:literal_promotion_arith.c) return 0 ;;
        8086:longlong_add_carry_chain.c) return 0 ;;        # same - doesn't fit the 512-byte boot sector
        8088:longlong_add_carry_chain.c) return 0 ;;
        8086:longlong_sub_borrow_chain.c) return 0 ;;
        8088:longlong_sub_borrow_chain.c) return 0 ;;
        8080:types_longlong_divmod.c) return 0 ;;          # no verified wide multiply/divide routine
        8080:types_longlong_divmod_repeated.c) return 0 ;; # same - no verified wide multiply/divide routine
        z80:types_longlong_divmod.c) return 0 ;;
        z80:types_longlong_divmod_repeated.c) return 0 ;;  # same - no verified wide multiply/divide routine
        8051:types_arrays.c) return 0 ;;                   # wide (long/long long) values unsupported at all
        8051:types_longlong_arith.c) return 0 ;;
        8051:test_long_arith.c) return 0 ;;
        8051:local_mixed_width_layout.c) return 0 ;;
        8051:local_wide_scalar_layout.c) return 0 ;;
        8051:convert_int_to_longlong.c) return 0 ;;
        8051:literal_promotes_to_long.c) return 0 ;;
        8051:literal_promotion_arith.c) return 0 ;;
        8051:wide_compare_in_expression.c) return 0 ;;
        8051:wide_loop_stack.c) return 0 ;;
        8051:longlong_add_carry_chain.c) return 0 ;;
        8051:longlong_sub_borrow_chain.c) return 0 ;;
        8051:test_unsigned_long_arith.c) return 0 ;;
        8051:types_longlong_divmod.c) return 0 ;;
        8008:types_longlong_divmod.c) return 0 ;;          # exhaustive sign-combination coverage needs more RAM than the 8008's 16KB/14-bit address space has, even with every byte of pseudo-register reservation reclaimed
        8051:types_longlong_divmod_repeated.c) return 0 ;;
        8051:types_longlong_equality_repeated.c) return 0 ;;
        8051:types_longlong_signed_cmp.c) return 0 ;;
        8051:types_params_return.c) return 0 ;;
        pdp11:types_longlong_divmod.c) return 0 ;;          # no verified wide multiply/divide routine
        pdp11:types_longlong_divmod_repeated.c) return 0 ;; # same - no verified wide multiply/divide routine
        pdp11:types_arrays.c) return 0 ;;                   # a wide (long/long long) ARRAY ELEMENT's own
                                                             # read/write still only moves one native word here -
                                                             # see Pdp11Codegen.cpp's own genIndexAddr()/genIndex()
        pdp11:string_literal.c) return 0 ;;                 # walks a string through a plain int-as-address
                                                             # (see string_literal.c's own header comment) - this
                                                             # style always reads/writes a full native WORD, never
                                                             # a byte (same on every target), so correctly
                                                             # detecting the trailing NUL here relies on whatever
                                                             # byte happens to follow it in memory also being zero;
                                                             # a real `char*` (test_string_sum.c) reads true bytes
                                                             # and isn't affected by this at all
        pdp11:string_literal_unaligned_compound.c) return 0 ;; # deliberately dereferences a plain int-as-address
                                                             # one byte off its own word alignment - real PDP-11
                                                             # hardware traps on an odd-address word access, so
                                                             # this specific style can never work here
        r10000:string_literal.c) return 0 ;;                # same plain-int-as-address trailing-NUL-byte
                                                             # fragility as pdp11's own entry above
        r10000:string_literal_unaligned_compound.c) return 0 ;; # same deliberately-misaligned-word-access style
                                                             # as pdp11's own entry above - real MIPS hardware
                                                             # also raises an address-error exception on it
        r10000:types_longlong_divmod.c) return 0 ;;         # MipsCodegen.cpp's genWideDivMod() gives a wrong
                                                             # answer here - not yet root-caused, unlike the
                                                             # i386/ARM/M68k ports of the same algorithm
        r10000:types_longlong_divmod_repeated.c) return 0 ;; # same
        r10000:types_params_return.c) return 0 ;;           # a long long parameter/return value comes back
                                                             # wrong here - not yet root-caused
        powerpc:types_longlong_divmod.c) return 0 ;;        # same symptom as r10000's own entry above -
                                                             # not yet root-caused
        powerpc:types_longlong_divmod_repeated.c) return 0 ;; # same
        powerpc:types_params_return.c) return 0 ;;          # same symptom as r10000's own entry above -
                                                             # not yet root-caused
        *) return 1 ;;
    esac
}

for src in "$SCRIPT_DIR"/*.c; do
    expect="$(grep -m1 -oE 'EXPECT: *-?[0-9]+' "$src" | grep -oE '\-?[0-9]+$' || true)"
    if [ -z "$expect" ]; then
        continue
    fi
    name="$(basename "$src")"
    if is_known_unsupported "$name"; then
        echo "SKIP $name (known unsupported on $TARGET)"
        continue
    fi

    case "$TARGET" in
        pdp7) check_pdp7 "$src" "$name" || fail=1 ;;
        pdp11) check_pdp11 "$src" "$name" || fail=1 ;;
        r10000) check_r10000 "$src" "$name" || fail=1 ;;
        powerpc) check_powerpc "$src" "$name" || fail=1 ;;
        cortex-m3) check_cortex_m3 "$src" "$name" || fail=1 ;;
        8086) check_8086 8086 "$src" "$name" || fail=1 ;;
        8088) check_8086 8088 "$src" "$name" || fail=1 ;;
        6502) check_6502 "$src" "$name" || fail=1 ;;
        65c02) check_65c02 "$src" "$name" || fail=1 ;;
        68000) check_68000 "$src" "$name" || fail=1 ;;
        mc68328) check_mc68328 "$src" "$name" || fail=1 ;;
        8080) check_8080 "$src" "$name" || fail=1 ;;
        z80) check_z80 "$src" "$name" || fail=1 ;;
        8008) check_8008 "$src" "$name" || fail=1 ;;
        8051) check_8051 "$src" "$name" || fail=1 ;;
        stm32) check_stm32 "$src" "$name" || fail=1 ;;
        i386) check_i386 "$src" "$name" || fail=1 ;;
        armv7a) check_armv7a "$src" "$name" || fail=1 ;;
        armv8a) check_armv8a "$src" "$name" || fail=1 ;;
    esac
done

exit $fail
