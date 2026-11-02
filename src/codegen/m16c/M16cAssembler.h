#pragma once

#include <string>

namespace pcc {

// A from-scratch, in-process two-pass assembler for the subset of
// M16C assembly ../M16cCodegen.cpp emits - not a general-purpose M16C
// assembler (no macros, no conditional assembly, no relocations/
// external symbols/`--format=coff` - see M16cCodegen.h's own header
// comment: this backend only supports a single, fully self-contained
// `--format=raw` compile), and - unlike the pdp11/8080 backends' own
// in-process assemblers (which produce real object code for their
// own targets) - not real M16C object code either: like the 6502/
// 68000 backends' own assemblers (see I8080Assembler.h's own header
// comment on that distinction), this one invents its own narrow,
// simply-decodable byte encoding for exactly the mnemonic/operand
// shapes ../M16cCodegen.cpp ever emits (a one-byte opcode, then one
// tagged byte-or-more per operand - see encodeOperand() in
// M16cAssembler.cpp) - a real Renesas M16C disassembler would not
// make sense of this backend's own output. What *is* real here is the
// instruction set this dialect exposes: every mnemonic names an
// actual M16C instruction, used for the actual thing that instruction
// actually does on real silicon (real `MUL.W`/`DIVU.W`, a real
// signed-count `SHL.W`, real condition-code-driven conditional jumps -
// see M16cCodegen.h's own header comment) - only the *bytes* this
// particular assembler happens to produce for them are this project's
// own invention, not Renesas's.
//
// Every jump/conditional-jump mnemonic here always takes a full
// absolute address operand (unlike real M16C assembly, where
// conditional jumps are short and PC-relative) - a deliberate
// simplification this dialect makes, same as the 8080 backend's own
// jumps (see M16cCodegen.h's own header comment) - so there is no
// branch-range limit anywhere in this encoding to work around.
//
// Parses via the same shared grammar ../../../grammar/PccAsm.g4 every
// other backend's own in-process assembler uses; a register name
// (`r0`-`r3`, `a0`/`a1`, `sp`) is syntactically just another bare
// identifier to that grammar - this file is what decides, per operand,
// whether a given identifier names a register or a symbol to resolve
// against the label table (see classifyOperand() in
// M16cAssembler.cpp).
//
// Every instruction's own *size* depends on which addressing mode
// each of its operands resolves to (a register operand is one byte
// smaller than an immediate/absolute one - see encodeOperand()), so -
// unlike the 8080/pdp11 backends' own in-process assemblers, where
// every mnemonic's size is fixed by the mnemonic alone - this one
// genuinely needs its own two-pass label resolution to converge (a
// label referenced before a register-vs-memory-sized instruction
// between it and its own definition could otherwise land at the wrong
// address on a naive single pass).
//
// Throws std::runtime_error on any assembly error.
std::string assembleM16c(const std::string &asmText);

}  // namespace pcc
