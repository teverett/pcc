#pragma once

#include <string>

namespace pcc {

// A from-scratch, in-process two-pass assembler for exactly the
// narrow 68000 dialect M68kCodegen.cpp emits - not a general-purpose
// 68000 assembler. Parses via the same shared grammar
// ../../../grammar/PccAsm.g4 the other backends' own in-process
// assemblers use (see Pdp7Assembler.h/I8086Assembler.h/
// M6502Assembler.h's own header comments). Every operand is a plain
// expression (a number, a register name, or a label) classified by
// M68kCodegen.cpp's own convention, not real `#imm`/`(An)`-style
// 68000 syntax - the grammar has no `#` token and no way to
// distinguish "(expr)" (a parenthesized group, already used
// elsewhere) from "(An)" (register-indirect), so every addressing
// mode this dialect can't express as a bare register-or-label
// operand gets its own dedicated mnemonic instead (`ldind`/`stind`
// for `(An)`, `pushl`/`popl` for `-(A7)`/`(A7)+`) - see
// M68kAssembler.cpp's own comment for the full mnemonic list.
//
// Only this project's 32-bit (.L) forms are implemented (the
// language's own `int`, chosen to match this target's native
// register width - see M68kCodegen.h), plus the two 16-bit (.W)
// forms (`MULS.W`/`DIVS.W`) that are the *only* multiply/divide this
// CPU has at all.
//
// Every instruction's size is fixed by its mnemonic alone, same
// convention (and the same reason) as every other backend's own
// assembler.
//
// Throws std::runtime_error on any assembly error.
std::string assembleM68k(const std::string &asmText);

}  // namespace pcc
