# Supported platforms

`pcc` is a compiler and linker for a small subset of C, targeting real
retro hardware through pluggable backends. Today it supports:

- **pdp7** — PDP-7 assembly, assembled and linked into real PDP-7 binary
  formats (bootable paper tape, etc.), runnable under the `open-simh`
  PDP-7 simulator.
- **8086** — 16-bit real-mode 8086 assembly (NASM syntax), producing a
  bootable 512-byte boot-sector binary runnable under `qemu-system-i386`,
  or (`--format=elf`) a minimal flat ELF executable — same single-blob
  model as every other format here (no section/symbol tables), and
  incomplete the way `--format=aout` is: a real ELF loader expects
  32-bit protected-mode code, not 16-bit real mode, so this is for
  tooling that just needs to recognize the file as ELF.
- **8088** — a deliberate, software-identical alias for `8086`: the
  8088's one real difference (an 8-bit external data bus) is invisible
  to every instruction this compiler could emit, so `--target=8088`
  reuses the 8086 backend outright.
- **6502** — 6502 assembly (own dialect; `src/codegen/m6502/M6502Assembler.h`),
  a bare flat binary loaded/entered at `$0600`, no OS/I/O conventions, on
  a CPU with three 8-bit registers, no hardware multiply/divide, and
  branches limited to ±127 bytes (see `M6502Codegen.h`'s header comment
  for the workarounds, including recursion: the real stack is only used
  for `JSR`/`RTS`; every Local/Param lives in a software frame reached
  through a zero-page frame pointer). `tests/run_tests.sh --target=6502`
  runs against a real 6502 core via `py65` (`pip install py65`).
- **65c02** — WDC 65C02, same dialect/platform choices as `6502` plus one
  real CMOS-only instruction: zeroing a zero-page pseudo-register uses
  `STZ` directly instead of `LDA #0`/`STA` (NMOS 6502 has no "store zero").
  Tested against `py65`'s separate CMOS core
  (`py65.devices.mpu65c02.MPU`).
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
- **stm8** — real STM8 assembly/machine code (`STM8Assembler.h`), a bare
  flat binary loaded/entered at `$0100`. `int` is 16 bits; the STM8 has
  an 8-bit accumulator `a` and two 16-bit index registers (`x`/`y`), but
  an arbitrarily deep expression tree needs more operand slots than
  those provide, so every value in flight lives in one of five fixed
  two-byte memory cells (same convention as `8080`, with `x`/`y` as the
  transit registers). Every multi-byte cell is big-endian (`LDW` stores
  high byte first) — the only big-endian target here. No automated test
  yet.
- **m16c** — real Renesas M16C assembly (`M16cAssembler.h`), a bare flat
  binary loaded/entered at `$0400`. `int` is 16 bits; values live in four
  16-bit general registers (`R0`-`R3`) plus two dedicated address
  registers (`A0`/`A1`, never colliding with `R0`/`R1`), with a real
  hardware stack (`SP`, `PUSH.W`/`POP.W`/`JSR.W`/`RTS`) and real EIS-style
  `MUL.W`/`DIVU.W`. No automated test yet.
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
  stand-in.
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

Every global variable is a single statically allocated word; every Local
and Param lives in a per-call frame that each backend builds on whatever
stack (hardware or software) its target provides, so direct and
indirect/mutual recursion compute correctly everywhere — bounded by that
backend's own real limit (available memory for a software stack, or a
fixed-depth hardware call stack on `8008`/`pic16`/`pic18` that can't be
extended).

The same C subset compiles to every target, each predefining its own
macro for `#ifdef`-gated code (an alias target defines both its base
target's macro and its own): `__PDP7__`, `__8086__` (also for `8088`),
`__6502__` (also for `65c02`, plus `__65C02__`), `__68000__` (also for
`mc68328`, plus `__MC68328__`), `__PDP11__`, `__STM8__`, `__M16C__`,
`__8080__`, `__Z80__`, `__8008__`, `__MCS51__` (also for
`8051`/`8052`/`8031`/`8032`, plus each one's own `__8051__`/`__8052__`/
`__8031__`/`__8032__`), `__PIC16__`, `__PIC18__`, `__STM32__`, `__I386__`,
`__ARMV7A__`, `__ARMV8A__` (also `__aarch64__`), `__CORTEX_M3__` (also
for `cortex-m4`/`cortex-m7`, plus `__CORTEX_M4__`/`__CORTEX_M7__`),
`__R10000__` (also for `r4000`, plus `__R4000__`), `__POWERPC__` (also
`__ppc__`), `__SPARC__` (also `__sparc__`) — see `src/main.cpp`'s
`makeBackend()` comment.
