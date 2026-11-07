# Supported platforms

`pcc` is a compiler and linker for a small subset of C, targeting real
retro hardware through pluggable backends. Today it supports:

- **pdp7** — PDP-7 assembly, assembled and linked into real PDP-7
  binary formats (bootable paper tape, etc.) runnable under the
  `open-simh` PDP-7 simulator.
- **8086** — 16-bit real-mode 8086 assembly (NASM syntax), producing
  a bootable 512-byte boot-sector binary runnable directly under
  `qemu-system-i386`, or (`--format=elf`, both compile and link mode)
  a minimal flat ELF executable — same single-flat-blob model as
  every other format here (no section/symbol tables), and honestly
  incomplete the way `--format=aout` already is: a real ELF loader
  expects 32-bit protected-mode code, not 16-bit real mode, so this
  is for tooling that merely needs to recognize the file as ELF, not
  a claim that it's runnable by anything other than pcc's own
  `--format=raw` convention.
- **8088** — a deliberate, software-identical alias for `8086`, not a
  separate backend: the 8088 is the exact same instruction set as the
  8086 (its one real difference, an 8-bit external data bus instead
  of a 16-bit one, is a hardware bus-timing detail invisible to every
  instruction this compiler could ever emit), so `--target=8088`
  reuses the 8086 backend outright.
- **6502** — 6502 assembly (this project's own dialect - see
  `src/codegen/m6502/M6502Assembler.h`), producing a bare flat binary
  loaded and entered at a fixed address ($0600), with no OS or I/O
  conventions, on a CPU with only three 8-bit registers, no
  hardware multiply/divide, and branches that can only reach ±127
  bytes (see `src/codegen/m6502/M6502Codegen.h`'s own header comment
  for how each of those, plus recursion, gets worked around - the
  real hardware stack is only used for `JSR`/`RTS`; every Local/Param
  lives in a software frame reached through a zero-page frame
  pointer, since this CPU has no base+displacement addressing mode of
  its own). `tests/run_tests.sh --target=6502` runs its own test
  suite against a real 6502 core via
  the `py65` package (`pip install py65`) - during development this
  was additionally cross-checked against a from-scratch pure-Python
  6502 emulator written just for that (not part of this repo).
- **65c02** — WDC 65C02 assembly, the exact same dialect/platform
  choices as `6502` above (see `src/codegen/m65c02/M65C02Assembler.h`)
  plus exactly one real CMOS-only instruction: zeroing a zero-page
  pseudo-register goes through `STZ` directly instead of the plain
  6502 backend's own "LDA #0 / STA" (a real NMOS 6502 has no "store
  zero" opcode at all) - see `src/codegen/m65c02/M65C02Codegen.h`'s
  own header comment. `tests/run_tests.sh --target=65c02` runs the
  same test suite against `py65`'s own separate CMOS core
  (`py65.devices.mpu65c02.MPU`, part of the same `py65` package as
  `6502` above) rather than its NMOS one, so the one real STZ this
  backend emits is actually checked against real 65C02 semantics.
