# Supported platforms

`rcc` is a compiler and linker for a small subset of C, targeting real
retro hardware through pluggable backends. Today it supports:

For the exact, up-to-date list of every target's own output-format
choices (the bullets below describe each one in prose, which can go
stale), run `rcc --list-platforms` - it prints every `--target=<name>[-<host>]`
triple and every `--platform=<name>` name this build accepts, generated
directly from each target's own `TargetLinker::outputFormats()` (or, for
the few targets with no `--link` support, `Backend::assembledFormats()`)
rather than hand-maintained. `--platform=<name>` (e.g.
`--platform=6502-dos33`) is sugar for the matching `--target`/`--format`
pair - see `docs/using.md`.

- **pdp1** — PDP-1 assembly (`src/codegen/pdp1/Pdp1Codegen.h`), a genuine
  one's-complement machine with no link/carry bit and no hardware
  multiply/divide, 4096 words of address space (half of pdp7's), producing
  a bootable RIM-format paper tape runnable under the `open-simh` PDP-1
  simulator. `char`/`short`/`int` are fully supported, including software
  multiply/divide (sign-magnitude shift-add/shift-subtract); `long`/`long
  long` are not (a correct multi-word carry chain needs its own
  independently-verified algorithm on a machine with no link bit at all);
  `unsigned` is reliable up to 2^17-1, not the full 2^18-1 (the top half of
  the range isn't reliably distinguishable from a negative signed value on
  this one's-complement ALU) - all documented in `Pdp1Codegen.h`'s own
  class comment. No separate-compilation/`--link` support.
- **pdp7** — PDP-7 assembly, assembled and linked into real PDP-7 binary
  formats (bootable paper tape, etc.), runnable under the `open-simh`
  PDP-7 simulator.
- **8086** — 16-bit real-mode 8086 assembly (NASM syntax), producing a
  bare flat binary loaded/entered at `kI8086BaseAddress` (`0x1000`) with
  no boot-sector/BIOS loading convention of any kind (building a bootable
  image from it is out of scope for this project), or (`--format=elf`) a
  minimal flat ELF executable — same single-blob model as every other
  format here (no section/symbol tables), and incomplete the way
  `--format=aout` is: a real ELF loader expects 32-bit protected-mode
  code, not 16-bit real mode, so this is for tooling that just needs to
  recognize the file as ELF. `tests/run_tests.sh --target=8086` is
  deliberately disabled for now - its qemu harness used to BIOS-boot the
  old boot-sector image, which no longer exists - see `docs/known-bugs.md`.
- **8088** — a deliberate, software-identical alias for `8086`: the
  8088's one real difference (an 8-bit external data bus) is invisible
  to every instruction this compiler could emit, so `--target=8088`
  reuses the 8086 backend outright.
- **8086-dos** — the same `I8086Codegen` backend as `8086`/`8088` (every instruction it
  emits is identical), but targeting a real MS-DOS `.com` program instead of a bare
  flat binary: `--format=raw` (and `--link --format=raw`) produce exactly the
  plain-bytes program a `.com` file is - DOS's own loader already makes CS=DS=ES=SS
  the one segment the process gets (the PSP) and gives it a valid stack before the
  entry point ever runs, so unlike `8086`'s own entry stub there's no segment/stack
  setup at all. Loaded/entered at `kI8086DosBaseAddress` (`0x100`, where DOS always
  loads a `.com`), and exits via `INT 21h` function `4Ch` instead of `hlt` (see
  `I8086Codegen::generate()`'s and `I8086LinkerFactory.cpp`'s own `dosAbi`/
  `kDosEntryAsm` comments). Gets its own reserved COFF machine ID, `kI8086DosCoffMachine`
  (`0x0D86`), distinct from plain `8086`'s `0x0086`, since the two entry/exit
  conventions are link-incompatible - `GenericLinker`'s machine-type check is what
  catches an attempt to `--link` one target's objects against the other's entry
  object. `src/rt/msdos/` provides an `INT 21h` runtime library - console
  I/O (`dos_putchar`/`dos_puts`/`dos_print`/`dos_getchar`/`dos_gets`/
  `dos_kbhit`), file I/O (`dos_create`/`dos_open`/`dos_close`/`dos_read`/
  `dos_write`/`dos_delete`/`dos_rename`/`dos_seek`, each returning a
  non-negative result on success or a negated DOS error code on failure),
  date/time (`dos_date_year`/`_month`/`_day`/`_weekday`,
  `dos_time_hour`/`_minute`/`_second`/`_hundredths`), the command tail
  (`dos_cmdline_len`/`dos_cmdline_char`, read straight out of the PSP at
  absolute addresses `0x80`/`0x81` - a `.com` program's own DS already is
  the PSP segment), and `dos_exit` - the same role `src/rt/apple2/`'s
  ROM/ProDOS-MLI wrapper plays for `6502`/`65c02`. Deliberately not
  provided: environment-block access and `INT 21h`/48h-4Ah (DOS memory
  allocation), both of which hand back a real-mode *segment* that there's
  no way to dereference from C - this dialect's own assembler has no
  segment-override syntax (`es:[bx]`) at all. Every staging global is a
  full `int`, read/written through `[g_name]` inside `asm { }`, never bare
  `g_name` (this dialect's "mov" has no `ah`/`al`/`dl`-style sub-register
  names - see `I8086Assembler.cpp`'s own `regIndex()` - so a function
  number or byte argument that has to land in one specific byte of a
  register is folded into one `mov ax/dx, imm16` instead, or extracted
  from one with `and ax, 255`/`shr ax, 8` where DOS packs two fields into
  one register); and this C subset's own number literals are decimal-only
  (no `0x` hex, unlike the asm-text grammar inline `asm { }` blocks use),
  so `dos_exit()`'s `INT 21h`/`4Ch` is built from `19456 + code`, not
  `0x4C00 + code` - see `docs/language.md`'s own `NUMBER` note.
- **6502** — 6502 assembly (own dialect; `src/codegen/m6502/M6502Assembler.h`),
  a bare flat binary loaded/entered at `$0600`, no OS/I/O conventions, on
  a CPU with three 8-bit registers, no hardware multiply/divide, and
  branches limited to ±127 bytes (see `M6502Codegen.h`'s header comment
  for the workarounds, including recursion: the real stack is only used
  for `JSR`/`RTS`; every Local/Param lives in a software frame reached
  through a zero-page frame pointer). `tests/run_tests.sh --target=6502`
  runs against a real 6502 core via `py65` (`pip install py65`).
  `--link --format=dos33`/`prodos` wrap the same linked bytes in an
  Apple ][ DOS 3.3/ProDOS metadata convention - see `docs/using.md`'s
  own `--format` row and `src/rt/apple2/` for a ROM/ProDOS-MLI call
  runtime library built on top of this target. `--link --format=prg`
  wraps the same linked bytes in the Commodore VIC-20/C64/PET load-
  address convention instead - see `src/rt/vic20/` (`--base=4097`,
  $1001, the unexpanded VIC-20's own BASIC-program start address;
  `examples/6502-vic20/` for a worked "hello world") and `src/rt/c64/`
  (`--base=2049`, $0801, the C64's own BASIC-program start address;
  `examples/6502-c64/`) - both KERNAL call runtime libraries built on
  top of this target, duplicated between the two directories rather
  than shared even though they call the identical jump-table addresses
  (see `src/rt/c64/c64.h`'s own header comment for why).
- **65c02** — WDC 65C02, same dialect/platform choices as `6502` plus the
  real CMOS-only instructions `M65C02Codegen.cpp`/`M65C02Assembler.cpp`
  support: `STZ`, used automatically by codegen itself to zero a zero-page
  pseudo-register (`stzp`/`stz`) instead of the NMOS `LDA #0`/`STA` idiom
  (NMOS 6502 has no "store zero"); and `rmb0`-`rmb7`/`smb0`-`smb7` (reset/
  set bit n of a zero-page byte directly), `bbr0`-`bbr7`/`bbs0`-`bbs7`
  (branch if bit n of a zero-page byte is reset/set), and `wai`/`stp`
  (wait for interrupt/stop the clock) - none of which any C-level
  construct maps to, so (unlike `STZ`) codegen never emits these itself;
  they're only reachable through a program's own `asm { }` escape hatch
  (see `docs/language.md`'s "Inline asm"; `tests/65c02/*.c` holds
  worked examples of each). Tested against `py65`'s separate CMOS core
  (`py65.devices.mpu65c02.MPU`), which implements `rmb0`-`7`/`smb0`-`7`/
  `wai` itself; `bbr0`-`7`/`bbs0`-`7`/`stp` aren't modeled by that core at
  all, so `tests/run_tests.sh`'s own `check_65c02()` patches those three
  opcodes into the simulator instance directly before stepping - the same
  "small core for whatever a mainstream tool doesn't model" idea `alpha`/
  `saturn`/`tms320c6713`'s own from-scratch interpreters already use, just
  patching three opcodes onto an otherwise-real core instead of writing a
  whole one.
- **68000** — 68000 assembly (own dialect; `M68kAssembler.h`), a bare flat
  binary loaded/entered at `$1000` with no boot ROM/vector table (the
  test harness sets PC directly). Rich enough that almost everything maps
  onto a real instruction: `MULS.W`/`DIVS.W` (the original 68000's only
  multiply/divide — 16-bit operands even though `int` is 32 bits, a
  narrower documented limit than other backends' non-negative-only
  software multiply/divide), a real variable-count shift, real signed
  `CMP.L`+`Bcc`, and a real hardware stack (`A7`, `JSR`/`RTS`). Tested
  against `qemu-system-m68k -cpu m68000`.
- **mc68328** — the real Motorola/Freescale MC68328 "DragonBall" SoC (the
  chip in the original Palm Pilot), a real MC68EC000 core — bit-for-bit
  the same instruction set as `68000`, so every instruction choice is
  identical. The one difference: a real two-word reset vector table at
  address 0 (`dl kInitialSP` / `dl _start`), since real DragonBall
  silicon's hardware reset fetches its initial `SSP`/`PC` from there.
  Tested against `qemu-system-m68k -M none` with no PC-forcing, relying
  on that vector table to start execution.
- **pdp11** — PDP-11 assembly (own dialect; `Pdp11Assembler.h`), a bare
  flat binary loaded/entered at `$1000`, halting via `HALT` (with a `BR`
  safety net in case a simulator's `HALT` just resumes). `int` is 16
  bits; every value in flight lives in `R0`/`R1` (with `R2` for a
  computed address) — a CPU rich enough (eight 16-bit registers,
  register-indirect/autoincrement/autodecrement addressing, real EIS
  `MUL`/`DIV`) that almost everything maps onto one real instruction.
  Tested against an `open-simh` PDP-11 simulator; not run by `make test`
  (not wired into the Makefile's `test` rule — see `docs/building.md`).
- **pdp10** — real PDP-10 (KL10-class) assembly/machine code (own
  dialect; `Pdp10Assembler.h`), a bare flat binary loaded/entered at
  `010000` octal, halting via the real `JRST 4,0` ("HALT") instruction
  (with a `JRST` safety net in case a simulator's halt just resumes).
  `char`/`short`/`int` are all one native 36-bit word (not
  byte-addressable — the same convention as `pdp7`); `long` is 2 words,
  `long long` is 4. Sixteen real accumulators exist, and an ACn's
  register and the memory word at address n (0-17 octal) are the exact
  same storage — `AC1`/`AC2`/`AC3` hold the values in flight, `AC16`
  ("F") is the frame pointer and `AC17` ("P") is the real hardware
  stack-pointer accumulator: calls use the real `PUSHJ`/`POPJ`/`PUSH`/
  `POP` family, confirmed directly against the real simulator that
  `PUSH`/`PUSHJ` increment `P`'s address *before* storing — the real
  stack grows upward, the opposite of every byte-addressable target's
  software stack here, flipping every sign in the frame layout (a
  Param sits *below* the frame pointer, a Local *above* it). `*` uses
  the real double-length `MUL` (confirmed directly that the truncated
  single-length product lands in `AC+1`, not `AC`); `/`/`%` use the
  real single-length `IDIV` (confirmed directly to *not* take a
  double-length dividend, unlike `MUL`). There's no native unsigned
  compare or divide: an unsigned `<`/`<=`/`>`/`>=` splits into a
  same-sign-bit case (an ordinary signed `SUB` can't overflow) and a
  differing-sign-bit case (decided directly from which operand has the
  bit set, since a plain "flip the sign bit, then subtract" trick can
  itself overflow — confirmed by a real failure during development);
  unsigned `/`/`%` is a 36-bit restoring shift-subtract division loop,
  since the real double-length `DIV` turned out (confirmed directly)
  not to treat a zero-extended dividend as a plain unsigned 72-bit
  magnitude. Every branch/jump (`JRST`, the `JUMPx` family) takes a
  full 18-bit absolute address in one instruction, so there's no
  short-branch range limit and no `safeBranch()`-style trick. Objects
  use the ad hoc COFF machine ID `0x1010`. The real `pdp10` (open-simh
  V3.12-3) simulator on this machine crashes on any plain `d`/`e`
  <address> command (a stack-smashing write inside its own symbolic
  parser, confirmed with `lldb`) — only register access and its
  `load`/binary-file commands are unaffected — so
  `tests/run_tests.sh --target=pdp10` loads the compiled program as a
  classic RIM10B paper-tape boot block via `load -r` instead of
  depositing it word by word; `tests/pdp10/*.c` holds the pdp10-only
  cases. Not run by `make test`.
- **stm8** — real STM8 assembly/machine code (`STM8Assembler.h`), a bare
  flat binary loaded/entered at `$0100`. `int` is 16 bits; the STM8 has
  an 8-bit accumulator `a` and two 16-bit index registers (`x`/`y`), but
  an arbitrarily deep expression tree needs more operand slots than
  those provide, so every value in flight lives in one of five fixed
  two-byte memory cells (same convention as `8080`, with `x`/`y` as the
  transit registers). Every multi-byte cell is big-endian (`LDW` stores
  high byte first) — the only big-endian target here. No automated test
  yet.
- **m16c**/**m30626fjpfp** — real Renesas M16C assembly (`M16cAssembler.h`),
  a bare flat binary loaded/entered at `$0400`, one shared backend for the
  M16C/60 core and any chip built on it; `--target=m30626fjpfp` (the
  Renesas M30626FJPFP) is a software-identical alias, since the chip's
  real hardware differences (on-chip peripherals, memory size) are
  invisible to any instruction this compiler emits. `int` is 16 bits;
  values live in four 16-bit general registers (`R0`-`R3`) plus two
  dedicated address registers (`A0`/`A1`, never colliding with `R0`/`R1`),
  with a real hardware stack (`SP`, `PUSH.W`/`POP.W`/`JSR.W`/`RTS`) and
  real EIS-style `MUL.W`/`DIVU.W`. No automated test yet.
- **8080** — real Intel 8080 assembly/machine code (`I8080Assembler.h`),
  a bare flat binary loaded/entered at `$0100`. Values live in fixed
  memory cells (like 6502's zero-page pseudo-registers, but without the
  addressing-mode restriction, since `LHLD`/`SHLD` take any absolute
  address); the real hardware stack (`PUSH`/`POP`, `CALL`/`RET`) saves
  one operand across evaluating the other. No hardware multiply/divide
  and no overflow flag (`<`/`<=`/`>`/`>=` use a sign-bit bias trick
  instead of 6502/68000's N-xor-V fixup). Tested against a real,
  independent Z80 core (`pip install z80-python`) — valid since the Z80
  is object-code compatible with the 8080.
- **z80** — real Z80 assembly/machine code (`Z80Assembler.h`), its own
  independent backend (unlike the `8086`/`8088` alias) since it uses
  Z80-only instructions: real 16-bit `ADD HL,rr`/`SBC HL,rr`, direct
  `LD rr,(nn)` for BC/DE/SP, and register-targeted `RL r`/`RR r`. Same
  `z80-python` test setup as `8080`, run directly instead of as a
  stand-in. Also supports `--format=cmd` (see `docs/using.md`'s own
  `--format` row and `src/rt/trs80/` for a TRS-80 Model I/III runtime
  that writes straight to video RAM - no ROM/DOS call needed, since
  this backend's full `T*` pointer support reaches an absolute address
  directly). `--target=z80-coleco` is the same backend under a
  different `--target` name (same "one ISA, several platform
  conventions" pattern as `8086-dos`): a ColecoVision cartridge instead
  of a TRS-80 `/CMD` load module, linked by
  `src/linker/factory/ColecoLinkerFactory.cpp` against a BIOS-readable
  cartridge header (a `$55AA`/`$AA55` signature and pointer table, not
  a plain `call fn_main`) and written out via `--format=colecorom`
  (see `docs/using.md`'s own `--format` row and
  `src/fileformat/colecorom/ColecoRomFormat.h`). Its VDP/controller/
  sound hardware is entirely port-mapped (`IN`/`OUT`, not a memory
  address), which this backend's assembler didn't support until
  `src/rt/coleco/`'s own runtime library needed it - see
  `Z80Assembler.h`'s own header comment.
- **6809** — real Motorola 6809 assembly/machine code (`M6809Assembler.h`),
  a bare flat binary loaded/entered at `$3000` (12288). Big-endian, unlike
  every other target documented so far on this page. Values live in the
  same fixed memory cells (`ac`/`_bc`/`cx`/`dx`/`ptr`) as `z80`/`8080`,
  but a Local/Param's frame address is one `LEAX <off>,U` (`U` copied
  from `S` once per call) instead of `z80`'s multi-instruction push/pop-
  `ix` dance, since this chip has real indexed addressing `z80`/`8080`
  don't. Real `N`/`V` flags on `SUBD`/`CMPD` mean signed `<`/`<=`/`>`/`>=`
  need no sign-bit-bias trick either (unlike `z80`/`8080`). The real
  hardware `MUL` instruction is used directly for an
  `unsigned char * unsigned char` multiply; every other `*` still falls
  back to a shift-and-add loop (no 16x16 hardware multiply exists), and
  `/`/`%` are still repeated subtraction - no hardware divide, same as
  `z80`/`8080`. `long`/`long long` `*`/`/`/`%` are unsupported, same
  reasoning as `z80`/`8080`. Also supports `--format=decb` (see
  `docs/using.md`'s own `--format` row), the real CoCo `LOADM`/`CLOADM`
  binary-file convention, and `src/rt/coco/` is a TRS-80 Color Computer
  runtime that writes straight to the VDG's text-mode video RAM - unlike
  `z80`'s TRS-80 runtime, the VDG's character ROM doesn't use plain
  ASCII, so `coco_putc()` masks each code to 6 bits (`c & 0x3F`);
  lowercase and codes above `$5F` aren't supported by this runtime as a
  deliberate scope limitation, not a bug. No automated emulator-based
  test exists yet for this target or its CoCo runtime - see
  `docs/known-bugs.md`.
- **8008** — Intel's first microprocessor, the most primitive target
  here: no stack pointer or PUSH/POP (`CALL`/`RET` use a real but
  internal 7-level return stack — a hardware ceiling on call-nesting
  depth this backend can't extend), no 16-bit instructions, no
  direct-addressing load/store (every memory access goes through
  `H`,`L`), and rotates that only work on the accumulator. Saving one
  operand, and giving every Local/Param a per-call frame for recursion,
  both need a software stack this backend builds itself (a fixed RAM
  region plus its own frame-pointer/stack-pointer cells, locals at
  ascending addresses to avoid base-minus-index arithmetic). No PyPI
  8008 core exists, so `tests/run_tests.sh --target=8008` runs a small
  from-scratch emulator written for this, standard library only.
- **8051**/**8052**/**8031**/**8032** — real MCS-51 assembly/machine code
  (`MCS51Assembler.h`), one shared backend for all four part numbers
  (provably byte-identical output across them). A genuinely Harvard
  target: CODE and external DATA (`XDATA`, via `MOVX`+`DPTR`) are
  separate address spaces, so every variable lives in XDATA at a fixed
  compile-time address (no assembler symbol — the address is a plain
  immediate), while the five fixed scratch pseudo-registers live in
  internal RAM, reached by the accumulator-centric ALU. No hardware
  16-bit multiply/divide (`MUL AB`/`DIV AB` are 8x8→16/8÷8 only), so
  `*`/`/`/`%` use the same software loops as other memory-cell backends;
  comparisons use the same N-xor-V idiom as 6502/8080. No PyPI package
  models this Harvard split, so `tests/run_tests.sh --target=8051` runs
  a small from-scratch emulator, standard library only.
- **pic16** — real PIC16 mid-range (14-bit core) assembly for the
  classic PIC16F84A: its real SFR map, 68-byte GPR bank (0x0C-0x4F),
  11-bit direct `GOTO`/`CALL` range (well inside program memory
  0x000-0x3FF, so PCLATH paging is never needed), and real 8-level
  hardware call stack — a dedicated return-address-only stack, a real
  ceiling on call-nesting depth software can't extend. Every Local/Param
  still gets its own per-call frame (via `FSR`/`INDF` indirect
  addressing), so recursion computes correctly up to that depth.
  Genuinely Harvard: program memory (14-bit words) and data memory
  (byte-addressed file registers) are separate spaces tracked by two
  independent cursors (`org` for code, a private `orgram` for data).
  Verified by hand-decoding assembled/linked instruction words and
  relocations (no PIC16 simulator used); no automated test yet. Taking a
  function's address works, including across separate compilation
  (`--format=coff`) for an extern function; calling *through* one
  (`ir::ExprKind::CallPtr`) is a genuine hardware ceiling, not a missing
  feature — this core's call/return stack has no software-addressable
  storage (no `PUSH`/`POP`, unlike `pic18`), and `GOTO`/`CALL` only take
  an inline literal target, so there's no indirect call or jump of any
  kind on real PIC16 mid-range silicon.
- **pic18** — real PIC18 assembly for the classic PIC18F2420: real
  Access Bank SFR aliasing, a 16K-word program memory with a genuine
  20-bit absolute `GOTO`/`CALL` range (so, unlike `pic16`, no paging
  workaround is needed), and a real 31-level hardware call stack — the
  same kind of ceiling as `pic16`'s 8 levels, just deeper. Shares
  `pic16`'s Harvard split and fixed-pseudo-register conventions, plus a
  software `SSTACK`/`SP` push/pop stack for saving one operand across
  evaluating another. Verified the same way as `pic16` (hand-decoded
  instruction words/relocations, no live simulator); no automated test
  yet. Taking a function's address works the same as `pic16`, including
  across separate compilation for an extern function — taking the
  address of a function defined in the *same* file under separate
  compilation isn't supported yet (still a hard compile error, not a
  silently wrong value). Unlike `pic16`, calling through a function
  pointer does work: this core has no `CALLW`/indirect-call instruction,
  but it has the real mechanism actual PIC18 C compilers use instead —
  `PUSH` a stack slot, overwrite its `TOSU`/`TOSH`/`TOSL` bytes with the
  real return point, then load `PCLATU`/`PCLATH` and write `PCL` from the
  callee's address (the `PCL` write latches the upper bytes into `PC` and
  jumps — see `PIC18Codegen.cpp`'s `CallPtr` case).
- **stm32** — real Thumb-2 assembly for a real STM32 (ARMv7-M, Cortex-M3):
  a real 32-bit multiply (`muls`), a real signed hardware divide (`sdiv`
  — unlike every other target here, so `/`/`%` need no software loop), a
  real variable-count shift, real signed `cmp`+`Bcc`, and a real hardware
  call/return (`bl`/`push {lr}`+`pop {pc}`). Flash and RAM are genuinely
  separate address ranges (flash is read-only, confirmed against qemu's
  `stm32vldiscovery` model), so unlike every other backend's one flat
  blob, every global/local/temp/array lives in RAM while code lives in
  flash, with a real two-word Cortex-M reset vector table at the front
  (`--format=raw` is exactly what `qemu-system-arm -M stm32vldiscovery
  -kernel` boots).
- **i386** — real 32-bit protected-mode i386 assembly: `src/codegen/x86_8086`'s
  `8086` backend grown up one generation — the same register-centric
  model (`eax` the "ac", `ebx` the "other operand" and indirect-addressing
  register), the same real `imul`/`idiv`, `cmp`+conditional-jump, and
  `[label]`/`[ebx]`/`[label+ebx]` addressing, just 32 bits wide with a
  flat 4GB address space. Getting from power-on into protected mode is
  deliberately not this backend's job: `--format=raw`'s output is a real
  Multiboot image (the standard contract a bootloader like GRUB uses) —
  loading it via `qemu-system-i386 -kernel <file>` does the whole
  real-mode-to-protected-mode transition before this program's first
  instruction runs (confirmed against a real boot, reading `EAX`/`CS`
  over the monitor). The one thing this backend's own entry sequence
  still does is mask both real 8259 PICs (so `_ion()` never risks a
  triple fault) before calling `main`. This is also the only target with
  real **hosted** (real-OS) execution support — see `docs/using.md`'s
  `--target=i386-linux`/`i386-windows` and `docs/hosted-linux.md`.
- **armv7a** — real ARMv7-A A32 (32-bit ARM, not Thumb) assembly for an
  A-profile core such as the Cortex-A15: `r0`-`r12`, real `sp`/`lr`, a
  real hardware signed divide (`sdiv`, an optional ARMv7-A extension
  present on the qemu `cortex-a15` model this is tested against) plus
  `mls` for the remainder, and ARM's per-instruction condition field
  turning every comparison into one `cmp` + one conditionally-executed
  `mov` (`moveq`/`movlt`/etc.) with no branch at all. Tested against
  `qemu-system-arm -M none -cpu cortex-a15`.
- **armv8a** — real ARMv8-A A64 (64-bit AArch64) assembly for an
  A-profile core such as the Cortex-A53: the 32-bit `int` lives in
  `w0`-`w30` while addresses use the 64-bit `x0`-`x30`/`sp`/`x30`-as-`lr`
  view, relying on AArch64's architectural guarantee that every 32-bit op
  zero-extends into its 64-bit register (no explicit extension anywhere).
  Real `sdiv`+`msub` for `/`/`%`, real `cmp`+`cset<cc>` for comparisons,
  and `cbz`/`cbnz` drive `if`/`while` directly with no flags needed.
  Tested against `qemu-system-aarch64 -M virt -cpu cortex-a53`. The only
  other target with real hosted execution support — see
  `docs/using.md`'s `--target=armv8a-macos`.
- **cortex-m3**/**cortex-m4**/**cortex-m7** — real ARM Thumb/Thumb-2
  assembly for the ARMv7-M "mainline" M-profile architecture, one shared
  backend for all three part numbers. Every instruction emitted is real
  Thumb-1 or the handful of Thumb-2 32-bit forms genuinely needed
  (`MOVW`/`MOVT`, `BL`, a wide `B`), so it runs unchanged on any real
  ARMv7-M core; baseline M-profile (Cortex-M0/M0+/M23, ARMv6-M) isn't
  covered. A real register machine with a hardware stack and hardware
  multiply — `generate()` returns a complete program loaded at a fixed
  address, no vector table (the simulator sets PC directly, unlike
  `stm32`/`mc68328`'s real-vector-table choice). Tested against
  `qemu-system-arm -M mps2-an385`; not run by `make test`.
- **r10000**/**r4000** — MIPS assembly (own dialect; `MipsAssembler.h`),
  targeting the MIPS IV instruction set the R10000 implements;
  `--target=r4000` is a software-identical alias (this backend only ever
  emits real MIPS I instructions — a strict subset any MIPS I/II/III/IV
  core including a real R4000 already has). A real register machine with
  a hardware stack and hardware multiply/divide/shift, halting in a
  self-branch infinite loop. Verified under `qemu-system-mips -cpu R4000`
  (qemu ships no real R10000 model); tested against
  `qemu-system-mips -M malta -cpu 4Kc`, but not run by `make test`.
- **powerpc** — classic 32-bit PowerPC assembly (own dialect;
  `PowerPCAssembler.h`), the instruction set every G3/G4/G5 and embedded
  PowerPC core shares. A real register machine with a hardware stack and
  a single hardware link register — real `bl`/`blr`, real `r1` stack
  pointer, real `lis`/`ori` for loading a full 32-bit address (no single
  instruction holds one), real hardware multiply and `divw`-based
  divide. Verified under `qemu-system-ppc -M none -cpu 603`, including a
  deliberately crafted carry-overflow case exercising the `lis`/`ori`
  hi/lo split at link time. Tested against `qemu-system-ppc -M none -cpu
  G3`, but not run by `make test`.
- **sparc** — 32-bit SPARC V8 assembly (own dialect; `SparcAssembler.h`),
  the closest sibling to `powerpc`: another real-register,
  real-hardware-stack 32-bit RISC load/store target with no immediate
  wide enough for a full 32-bit address in one instruction
  (`sethi`/`or`, SPARC's 22-bit/10-bit split). Real SPARC has 32-entry
  *windowed* `%i`/`%l`/`%o` register banks tied to `save`/`restore`, but
  this backend deliberately doesn't use them (a real window only has a
  handful of entries, and a full one traps for the OS to spill — machinery
  this backend, running bare-metal with no trap table, doesn't have). So
  `%i`/`%l`/`%o`/`%g` stay plain, fixed register names, and recursion
  instead gets a conventional software stack frame (a dedicated `fp`
  register plus `sp`), bounded only by available stack, not window count.
  Every `call`/`retl`/branch synthesizes its own required delay-slot
  `nop`. Verified under `qemu-system-sparc -cpu TI-SuperSparc-II`, both
  whole-program and a real cross-object `--link`; no automated test yet.
- **vax** — 32-bit VAX-11/780 machine code (own operand dialect;
  `VaxAssembler.h`): little-endian, variable-length instructions built
  from an opcode byte plus one operand specifier per operand. The
  shared assembler grammar only admits identifier/number/parenthesized
  operands, so addressing modes are spelled `r0`..`r11`/`ap`/`fp`/`sp`,
  `(rN)`, `-(rN)`, `ai (rN)` (autoincrement), `D(rN)`, `imm V` (a
  short literal for 0..63, otherwise an operand-sized immediate; a
  symbol becomes a relocated 32-bit field) and `abs V` (mode `0x9F` plus
  a 32-bit address). Calls use the hardware `calls`/`ret` pair with a
  zero entry mask; arguments are pushed right to left and reached
  through `ap`, locals through `fp`, and results come back in `r0`
  (`r1:r0` for `long long`). Real VAX conditional branches only reach
  a signed byte, so `jbr` and `j<cc>` are fixed-size pseudo-branches
  that hop over a `brw`. There is no `and`, unsigned divide or
  logical right shift, so those become `mcoml`+`bicl`, a three-path
  `divl3`/`extzv` sequence and `extzv`; 64-bit arithmetic uses
  `adwc`/`sbwc`, `emul`, `ashq` and a bit-serial divide, with no
  helper calls. The link base is `0x4000`, the entry stub sets
  `sp` to `0x00100000`, and objects use the ad hoc COFF machine ID
  `0x5641`. Verified under SIMH's `vax780`; `tests/vax/*.c` holds the
  VAX-only cases, run alongside the shared corpus by
  `tests/run_tests.sh --target=vax`, and `tests/run_link_tests.sh
  --target=vax` covers cross-object linking. Not run by `make test`.
- **avr** — real ATmega328P machine code, scoped to that one chip within the AVR family (own
  operand dialect; `AVRAssembler.h`): 16-bit little-endian instruction words, two words for
  `call`/`jmp`/`lds`/`sts`. 32 KB flash (program memory) starting at the real reset vector
  address 0, and 2 KB SRAM (data memory) starting at 0x0100 - a genuine Harvard split, so every
  Global lives in SRAM (`ram`/`lds`/`sts`), never flash, with its initializer run as `sts` code
  once at program start rather than loaded statically (SRAM has no static contents). `int`/
  `short` are 16 bits (one register pair), matching the hardware's native word, not VAX/ARM's 32.
  Real registers throughout, not mcs51/stm8's software pseudo-register/frame-stack style: `r16:
  r17` ("ac") is `genExpr()`'s result for anything up to 2 bytes, `r18:r19` ("bc") the other
  operand, `r26:r27`/`r30:r31` (X/Z) address scratch, and `r28:r29` (Y) the frame pointer - a real
  hardware stack frame (`push`/`pop`, `in`/`out` on SPL/SPH), arguments pushed right to left and
  reached through Y past the saved caller-Y and return address, matching `ir::IR.h`'s own
  Param-offset convention. A `long`/`long long` is staged through four fixed 8-byte SRAM scratch
  buffers instead of splitting more of the 32 registers across every width. Hardware `mul`
  (unsigned 8x8->16) backs both a narrow (3-`mul` 16x16->16) and a wide (schoolbook) multiply; there
  is no hardware divide at any width, so `/`/`%` are a bit-serial restoring-division loop. Real
  `brxx` only reaches a 7-bit signed displacement, so every conditional branch goes through an
  inverted-short-branch-over-a-`jmp` idiom (`safeBranch()`, the same split `MCS51Codegen.cpp`
  uses, not a `j<cc>` assembler pseudo-op). The link base is flash address 0, the entry stub sets
  `SPL`/`SPH` to RAMEND, and objects use the ad hoc COFF machine ID `0x4156`. A Global actually
  *defined* (with an initializer) in an object other than the one with `main` is never initialized
  under real separate compilation (SRAM's own lack of static contents, cross-object - see
  `AVRCodegen.h`'s own header comment); no Harvard-data target here is wired into
  `tests/run_link_tests.sh` for the same reason, until now. Verified under
  `qemu-system-avr -M uno` (a real ATmega328P core, not a from-scratch interpreter);
  `tests/avr/*.c` holds the AVR-only cases, run alongside the shared corpus by
  `tests/run_tests.sh --target=avr`, and `tests/run_link_tests.sh --target=avr` covers
  cross-object linking. Not run by `make test`.
- **alpha** — 64-bit DEC Alpha AXP machine code (own operand dialect;
  `AlphaAssembler.h`): little-endian, fixed-width instruction words built
  from Memory, Jump, Branch and Operate formats, every encoding a real
  Alpha opcode/function-field bit pattern. Unlike every 32-bit-register
  backend here, `long`/`long long` are the real, native 64-bit quadword -
  one register, not a word pair - so wide arithmetic needs no cross-word
  carry juggling at all; only narrowing a result back down to `int` (or
  narrower) needs extra care, since Alpha's own arithmetic/shift
  instructions are always 64-bit. There is no absolute-addressing memory
  operand (`lda`/`ldah` build an address a register at a time) and no
  integer divide instruction at all - a genuine, well-known omission from
  the real ISA - so `/`/`%` always run a bit-serial restoring-division
  loop, for both `int` and `long long`. Calls go through `jsr`/`ret` (the
  real Alpha calling convention's `pv`/`t12` register holds the callee's
  address), arguments are pushed right to left and reached through `fp`,
  and comparisons produce a 0/1 boolean directly (`cmpeq`/`cmplt`/`cmple`
  and their unsigned `cmpult`/`cmpule` counterparts) rather than setting
  any condition-code register - Alpha has none. The link base is
  `0x00120000`, the entry stub sets `sp` to `0x00200000`, and objects use
  the real historical Windows NT/Alpha COFF machine ID `0x184`. No SIMH or
  qemu target here can run bare Alpha code without a real PALcode image
  (confirmed directly against `qemu-system-alpha`), so
  `tests/run_tests.sh --target=alpha`/`tests/run_link_tests.sh
  --target=alpha` instead drive a small from-scratch Alpha core, the same
  idea as `8008`'s own; `tests/alpha/*.c` holds the Alpha-only cases. Not
  run by `make test`.
- **saturn** — a deliberately simplified, byte-addressed model of the HP
  Saturn (the 4-bit/nibble-oriented CPU in the HP48/HP49 calculator
  series), not a real nibble-accurate one: real nibble-level addressing
  and bit-packed instruction encoding would need a bespoke sub-byte
  relocation scheme this project's linker has no support for, so this
  backend instead models the chip at the word level pdp7's own backend
  already proved out, just byte- rather than word-addressed. Five
  Saturn-style registers - `a`/`c`/`b` (general-purpose, real Saturn
  register letters) and `d0`/`d1` (real Saturn address registers, used
  here as this backend's stack pointer and frame pointer) - plus
  mnemonics evoking real Saturn assembly (`goto`/`gosbvl`/`rtn`,
  real-register names); the actual encoding (`SaturnAssembler.cpp`) is
  this backend's own invention, not real Saturn nibble-level machine
  code. Real Saturn hardware has no multiply or divide instruction, so
  both are software routines (`saturnRuntimeAsm()`'s `__mul32`/
  `__udivmod32`) called like any other function. `char`/`short` get their
  own narrower `ldb`/`ldh`/`stb`/`sth` access; `int`/pointers use a plain
  4-byte `ld`/`st`; `long`/`long long` are handled 4 bytes at a time, with
  carry/borrow recovered via an unsigned-wraparound `cmp`+`jb` test (no
  hardware carry flag to chain through otherwise). `'@'` fixed placement
  isn't supported (a compile error, not silent corruption) - the shared
  `place.c`/`identical_code_folding_addr_exclusion.c` tests are skipped
  for this target. The link base is `0x1000`, the entry stub sets `d0`
  to `0xF000`, and objects use the ad hoc COFF machine ID `0x5A74`; the
  linked-output format name is `raw`, the same shared `RawFormat` writer
  name used for the single-file compile format. No real or
  mainstream Saturn emulator exists for headless, scriptable register
  checks (x48/emu48 are interactive HP48/HP49 calculator emulators, not
  bare Saturn cores) - and this backend's own dialect is this project's
  invention anyway - so `tests/run_tests.sh --target=saturn` drives a
  small from-scratch interpreter written just for it, the same idea as
  `8008`'s/`alpha`'s own. Not run by `make test`.
- **tms320c6713** — TMS320C6713 (TI C67x VLIW DSP) machine code (own
  operand dialect; `Tms320C6713Assembler.h`), deliberately using none of
  the real chip's VLIW packing or dual-register-file/cross-path reads:
  every instruction ends its own execute packet and only ever touches
  the B register file (B0..B15), the same kind of always-correct
  simplification `SparcCodegen`/`PowerPCCodegen` already make for their
  own targets, not an invented encoding. B0 is `genExpr()`'s result and
  also the sole predicate register for every conditional branch; B1/B2/
  B4..B12 are further scratch; B13 permanently holds 0 (materialized
  once at entry, matching Alpha's own hardwired `zero` idiom); B14 is
  the frame pointer, B15 the hardware stack pointer (the real TI C6000
  ABI's own role for both), and B3 the link/return-address register
  (also the real ABI's own role). Real pipeline hazards are modeled with
  explicit NOP padding rather than any instruction scheduling - every
  branch is followed by 5 NOPs, every load by 4, every multiply by 1 -
  since there's no native 32x32->32 multiply (only 16x16->32 `MPY`/
  `MPYH`/`MPYHL`/`MPYLH`) or divide instruction either, so both `long`
  arithmetic and `/`/`%` run software sequences. This is a first pass
  with no relocatable object format at all (`Tms320C6713LinkerFactory`
  produces no output formats) - only a whole-program flat `raw` binary,
  so there's no `--link`/separate-compilation support and no
  `tests/run_link_tests.sh --target=tms320c6713`. The link base is
  `0x00020000`, the entry stub sets `B15` to `0x00100000`, and objects
  use the ad hoc COFF machine ID `0x6713` (moot today, since nothing
  ever reads a tms320c6713 object back in). No mainstream standalone
  C6713 simulator suits headless, scriptable register checks (TI's own
  Code Composer Studio simulator is a full interactive IDE, not a
  scriptable bare core), and this backend's instruction stream depends
  on those same real pipeline hazards no generic tool would model the
  way this project's own codegen expects anyway, so
  `tests/run_tests.sh --target=tms320c6713` instead drives a small
  from-scratch C67x core written just for this, decoding exactly the
  instruction words `Tms320C6713Assembler.cpp` ever emits and modeling
  the branch/load/multiply delay slots faithfully; `tests/tms320c6713/
  *.c` holds the C6713-only cases. Standard library only, nothing to
  install. Not run by `make test`.

Every global variable is a single statically allocated word; every Local
and Param lives in a per-call frame that each backend builds on whatever
stack (hardware or software) its target provides, so direct and
indirect/mutual recursion compute correctly everywhere — bounded by that
backend's own real limit (available memory for a software stack, or a
fixed-depth hardware call stack on `8008`/`pic16`/`pic18` that can't be
extended).

The same C subset compiles to every target, each predefining its own
macro for `#ifdef`-gated code (an alias target defines both its base
target's macro and its own): `__PDP7__`, `__8086__` (also for `8088`/`8086-dos`,
plus `__MSDOS__` for `8086-dos`),
`__6502__` (also for `65c02`, plus `__65C02__`), `__68000__` (also for
`mc68328`, plus `__MC68328__`), `__PDP11__`, `__PDP10__`, `__STM8__`, `__M16C__` (also
for `m30626fjpfp`, plus `__M30626FJPFP__`), `__8080__`, `__Z80__`,
`__8008__`, `__MCS51__` (also for
`8051`/`8052`/`8031`/`8032`, plus each one's own `__8051__`/`__8052__`/
`__8031__`/`__8032__`), `__PIC16__`, `__PIC18__`, `__STM32__`, `__I386__`,
`__ARMV7A__`, `__ARMV8A__` (also `__aarch64__`), `__CORTEX_M3__` (also
for `cortex-m4`/`cortex-m7`, plus `__CORTEX_M4__`/`__CORTEX_M7__`),
`__R10000__` (also for `r4000`, plus `__R4000__`), `__POWERPC__` (also
`__ppc__`), `__SPARC__` (also `__sparc__`), `__VAX__` (also `__vax__`), `__AVR__` (also
`__AVR_ATmega328P__`), `__ALPHA__` (also
`__alpha__`), `__SATURN__` — see `src/main.cpp`'s `makeBackend()` comment.
