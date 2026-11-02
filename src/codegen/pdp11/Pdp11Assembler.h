#pragma once

#include <string>

namespace pcc {

// A from-scratch, in-process two-pass assembler for exactly the
// narrow PDP-11 dialect Pdp11Codegen.cpp emits - not a general-
// purpose PDP-11 (MACRO-11) assembler. Parses via the same shared
// grammar ../../../grammar/PccAsm.g4 the other backends' own in-
// process assemblers use (see M68kAssembler.h/M6502Assembler.h/
// Pdp7Assembler.h's own header comments). Every operand is a plain
// expression (a number, a register name, or a label) classified by
// Pdp11Codegen.cpp's own convention, not real MACRO-11 `(Rn)`/`#n`/
// `@#n` syntax - the grammar has no `#`/`@` token and no way to
// distinguish "(expr)" (a parenthesized group, already used
// elsewhere) from "(Rn)" (register-deferred), so every addressing
// mode this dialect can't express as a bare register-or-label
// operand gets its own dedicated mnemonic instead (`ldind`/`stind`
// for `(Rn)`, `push`/`pop` for `-(SP)`/`(SP)+`, `lea` for "the
// address of a label" rather than MACRO-11's own `MOV #label,Rn`
// idiom) - see Pdp11Assembler.cpp's own comment for the full
// mnemonic list.
//
// A bare label operand (anywhere other than `lea`'s own source) is
// always PDP-11 "relative" addressing (mode 067: index mode with
// PC as the base register) - the real hardware addressing mode
// MACRO-11 itself assembles a bare symbol to - which reads/writes
// the *value* stored at that label, exactly like an ordinary global
// variable access; `lea`'s own source is the one place a label means
// "give me this address as a value instead", via PC-relative
// *immediate* addressing (mode 027) with the label's absolute
// address as the literal.
//
// This project's own 16-bit `int` matches the PDP-11's native word
// size, so every instruction here is an ordinary word (no `.B`-
// suffixed byte op is ever emitted).
//
// Throws std::runtime_error on any assembly error.
std::string assemblePdp11(const std::string &asmText);

}  // namespace pcc