- **68000** — 68000 assembly (this project's own dialect - see
  `src/codegen/m68k/M68kAssembler.h`), producing a bare flat binary
  the same way the 6502 backend does (loaded and entered at a fixed
  address, `$1000`, with no OS/boot ROM/vector table involved at all -
  the test harness sets the simulated CPU's PC directly). Unlike the
  6502, this target is rich enough that almost everything maps onto a
  real instruction directly: real `MULS.W`/`DIVS.W` (the *original*
  68000's only multiply/divide - 16-bit operands even though this
  backend's own `int` is 32 bits, a narrower documented limitation
  than other backends' own non-negative-only software multiply/
  divide - see `src/codegen/m68k/M68kCodegen.h`'s own header comment),
  a real variable-count shift, real signed `CMP.L`+`Bcc` (no
  sign/overflow fixup needed the way the 6502 backend's own
  comparisons do), and a real hardware stack (`A7`, real `JSR`/`RTS`)
  instead of pdp7/6502-style synthesized calls. `tests/run_tests.sh
  --target=68000` runs its own test suite against a real 68000 core
  via `qemu-system-m68k -cpu m68000` (part of the same `qemu` package
  already used for `8086`/`8088`).
- **mc68328** — the real Motorola/Freescale MC68328 "DragonBall" SoC
  (the chip at the heart of the original Palm Pilot and several other
  mid-1990s PDAs - see `src/codegen/mc68328/MC68328Codegen.h`'s own
  header comment) - a real MC68EC000 core, bit-for-bit the same
  user/supervisor instruction set as plain `68000` above, so every
  instruction-selection choice (`D0`/`D1`/`A0` pseudo-register roles,
  `MULS.W`/`DIVS.W`, `LSL.L`/`LSR.L`, signed `CMP.L`+`Bcc`) is
  identical to that backend's own. The one real difference: this
  backend lays out a real two-word reset vector table at address 0
  (`dl kInitialSP` / `dl _start`) instead of leaning on `68000`'s own
  simulator-only PC-forcing convenience, because real DragonBall
  silicon genuinely needs one - chip-select A is based at address 0
  at reset specifically so the CPU's own hardware reset exception can
  fetch its initial `SSP`/`PC` from vectors 0/1 there, the standard
  68000-family reset mechanism. `tests/run_tests.sh --target=mc68328`
  runs its own test suite against `qemu-system-m68k -M none` with no
  PC-forcing trick at all, relying on that real vector table to start
  execution, confirming it actually works the way real silicon would.
- **pdp11** — PDP-11 assembly (this project's own dialect - see
  `src/codegen/pdp11/Pdp11Assembler.h`), producing a bare flat binary
  loaded and entered at a fixed address (`$1000`), halting via the
  real `HALT` instruction (with a `BR` safety net right after it, in
  case a given simulator's own `HALT` just resumes at the next word
  instead of actually stopping). This language's own `int` is 16
  bits, the PDP-11's native word size; every value in flight lives in
  `R0` (genExpr()'s own result) and `R1` ("the other operand"), with
  `R2` holding a computed address when dereferencing one - like
  `68000`/`m16c`, a CPU rich enough (eight real 16-bit general
  registers, genuine register-indirect/autoincrement/autodecrement
  addressing, real EIS `MUL`/`DIV` hardware multiply/divide) that
  almost everything maps onto one real instruction rather than a
  named memory cell. `tests/run_tests.sh --target=pdp11` runs its own
  test suite against an `open-simh` PDP-11 simulator (`pdp11`),
  available the same way the `pdp7` one is; `make test` doesn't run
  it, since `pdp11` isn't one of the targets wired into the Makefile's
  own `test` rule (see `docs/building.md`).
- **stm8** — real STM8 assembly and machine code (see
  `src/codegen/stm8/STM8Assembler.h`), producing a bare flat binary
  loaded and entered at a fixed address (`$0100`), the same "bare
  flat binary" platform choice as `6502`/`8080`. This language's own
  `int` is 16 bits; the STM8 has a real 8-bit accumulator `a` and two
  real 16-bit index registers (`x`/`y`), but an arbitrarily deep
  expression tree still needs more "other operand" slots than those
  two alone can provide, so every value in flight lives in one of
  five fixed, two-byte memory cells - the same convention the `8080`
  backend uses, just with `x`/`y` as the transit registers instead of
  `HL`. Every multi-byte cell is declared high-byte-first: the STM8
  is a big-endian machine (`LDW` stores a word's high byte at the
  lower address), unlike every little-endian target here. No
  automated `tests/run_tests.sh` target yet.
- **m16c** — real Renesas M16C assembly (see
  `src/codegen/m16c/M16cAssembler.h`), producing a bare flat binary
  loaded and entered at a fixed address (`$0400`). This language's
  own `int` is 16 bits; every value in flight lives in one of four
  real 16-bit general registers (`R0`-`R3`) plus two dedicated
  address registers (`A0`/`A1` - never colliding with `R0`/`R1` the
  way the pdp11 backend's own general-purpose `R2` can), with a
  genuine hardware stack (`SP`, real `PUSH.W`/`POP.W`/`JSR.W`/`RTS`)
  and real EIS-style `MUL.W`/`DIVU.W` hardware multiply/divide - like
  `68000`/`pdp11`, almost everything maps onto one real instruction.
  No automated `tests/run_tests.sh` target yet.
- **8080** — real, standard Intel 8080 assembly and machine code (see
  `src/codegen/i8080/I8080Assembler.h`), producing a bare flat binary
  loaded and entered at a fixed address (`$0100`). Every value in
  flight lives in a handful of fixed memory cells (same idea as the
  6502's own zero-page pseudo-registers, but without that CPU's
  addressing-mode restriction, since 8080 `LHLD`/`SHLD` take any
  absolute address), with the real hardware stack (`PUSH`/`POP`,
  `CALL`/`RET`) used to save one operand across evaluating the other.
  No hardware multiply/divide and no overflow flag (`<`/`<=`/`>`/`>=`
  use the sign-bit bias trick instead of the 6502/68000's N-xor-V
  fixup — see `src/codegen/i8080/I8080Codegen.h`'s own header
  comment). `tests/run_tests.sh --target=8080` runs its own test
  suite against a real, independent Z80 core (`pip install
  z80-python`) — valid because the Z80 is object-code compatible
  with the 8080.
- **z80** — real, standard Z80 assembly and machine code (see
  `src/codegen/z80/Z80Assembler.h`), its own independent backend
  rather than a software alias of `8080` (unlike `8088`/`8086`),
  since it actually uses Z80-only instructions the 8080 doesn't have:
  real 16-bit `ADD HL,rr`/`SBC HL,rr` (so `-` and every comparison are
  one instruction instead of a byte-wise chain), direct `LD rr,(nn)`
  for BC/DE/SP (not just HL), and register-targeted `RL r`/`RR r`
  (shifting any register directly, not just the accumulator). Same
  `tests/run_tests.sh --target=z80` / `z80-python` test setup as
  `8080`, just run directly instead of as a stand-in.
- **8008** — Intel's first microprocessor, and the most primitive
  target here by a wide margin (see `src/codegen/i8008/I8008Codegen.h`'s
  own header comment): no stack pointer or PUSH/POP at all (`CALL`/
  `RET` use a real but internal, software-invisible 7-level return
  stack - a real hardware ceiling on call-nesting depth, recursive or
  not, that this backend can't extend), no 16-bit instructions of any
  kind, no direct-addressing load/store (every memory access goes
  through `H`,`L`), a different register-code numbering than every
  later chip in this family, and rotate instructions that only ever
  work on the accumulator. Saving one operand across evaluating
  another, and giving every Local/Param its own per-call frame for
  recursion, therefore both need a real *software* stack this backend
  builds for itself (a fixed RAM region plus its own runtime frame-
  pointer/stack-pointer cells, with locals laid out at ascending
  addresses so indexing a local array never needs the base-minus-
  index arithmetic a descending frame would), since there's no
  hardware one to use beyond the 7-level call stack. No PyPI package
  exists for an 8008 core (it isn't
  object-code compatible with anything later), so
  `tests/run_tests.sh --target=8008` runs a small from-scratch 8008
  emulator written just for this, standard library only.
- **8051**/**8052**/**8031**/**8032** — real, standard MCS-51
  assembly and machine code (see
  `src/codegen/mcs51/MCS51Assembler.h`), one shared backend for all
  four part numbers (`--target=8052`/`8031`/`8032` produce provably
  byte-identical output to `--target=8051` - same "software-identical
  part numbers" reasoning as the `8088`/`8086` alias above; see
  `src/main.cpp`'s own `makeBackend()` comment). A genuinely Harvard
  target: CODE (fetched by `PC`) and external DATA (`XDATA`, reached
  only via `MOVX`+`DPTR`) are two distinct address spaces, so every
  ordinary variable lives in XDATA at a fixed, compile-time-assigned
  address (there's no assembler symbol for one at all - the address
  is a plain immediate baked in by the codegen itself), while the
  five fixed scratch pseudo-registers (same idea as the 6502/8080
  backends' own) live in the 128-byte-or-more internal RAM every one
  of the four parts guarantees, reached directly by the real
  accumulator-centric ALU (`ADD`/`SUBB`/`ANL`/... `A,direct`). No
  hardware 16-bit multiply/divide (`MUL AB`/`DIV AB` are 8x8->16/8÷8
  only), so `*`/`/`/`%` use the same software loops every other
  memory-cell-based backend here does; `<`/`<=`/`>`/`>=` use the same
  N-xor-V idiom as 6502/8080, via the real `ACC.7` sign bit and
  `PSW.OV`. No PyPI package models this target's own Harvard split,
  so `tests/run_tests.sh --target=8051` runs a small from-scratch
  MCS-51 emulator written just for this, standard library only.
- **pic16** — real PIC16 mid-range (14-bit core) assembly for a
  concrete real chip, the classic PIC16F84A (see
  `src/codegen/pic16/PIC16Codegen.h`'s own header comment): its real
  SFR map, its real 68-byte GPR file-register bank (0x0C-0x4F), its
  real 11-bit direct `GOTO`/`CALL` range (program memory 0x000-0x3FF,
  well inside that range so PCLATH paging is never needed), and its
  real 8-level hardware call stack - a dedicated return-address-only
  stack, not ordinary addressable memory, so it's a real ceiling on
  call-nesting depth (recursive or not) that software can't extend.
  Every Local/Param still gets its own per-call frame (via this
  chip's `FSR`/`INDF` indirect addressing), so recursion computes
  correctly up to that depth. Genuinely Harvard: program memory
  (14-bit words) and data memory (byte-addressed file registers) are
  two separate address spaces tracked by two independent cursors in
  this backend's own in-process assembler (`org` for code, a private
  `orgram` for data) - unlike every unified-memory backend here, a
  label can never mean both "an address to load from" and "a place
  code lives" at once. `generate()` starts at the real hardware reset
  vector (0x0000). Verified by hand-decoding the assembled/linked
  instruction words and relocations directly (no PIC16 instruction-
  level simulator was used); no automated `tests/run_tests.sh` target
  yet. Taking a function's address works (`genAddrOf`'s `Func`
  branch), but calling *through* one (`ir::ExprKind::CallPtr`) is a
  genuine hardware ceiling, not a missing feature: this core's one
  hardware call/return stack has no software-addressable storage at
  all (no `PUSH`/`POP`, unlike `pic18`'s own), and `GOTO`/`CALL` only
  ever take an inline literal target - there is no indirect call or
  jump of any kind on real PIC16 mid-range silicon, so `genExpr`'s
  switch has no case for it and never will.
- **pic18** — real PIC18 assembly for a concrete real chip, the
  classic PIC18F2420 (see `src/codegen/pic18/PIC18Codegen.h`'s own
  header comment): real Access Bank SFR aliasing, a real 16K-word
  program memory with a genuine 20-bit absolute `GOTO`/`CALL` range
  (so, unlike `pic16`, no paging workaround is ever needed), and a
  real 31-level hardware call stack - the same kind of real,
  software-unextendable ceiling on call-nesting depth as `pic16`'s
  own 8 levels, just deeper; every Local/Param gets its own per-call
  frame (via this chip's more capable `FSR`/`INDF` auto-increment/
  decrement indirect addressing), so recursion computes correctly up
  to that depth. Shares `pic16`'s own Harvard
  code/data split and fixed-pseudo-register conventions, plus a real
  software `SSTACK`/`SP` push/pop stack for saving one operand across
  evaluating another (a compile-time-indexed fixed cell would be
  unsound here, since a nested call reuses the same cell at runtime).
  `generate()` starts at the real hardware reset vector (0x000000).
  Verified the same way as `pic16` (hand-decoded instruction words and
  relocations, no live simulator); no automated `tests/run_tests.sh`
  target yet. Unlike `pic16`, calling through a function pointer
  (`ir::ExprKind::CallPtr`) does work here - this classic (non-XINST)
  core has no `CALLW` and no single indirect-call instruction either,
  but it does have the real mechanism actual PIC18 C compilers use
  instead: `PUSH` a stack slot, overwrite its `TOSU`/`TOSH`/`TOSL`
  bytes with the real desired return point (a label the assembler
  resolves, never a hand-counted offset), then load `PCLATU`/`PCLATH`
  and write `PCL` from the callee's own computed address - the write
  to `PCL` is what actually latches the upper bytes into the real `PC`
  and jumps (see `PIC18Codegen.cpp`'s own `CallPtr` case).
- **stm32** — real Thumb-2 assembly for a real STM32 (ARMv7-M,
  Cortex-M3 - see `src/codegen/stm32/STM32Codegen.h`'s own header
  comment), the "rich register machine" case here: a real 32-bit
  multiply (`muls`), a real signed hardware divide (`sdiv` - this
  CPU, unlike every other target here, genuinely has one, so `/`/`%`
  need no software loop), a real variable-count shift (`lsl`/`lsr`),
  real signed `cmp`+`Bcc`, and a real hardware call/return
  (`bl`/`push {lr}`+`pop {pc}`). The one real wrinkle: flash and RAM
  are two genuinely separate address ranges on real hardware (flash
  is actually read-only - confirmed directly against qemu's own
  `stm32vldiscovery` board model), so unlike every other backend's
  one flat blob, every global/local/temp/array slot lives in RAM
  (reserved, not baked in) while code lives in flash, with a real
  two-word Cortex-M reset vector table at the front (`--format=raw`'s
  own output is exactly what `qemu-system-arm -M stm32vldiscovery
  -kernel` boots). `tests/run_tests.sh --target=stm32` runs its own
  test suite against that real board model, polling a register over
  qemu's own monitor the same way the `68000` target's own check
  does.
- **i386** — real 32-bit protected-mode i386 assembly (see
  `src/codegen/i386/I386Codegen.h`'s own header comment) -
  `src/codegen/x86_8086`'s own `8086` backend grown up one generation:
  the same register-centric model (`eax` is every other backend's
  "ac", `ebx` the "other operand" in a binary op *and* the one
  indirect-addressing register), the same real `imul`/`idiv`,
  `cmp`+conditional-jump, and `[label]`/`[ebx]`/`[label+ebx]`
  addressing - just 32 bits wide throughout and a real flat 4GB
  address space instead of 16-bit real-mode segmentation. Getting
  from power-on into 32-bit protected mode at all is deliberately
  *not* this backend's job: `--format=raw`'s own output is a real
  Multiboot image (the standard contract a real bootloader like GRUB
  uses to load a 32-bit kernel) - a small, standard header plus this
  backend's own ordinary 32-bit code+data, nothing else. Loading it
  (`qemu-system-i386 -kernel <file>`, acting as that bootloader) does
  the whole real-mode-to-protected-mode transition before this
  program's own first instruction ever runs - confirmed directly
  against a real `qemu-system-i386` boot (reading `EAX`/`CS` over the
  monitor) rather than assumed from the spec alone. The one thing this
  backend's own entry sequence still does for itself is mask both real
  8259 PICs (so this language's `_ion()` never risks a triple fault
  with no real IDT behind it) before calling `main`.
  `tests/run_tests.sh --target=i386` loads that same Multiboot image
  directly via `-kernel` (no floppy image, no BIOS), polling the full
  32-bit `EAX` over qemu's own monitor.
- **armv7a** — real ARMv7-A A32 (32-bit ARM, not Thumb) assembly for
  an A-profile core such as the Cortex-A15 (see
  `src/codegen/arm/ArmCodegen.h`'s own header comment): `r0`-`r12`
  general registers, real `sp`/`lr`, a real hardware signed divide
  (`sdiv`, an optional ARMv7-A extension present on the qemu
  `cortex-a15` model this backend is tested against) plus `mls` for
  the remainder, and ARM's per-instruction condition field turning
  every comparison into one `cmp` + one conditionally-executed `mov`
  (`moveq`/`movlt`/etc.) with no branch at all. `tests/run_tests.sh
  --target=armv7a` runs its test suite against real
  `qemu-system-arm -M none -cpu cortex-a15`, the same PC-forcing
  `loader`-device trick the `68000` target's own check uses.
- **armv8a** — real ARMv8-A A64 (64-bit AArch64) assembly for an
  A-profile core such as the Cortex-A53 (see
  `src/codegen/arm64/Arm64Codegen.h`'s own header comment): this
  language's 32-bit `int` lives in the `w0`-`w30` registers while
  addresses use the real 64-bit `x0`-`x30`/`sp`/`x30`-as-`lr` view,
  relying on AArch64's own architectural guarantee that every 32-bit
  op zero-extends into its 64-bit register view - no explicit
  extension instructions anywhere. Real `sdiv`+`msub` for `/`/`%`,
  real `cmp`+`cset<cc>` for comparisons, and `cbz`/`cbnz` drive
  `if`/`while` directly with no flags needed at all.
  `tests/run_tests.sh --target=armv8a` runs its test suite against
  real `qemu-system-aarch64 -M virt -cpu cortex-a53`.
- **cortex-m3**/**cortex-m4**/**cortex-m7** — real ARM Thumb/Thumb-2
  assembly for the ARMv7-M "mainline" M-profile architecture (see
  `src/codegen/armm/ArmMCodegen.h`'s own header comment), one shared
  backend for all three part numbers (`cortex-m4`/`cortex-m7` produce
  provably byte-identical output to `cortex-m3` - same software-
  identical-part-numbers reasoning as `8088`/`8086`). Every
  instruction emitted is real Thumb-1 (16-bit) or the handful of
  Thumb-2 32-bit forms (`MOVW`/`MOVT`, `BL`, a wide `B`) genuinely
  needed, so it runs unchanged on any real ARMv7-M core; baseline
  M-profile (Cortex-M0/M0+/M23, ARMv6-M) isn't covered, since that
  architecture has neither of those. Like `68000`/`mips`, a real
  register machine with a real hardware stack and hardware multiply -
  `generate()` returns a complete, self-contained program loaded at a
  fixed address, no vector table or reset exception (the simulator
  sets the CPU's PC directly, unlike `stm32`'s/`mc68328`'s own real-
  vector-table choice). `tests/run_tests.sh --target=cortex-m3` runs
  its own test suite against `qemu-system-arm -M mps2-an385`;
  `make test` doesn't run it.
- **r10000**/**r4000** — MIPS assembly (this project's own dialect -
  see `src/codegen/mips/MipsAssembler.h`), targeting the MIPS IV
  instruction set the R10000 implements; `--target=r4000` is a
  software-identical alias (this backend only ever emits real MIPS I
  instructions - `ADDU`/`SUBU`/`SLT`/`MULT`/`DIV`/`LW`/`SW`/`BEQ`/
  `BNE`/`JAL`/etc. - a strict subset any MIPS I/II/III/IV core
  including a real R4000 already has, so there's no separate
  R4000-specific backend, same `8088`/`8086` reasoning). Like
  `68000`, a real register machine with a real hardware stack and
  hardware multiply/divide/shift - `generate()` returns a complete,
  self-contained program loaded at a fixed address, halting in a
  self-branch infinite loop. Verified directly under
  `qemu-system-mips -cpu R4000` (qemu ships no real R10000 model).
  `tests/run_tests.sh --target=r10000` runs its own test suite against
  `qemu-system-mips -M malta -cpu 4Kc`, but `make test` doesn't run
  it.
- **powerpc** — classic 32-bit PowerPC assembly (this project's own
  dialect - see `src/codegen/powerpc/PowerPCAssembler.h`), the
  instruction set every G3/G4/G5 and embedded PowerPC core shares.
  Like `mips`, a real register machine with a real hardware stack and
  a single hardware link register - real `bl`/`blr` calls, real `r1`
  stack pointer, real `lis`/`ori` for loading a full 32-bit address
  (no single instruction holds one), real hardware multiply and
  `divw`-based divide. `generate()` returns a complete, self-
  contained program loaded at a fixed address, halting in a self-
  branch infinite loop. Verified directly under
  `qemu-system-ppc -M none -cpu 603` during development, including a
  deliberately crafted carry-overflow case exercising the `lis`/`ori`
  hi/lo address-load split at link time. `tests/run_tests.sh
  --target=powerpc` runs its own test suite against
  `qemu-system-ppc -M none -cpu G3`, but `make test` doesn't run it.
- **sparc** — 32-bit SPARC V8 assembly (this project's own dialect -
  see `src/codegen/sparc/SparcAssembler.h`), the closest sibling to
  `powerpc` here: another real-register, real-hardware-stack 32-bit
  RISC load/store target with no immediate wide enough to hold a full
  32-bit address in one instruction (`sethi`/`or`, SPARC's own
  22-bit/10-bit split, the direct analogue of PowerPC's `lis`/`ori`).
  Real SPARC has 32-entry *windowed* `%i`/`%l`/`%o` register banks
  tied to a real `save`/`restore` pair that rotates the whole window
  on every non-leaf call - genuine hardware recursion support, but
  this backend deliberately doesn't use it: a real window only has a
  handful of entries (commonly 8), and once they're all in use a
  further `save` traps for the OS to spill the oldest one to memory -
  machinery this backend, running bare-metal with no trap table at
  all, doesn't have. So `%i`/`%l`/`%o`/`%g` stay plain, always-the-
  same-physical-register names for the entire run (the real leaf-
  routine optimization real SPARC compilers already use for any
  routine that doesn't need a fresh window), and recursion instead
  gets a conventional software stack frame (a dedicated `fp` register
  plus the real `sp`), the same shape every other backend here uses -
  bounded only by how much of the real stack is available, not by the
  hardware window count.
  Every `call`/`retl`/branch mnemonic always synthesizes its own
  required delay-slot `nop`. `generate()` returns a complete, self-
  contained program loaded at a fixed address (`$4000`), halting in a
  self-branch infinite loop. Verified directly under
  `qemu-system-sparc -cpu TI-SuperSparc-II` during development, both
  for the whole-program path and a real cross-object `--link`;
  no automated `tests/run_tests.sh` target yet.

Every global variable is a single statically allocated word; every
Local and Param instead lives in a per-call frame that each backend
builds on whatever stack (hardware or software) its own target
provides, so direct and indirect/mutual recursion compute correctly
everywhere - bounded by that backend's own real limit, whether that's
available memory for a software stack or (on `8008`/`pic16`/`pic18`)
a fixed-depth hardware call stack that can't be extended. The same C
subset compiles to every target — `#ifdef __PDP7__` / `#ifdef __8086__` (also defined
for `--target=8088`) / `#ifdef __6502__` (also defined for
`--target=65c02`, plus its own `#ifdef __65C02__` - see
`src/main.cpp`'s own `makeBackend()` comment) / `#ifdef __68000__`
(also defined for `--target=mc68328`, plus its own `#ifdef
__MC68328__`) / `#ifdef __PDP11__` / `#ifdef __STM8__` / `#ifdef
__M16C__` / `#ifdef __8080__` / `#ifdef __Z80__` / `#ifdef __8008__` /
`#ifdef __MCS51__` (also defined for `--target=8051`/`8052`/`8031`/
`8032`, plus each one's own exact `#ifdef __8051__`/`__8052__`/
`__8031__`/`__8032__`) / `#ifdef __PIC16__` / `#ifdef __PIC18__` /
`#ifdef __STM32__` / `#ifdef __I386__` / `#ifdef __ARMV7A__` /
`#ifdef __ARMV8A__` (also defines `#ifdef __aarch64__`) / `#ifdef
__CORTEX_M3__` (also defined for `--target=cortex-m4`/`cortex-m7`,
plus each one's own exact `#ifdef __CORTEX_M4__`/`__CORTEX_M7__`) /
`#ifdef __R10000__` (also defined for `--target=r4000`, plus its own
`#ifdef __R4000__`) / `#ifdef __POWERPC__` (also defines `#ifdef
__ppc__`) / `#ifdef __SPARC__` (also defines `#ifdef __sparc__`) let
source code branch on which one it's being built for.
