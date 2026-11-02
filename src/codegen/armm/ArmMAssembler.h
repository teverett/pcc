#pragma once

#include <cstdint>
#include <string>

#include "fileformat/ObjectFile.h"

namespace pcc {

// This target's own custom, non-registered COFF/object-format
// machine ID (no real one exists for this project's own ARMv7-M
// Thumb/Thumb-2 dialect - see ../../fileformat/coff/CoffFormat.h's own header
// comment) - exported so ../../linker/factory/ArmMLinkerFactory.cpp
// can check an input object's embedded machine type against
// `--target=cortex-m3`/`cortex-m4`/`cortex-m7` without duplicating
// this literal. Shares the `0xA0` ARM-family prefix with
// `kArmCoffMachine`/`kArm64CoffMachine` (see ArmAssembler.h's/
// Arm64Assembler.h's own header comments); the low byte names the
// architecture version every Cortex-M3/M4/M7 core implements
// (ARMv7-M/ARMv7E-M), not one specific core - this single machine ID
// covers all three `--target=cortex-m*` aliases (see ArmMCodegen.h's
// own header comment), since they all compile to the exact same
// object code.
constexpr uint16_t kArmMCoffMachine = 0xA007;

// Where the whole program loads and starts running once real
// separate compilation (`--format=coff`/`elf`/`macho`/`aout`, see
// `allowExternal` below) and `--link` are both involved - the same
// address ArmMCodegen.cpp's own whole-program `--format=raw` entry
// sequence already uses (`kBaseAddress`), kept identical so a linked
// program behaves exactly like a single-file one. Exported so
// ArmMLinkerFactory.cpp's own sequential object layout starts from
// the same place.
constexpr uint32_t kArmMBaseAddress = 0x1000u;

// This dialect's three relocation kinds - see
// ../../linker/factory/ArmMLinkerFactory.cpp's own local patch
// functions (not shared in GenericLinker.h, since this dialect's own
// `MOVW`/`MOVT` imm4:i:imm3:imm8 split and `BL`/wide-`B`'s own
// S:I1:I2:imm10:imm11 (J1/J2-encoded) split aren't reusable by any
// other migrated target - see GenericLinker.h's own header comment,
// which calls both of these out by name):
//   MovWLo - `MOVW`'s own low 16 bits of a full address, spread
//            across its own non-contiguous imm4/i/imm3/imm8 fields.
//   MovTHi - `MOVT`'s own high 16 bits of the same address, same
//            split.
//   Branch - the 32-bit-wide `BL`/`B.W` PC-relative immediate field
//            (also used by the wide `B.W` half of every
//            `beq`/`bne`/`blt`/`ble`/`bgt`/`bge` expansion - see
//            ArmMAssembler.cpp's own `encodeConditionalBranch()`),
//            scrambled across both halfwords via the real Thumb-2
//            S/I1/I2/J1/J2 encoding. Fires only when the branch's
//            own target is undefined in this object (a genuinely
//            cross-object call/jump) - never for one resolved
//            locally, which needs no relocation at all (see this
//            header's own `assembleArmMObject` comment).
constexpr uint16_t kArmMRelocMovWLo = 1;
constexpr uint16_t kArmMRelocMovTHi = 2;
constexpr uint16_t kArmMRelocBranch = 3;

// A from-scratch, in-process two-pass assembler for exactly the
// narrow ARM Thumb dialect ArmMCodegen.cpp emits - not a general-
// purpose ARM assembler. Parses via the same shared grammar
// ../../../grammar/PccAsm.g4 every other backend's own in-process
// assembler uses (see M68kAssembler.h's own header comment). Every
// operand is a plain expression (a number, a register name, or a
// label) classified by ArmMCodegen.cpp's own convention, not real
// `[Rn, #0]`/`{reglist}`/`#imm` ARM syntax - the grammar has no way
// to express a bracketed memory operand or a `{...}` register list,
// so every addressing mode or multi-register instruction this
// dialect can't express as a bare register-or-label operand instead
// gets its own dedicated mnemonic: `ldind`/`stind` (always a fixed
// zero displacement, same convention as MIPS's own `ldind`/`stind` -
// see MipsAssembler.h), and `push`/`pop` (always exactly one
// register) plus `pushlr`/`poppc` (the two fixed-register-list forms
// this backend's own prologue/epilogue needs - see ArmMCodegen.h's
// own header comment) in place of real `PUSH`/`POP {reglist}`.
//
// `movw`/`movt`'s own immediate operand resolves a *label* operand
// (as opposed to a plain number) to that label's own lower/upper 16
// bits respectively, instead of needing `:lower16:`/`:upper16:`
// spelled out - same convention as MIPS's `lui`/`ori` resolving a
// label's upper/lower half (see MipsAssembler.h's own header
// comment), just with the halves in the opposite order (ARM's own
// `MOVW` carries the *lower* half, `MOVT` the upper).
//
// Only Thumb/Thumb-2 instructions real ARMv7-M (Cortex-M3/M4/M7)
// hardware actually executes are ever emitted - see ArmMCodegen.h's
// own header comment on why baseline M-profile (Cortex-M0/M0+/M23)
// isn't covered. Every conditional branch mnemonic
// (`beq`/`bne`/`blt`/`ble`/`bgt`/`bge`) expands to *two* real
// instructions - a short inverted-condition `Bcc` immediately
// followed by a real wide unconditional `B` - see
// `encodeConditionalBranch()`'s own comment for why.
//
// Every instruction here is a fixed, predictable size per mnemonic
// (2 bytes for the plain 16-bit Thumb-1 forms, 4 bytes for the wide
// Thumb-2 ones - `movw`/`movt`/`bl`/`b`, and 6 bytes for the two-
// instruction conditional-branch mnemonics above), tracked by the
// same two-pass offset-computing scheme every other backend's own
// assembler uses.
//
// Throws std::runtime_error on any assembly error.
std::string assembleArmM(const std::string &asmText);

// The real separate-compilation entry point (`--format=coff`/`elf`/
// `macho`/`aout` - see ArmMCodegen.h's own `objectMode`): assembles
// `asmText` (generated with `org 0`, not `kArmMBaseAddress` - see
// ArmMCodegen.cpp's own `generate()`) exactly like assembleArmM()
// does, except an undefined symbol is deferred as a relocation
// instead of a hard error. Every `MOVW`/`MOVT` address reference -
// even to a label defined in this same file - also gets an
// additional relocation against the reserved pseudo-symbol ".." (see
// ../../linker/GenericLinker.h's own header comment on why: this
// text's own labels all resolve to "offset from this object's own
// org 0" until that base is added, and an absolute 32-bit address
// load needs that base added in no matter what). A `BL`/wide-`B`
// branch (including the one every conditional-branch mnemonic
// expands to) needs no such base-shift relocation at all - it's a
// plain PC-relative reference, already correct regardless of where
// this object ends up linked (both the branch site and a same-file
// target shift by the same base) - and gets a relocation only when
// its own target is genuinely undefined in this file, the same
// reasoning ../arm64/Arm64Assembler.h's own `b`/`bl` comment gives.
// Returns the format-neutral ObjectFile a codegen backend builds
// once per compile (see ../../fileformat/ObjectFile.h) - this
// function never touches a real COFF/ELF/Mach-O/a.out byte layout
// itself.
ObjectFile assembleArmMObject(const std::string &asmText);

}  // namespace pcc
