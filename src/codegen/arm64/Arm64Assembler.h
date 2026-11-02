#pragma once

#include <string>

namespace pcc {

// A from-scratch, in-process two-pass assembler for exactly the
// narrow AArch64 ("A64") dialect Arm64Codegen.cpp emits - not a
// general-purpose A64 assembler. Parses via the same shared grammar
// ../../../grammar/PccAsm.g4 every other backend's own in-process
// assembler uses (see M68kAssembler.h's own header comment). Every
// operand is a plain expression (a number, a register name, or a
// label) classified by Arm64Codegen.cpp's own convention, same "no
// bracketed memory operand" choice MIPS/M16C/STM8 already make (see
// MipsAssembler.h's own header comment on why) - every load/store
// here goes through the dedicated `ldind`/`stind` mnemonics instead,
// always a fixed zero displacement.
//
// Register names recognize both the 32-bit ("W") and 64-bit ("X")
// view of each of the 31 general registers (`w0`..`w30`, `x0`..`x30`)
// plus `wzr`/`xzr` (the zero register) and `sp` (the real hardware
// stack pointer) - exactly real A64 assembly syntax. Which view a
// register is named with on a given instruction's *destination*
// operand is what selects that instruction's own 32-bit-vs-64-bit
// ("sf") encoding, the same way a real A64 assembler infers it - see
// encodeInstr()'s own comment. `ldind`/`stind`'s own base-register
// operand is always 64-bit regardless of its own `w`/`x` spelling
// (real A64 load/store base registers always are), so Arm64Codegen.cpp
// always spells it with an `x` name purely for readability, not
// because this assembler requires it.
//
// `cset<cc>` is spelled as six fixed, single-operand mnemonics
// (`csetlt`/`csetgt`/`csetle`/`csetge`/`cseteq`/`csetne`) rather than
// one `cset rd,cc` mnemonic taking a condition-code operand - this
// grammar's own `expr` has no way to write a bare condition mnemonic
// like `lt` that isn't itself either a register name or a label (an
// `IDENT` is always one or the other to buildOperand() below), so a
// second structurally-distinct operand kind would need inventing just
// for this one mnemonic; six fixed mnemonics avoid that entirely,
// same spirit as `ldind`/`stind` existing instead of a bracketed
// memory operand.
//
// Only the ARMv8-A "A64" 32-bit data-processing instruction subset is
// emitted (this project's own `int`, matching every other backend's
// own word-size choice - see Arm64Codegen.h) wherever the destination
// uses a `w`-register; every instruction here is a real, standard A64
// encoding, so it runs unchanged on any real Armv8-A core. Every
// instruction here is a fixed 4 bytes. Unlike MIPS, A64 branches have
// no delay slot at all, so this assembler (unlike MipsAssembler.cpp)
// never needs to account for one.
//
// Throws std::runtime_error on any assembly error.
std::string assembleArm64(const std::string &asmText);

}  // namespace pcc
