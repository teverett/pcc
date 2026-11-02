#pragma once

#include <string>

namespace pcc {

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

}  // namespace pcc
