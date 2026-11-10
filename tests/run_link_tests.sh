#!/usr/bin/env bash
# Exercises real separate compilation: every tests/link/<case>_a.c /
# tests/link/<case>_b.c pair is compiled independently to a real
# relocatable object (--format=coff), combined with --link into one
# program, then run for real under the chosen target's own hardware
# simulator and checked against the "// EXPECT: <decimal>" comment on
# the "_a.c" half (the one with main) - same convention as
# run_tests.sh's own single-file loop, just for the extern/--link path
# that loop can't reach (every one of its own files is a complete,
# single-TU program - see docs/language.md's "Extern
# declarations": extern only means anything under real separate
# compilation).
#
# Usage: tests/run_link_tests.sh [path/to/rcc] [--target=pdp7|8086|8088|i386|vax|avr|alpha]

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LINK_DIR="$SCRIPT_DIR/link"
RCC="$SCRIPT_DIR/../build/bin/rcc"
TARGET="pdp7"

for arg in "$@"; do
    case "$arg" in
        --target=*) TARGET="${arg#--target=}" ;;
        *) RCC="$arg" ;;
    esac
done

if [ ! -x "$RCC" ]; then
    echo "rcc binary not found at $RCC - build it first ('make')" >&2
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
    vax)
        if ! command -v vax780 >/dev/null 2>&1; then
            echo "vax780 (open-simh) not found on PATH - install it (e.g. 'brew install open-simh' or 'apt install simh')" >&2
            exit 1
        fi
        ;;
    avr)
        if ! command -v qemu-system-avr >/dev/null 2>&1; then
            echo "qemu-system-avr not found on PATH - install it (e.g. 'brew install qemu')" >&2
            exit 1
        fi
        if ! command -v python3 >/dev/null 2>&1; then
            echo "python3 not found on PATH - needed to talk to qemu's monitor" >&2
            exit 1
        fi
        ;;
    alpha)
        # No external tool needed - see check_alpha()'s own comment.
        ;;
    *)
        echo "unknown --target '$TARGET' for run_link_tests.sh (known: pdp7, 8086, 8088, i386, vax, avr, alpha - the targets this harness's own link flow is wired up for so far)" >&2
        exit 1
        ;;
esac

fail=0
tmpdir="$(mktemp -d)"
# qemu monitor sockets live in their own short directory: a unix socket
# path is limited to about 104 bytes, which `$tmpdir` plus a long case
# name can exceed.
sockdir="$(mktemp -d /tmp/rccq.XXXXXX)"
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

    if ! "$RCC" --target="$TARGET" --format="$fmt" --out="$aobj" "$a" >"$tmpdir/$name.log" 2>&1; then
        echo "FAIL $name: compile error ($a)"
        sed 's/^/    /' "$tmpdir/$name.log"
        return 1
    fi
    if ! "$RCC" --target="$TARGET" --format="$fmt" --out="$bobj" "$b" >"$tmpdir/$name.log" 2>&1; then
        echo "FAIL $name: compile error ($b)"
        sed 's/^/    /' "$tmpdir/$name.log"
        return 1
    fi
    if ! "$RCC" --link --target="$TARGET" --format="$linkfmt" --out="$outfile" "$aobj" "$bobj" >"$tmpdir/$name.log" 2>&1; then
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

