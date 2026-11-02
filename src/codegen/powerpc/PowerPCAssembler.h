#pragma once

#include <string>

namespace pcc {

// A from-scratch, in-process two-pass assembler for exactly the
// narrow PowerPC dialect PowerPCCodegen.cpp emits - not a general-
// purpose PowerPC assembler. Parses via the same shared grammar
// ../../../grammar/PccAsm.g4 every other backend's own in-process
// assembler uses (see M68kAssembler.h's own header comment). Every
// operand is a plain expression (a number, a register name, or a
// label) classified by PowerPCCodegen.cpp's own convention, not real
// `0(r4)`/`%hi()`/`%lo()` PowerPC syntax - the grammar has no way to
// express a displacement-from-register memory operand or a function-
// style `%hi`/`%lo` modifier, so every addressing mode this dialect
// can't express as a bare register-or-label operand instead gets its
// own dedicated mnemonic (`ldind`/`stind`, always a fixed zero
// displacement - see PowerPCCodegen.h's own header comment on why
// every load/store here is always through a freshly computed
// address), and `lis`/`ori` resolve a *label* operand (as opposed to
// a plain number) to that label's own upper/lower 16 bits
// automatically instead of needing `%hi`/`%lo` spelled out at all
// (same convention as MipsAssembler.h's own `lui`/`ori`).
//
// Every real instruction encoded here matches genuine 32-bit classic
// PowerPC machine code bit-for-bit (opcodes/XO fields/operand-field
// placement all taken from the real architecture, including the
// well-known asymmetry where a logical instruction's own mnemonic
// operand order - `and rDest,rSrc,rB` - doesn't match its own
// encoded field order, unlike an arithmetic instruction's - see
// encodeXArith()/encodeXLogical()'s own comments) - this dialect only
// narrows which *mnemonics and operand shapes* exist, never what a
// given real encoding means.
//
// Every instruction here is a fixed 4 bytes. Unlike MIPS, PowerPC has
// no branch-delay slot at all, so there's no MIPS-style trailing
// `nop` convention to account for here either.
//
// Throws std::runtime_error on any assembly error.
std::string assemblePowerPC(const std::string &asmText);

}  // namespace pcc