# vax: the linked flat image loads at 4000 (hex), VaxAssembler.h's kVaxBaseAddress, and its
# entry sequence ends in a `halt` with the result in R0 - read back the same way
# run_tests.sh's own check_vax() does.
check_vax() {
    local a="$1" b="$2" name="$3"
    local binfile="$tmpdir/$name.bin"
    link_case "$a" "$b" "$name" coff flat "$binfile" || return 1

    local simfile="$tmpdir/$name.simh" outfile="$tmpdir/$name.out"
    printf 'load -o %s 4000\nd pc 4000\ngo\ne r0\nquit\n' "$binfile" >"$simfile"

    ( vax780 "$simfile" </dev/null >"$outfile" 2>/dev/null ) &
    local sim_pid=$!
    ( sleep 10; kill -9 "$sim_pid" 2>/dev/null ) &
    local watcher=$!
    wait "$sim_pid" 2>/dev/null
    kill "$watcher" 2>/dev/null
    wait "$watcher" 2>/dev/null

    local r0_hex
    r0_hex="$(grep '^R0:' "$outfile" | awk '{print $2}')"
    if [ -z "$r0_hex" ]; then
        echo "FAIL $name: simulator produced no R0 (timed out or crashed)"
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

# avr: the linked flat image loads at flash address 0 (AVRAssembler.h's kAVRFlashBaseAddress -
# the real ATmega328P reset vector), run under qemu-system-avr's own "uno" board the same way
# run_tests.sh's own check_avr() does; the result sits in r16:r17 the instant execution reaches
# the entry sequence's own spin loop.
check_avr() {
    local a="$1" b="$2" name="$3"
    local binfile="$tmpdir/$name.bin"
    link_case "$a" "$b" "$name" coff flat "$binfile" || return 1

    qemu_count=$((qemu_count + 1))
    local monitor="$sockdir/$qemu_count"
    local outfile="$tmpdir/$name.out"

    qemu-system-avr -M uno -device "loader,file=$binfile,addr=0x0" \
        -monitor "unix:$monitor,server,nowait" -display none -no-reboot -serial none \
        >/dev/null 2>&1 &
    local qemu_pid=$!
    ( sleep 45; kill -9 "$qemu_pid" 2>/dev/null ) &
    local watcher=$!

    python3 -I "$SCRIPT_DIR/qemu_wait.py" "$monitor" 40 >"$outfile"

    kill "$qemu_pid" 2>/dev/null
    wait "$qemu_pid" 2>/dev/null
    kill "$watcher" 2>/dev/null
    wait "$watcher" 2>/dev/null

    local r16_hex r17_hex
    r16_hex="$(grep -oE 'R\[16\]: *[0-9a-fA-F]+' "$outfile" | head -1 | grep -oE '[0-9a-fA-F]+$')"
    r17_hex="$(grep -oE 'R\[17\]: *[0-9a-fA-F]+' "$outfile" | head -1 | grep -oE '[0-9a-fA-F]+$')"
    if [ -z "$r16_hex" ] || [ -z "$r17_hex" ]; then
        echo "FAIL $name: qemu produced no R[16]/R[17] (timed out or crashed)"
        return 1
    fi

    local dec=$(( (16#$r17_hex << 8) | 16#$r16_hex ))
    dec="$(to_signed "$dec" 16)"
    if [ "$dec" -eq "$expect" ]; then
        echo "PASS $name (r16:r17=$dec)"
        return 0
    fi
    echo "FAIL $name: expected $expect, got $dec (r16=$r16_hex r17=$r17_hex)"
    return 1
}

# alpha: the linked flat image loads at AlphaAssembler.h's kAlphaBaseAddress and ends in
# `halt`'s own self-branch (AlphaCodegen.h) - read back with the same from-scratch core
# run_tests.sh's own check_alpha() uses, since qemu-system-alpha needs a real PALcode image
# before it executes anything, even cross-object-linked code (see that function's own
# comment).
check_alpha() {
    local a="$1" b="$2" name="$3"
    local binfile="$tmpdir/$name.bin"
    link_case "$a" "$b" "$name" coff flat "$binfile" || return 1

    local outfile="$tmpdir/$name.sim.out"
    python3 -I - "$binfile" > "$outfile" <<'PY'
import struct
import sys

BASE = 0x00120000
MASK64 = (1 << 64) - 1
STEP_LIMIT = 2_000_000

path = sys.argv[1]
with open(path, "rb") as f:
    data = f.read()
mem = bytearray(0x400000)
mem[0:len(data)] = data

regs = [0] * 32


def s16(v):
    v &= 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


def s32(v):
    v &= 0xFFFFFFFF
    return v - 0x100000000 if v & 0x80000000 else v


def s64(v):
    v &= MASK64
    return v - (1 << 64) if v & (1 << 63) else v


def getreg(i):
    return 0 if i == 31 else regs[i]


def setreg(i, v):
    if i != 31:
        regs[i] = v & MASK64


def rd32(addr):
    return struct.unpack_from("<I", mem, addr - BASE)[0]


def rdmem(addr, n):
    off = addr - BASE
    return int.from_bytes(mem[off:off + n], "little")


def wrmem(addr, v, n):
    off = addr - BASE
    mem[off:off + n] = (v & ((1 << (8 * n)) - 1)).to_bytes(n, "little")


pc = BASE
halted = False
for _ in range(STEP_LIMIT):
    word = rd32(pc)
    opcode = (word >> 26) & 0x3F
    ra = (word >> 21) & 0x1F
    this_pc = pc
    pc += 4

    if opcode in (0x08, 0x09, 0x0A, 0x0C, 0x0D, 0x0E, 0x28, 0x29, 0x2C, 0x2D, 0x1A):
        rb = (word >> 16) & 0x1F
        disp16 = word & 0xFFFF
        if opcode == 0x1A:
            action = (disp16 >> 14) & 0x3
            target = getreg(rb)
            if action == 1:
                setreg(ra, pc)
            pc = target
            continue
        disp = s16(disp16)
        addr = getreg(rb) + disp
        if opcode == 0x08:
            setreg(ra, getreg(rb) + disp)
        elif opcode == 0x09:
            setreg(ra, getreg(rb) + (disp << 16))
        elif opcode == 0x0A:
            setreg(ra, rdmem(addr, 1))
        elif opcode == 0x0C:
            setreg(ra, rdmem(addr, 2))
        elif opcode == 0x0E:
            wrmem(addr, getreg(ra), 1)
        elif opcode == 0x0D:
            wrmem(addr, getreg(ra), 2)
        elif opcode == 0x28:
            setreg(ra, s32(rdmem(addr, 4)) & MASK64)
        elif opcode == 0x29:
            setreg(ra, rdmem(addr, 8))
        elif opcode == 0x2C:
            wrmem(addr, getreg(ra), 4)
        elif opcode == 0x2D:
            wrmem(addr, getreg(ra), 8)
        continue

    if 0x30 <= opcode <= 0x3F:
        disp21 = word & 0x1FFFFF
        if disp21 & (1 << 20):
            disp21 -= 1 << 21
        target = this_pc + 4 + disp21 * 4
        v = s64(getreg(ra))
        taken = {
            0x30: True, 0x38: (v & 1) == 0, 0x39: v == 0, 0x3A: v < 0,
            0x3B: v <= 0, 0x3C: (v & 1) == 1, 0x3D: v != 0, 0x3E: v >= 0, 0x3F: v > 0,
        }.get(opcode)
        if taken is None:
            raise RuntimeError(f"unsupported branch opcode {opcode:#x} at {this_pc:#x}")
        if taken and target == this_pc:
            halted = True
            break
        if taken:
            pc = target
        continue

    if opcode in (0x10, 0x11, 0x12, 0x13):
        is_lit = (word >> 12) & 1
        rc = word & 0x1F
        function = (word >> 5) & 0x7F
        bv = ((word >> 13) & 0xFF) if is_lit else getreg((word >> 16) & 0x1F)
        av = getreg(ra)
        if opcode == 0x10:
            fn = {0x20: lambda: av + bv, 0x29: lambda: av - bv,
                  0x2D: lambda: 1 if (av & MASK64) == (bv & MASK64) else 0,
                  0x4D: lambda: 1 if s64(av) < s64(bv) else 0,
                  0x6D: lambda: 1 if s64(av) <= s64(bv) else 0,
                  0x1D: lambda: 1 if (av & MASK64) < (bv & MASK64) else 0,
                  0x3D: lambda: 1 if (av & MASK64) <= (bv & MASK64) else 0}.get(function)
        elif opcode == 0x11:
            fn = {0x00: lambda: av & bv, 0x20: lambda: av | bv, 0x40: lambda: av ^ bv,
                  0x28: lambda: av | (~bv)}.get(function)
        elif opcode == 0x12:
            sh = bv & 0x3F
            fn = {0x39: lambda: av << sh, 0x34: lambda: (av & MASK64) >> sh,
                  0x3C: lambda: s64(av) >> sh}.get(function)
        else:
            fn = {0x20: lambda: av * bv, 0x30: lambda: ((av & MASK64) * (bv & MASK64)) >> 64}.get(function)
        if fn is None:
            raise RuntimeError(f"unsupported opcode {opcode:#x}/{function:#x} at {this_pc:#x}")
        setreg(rc, fn())
        continue

    raise RuntimeError(f"unsupported opcode {opcode:#x} at {this_pc:#x}")
else:
    print("TIMEOUT")
    sys.exit(0)

if not halted:
    print("TIMEOUT")
    sys.exit(0)

print(s32(regs[1]))
PY
    local result
    result="$(cat "$outfile")"
    if [ "$result" = "TIMEOUT" ]; then
        echo "FAIL $name: simulator never reached the halt loop (step limit exceeded)"
        return 1
    fi
    if [ "$result" -eq "$expect" ] 2>/dev/null; then
        echo "PASS $name (t0=$result)"
        return 0
    fi
    echo "FAIL $name: expected $expect, got $result"
    return 1
}

# A handful of (target, case) pairs are architecturally incapable, not
# broken, the same situation run_tests.sh's own is_known_unsupported()
# covers for the single-file suite: 8086/8088's --format=raw output is
# always exactly one 512-byte boot sector (see RawFormat.cpp's
# kSectorSize), with no multi-sector loader, so any case whose linked
# code+data crosses that line fails this way on those two targets
# regardless of how compact rcc's own codegen is. stack_safety_headroom
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
        avr:stack_safety_headroom) return 0 ;;  # filler[1000] alone (2000 bytes) doesn't fit
                                                  # the ATmega328P's 2 KB of SRAM - see
                                                  # AVRCodegen.h's own header comment.
        avr:extern_array) return 0 ;;   # SRAM has no static initial contents, so a Global's
                                          # initializer is runtime `sts` code that only ever runs
                                          # out of the object defining `main` (see AVRCodegen.h's
                                          # own header comment) - an initializer defined in a
                                          # *different* linked object, read before any of that
                                          # object's own functions are ever called, never runs.
                                          # No Harvard-data target here (mcs51/pic16/pic18) is
                                          # wired into this harness at all, for the same reason.
        avr:extern_basic) return 0 ;;   # same as extern_array above.
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
        vax) check_vax "$a" "$b" "$base" || fail=1 ;;
        avr) check_avr "$a" "$b" "$base" || fail=1 ;;
        alpha) check_alpha "$a" "$b" "$base" || fail=1 ;;
    esac
done

exit $fail
