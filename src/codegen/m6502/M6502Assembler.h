#pragma once

#include <string>

namespace pcc {

// A from-scratch, in-process two-pass assembler for exactly the
// narrow 6502 dialect M6502Codegen.cpp emits - not a general-purpose
// 6502 assembler. Parses via the same shared grammar
// ../../../grammar/PccAsm.g4 the other two backends' own in-process
// assemblers use (see Pdp7Assembler.h/I8086Assembler.h's own header
// comments) - every operand here is a plain expression (a number, a
// symbol, or a signed combination of them); there's no register
// operand syntax at all (every mnemonic already implies which real
// 6502 register(s) it touches - see M6502Codegen.cpp's own comment
// for why), and no bracketed memory-operand syntax either (the one
// addressing mode that needs it, zero-page indirect indexed, has its
// own dedicated mnemonics - `ldaind`/`staind` - instead).
//
// No relocations, no external symbols, no `--format=coff`: this
// backend only supports a single, fully self-contained
// `--format=bin` compile (see M6502Codegen.h's own comment) - every
// symbol must resolve within the one file being assembled, a hard
// error otherwise.
//
// Every instruction's *size* is fixed by its mnemonic alone, never
// by any operand's resolved value - same convention, and the same
// reason (a stable two-pass size computation with no phase-sizing
// ambiguity), as ../pdp7/Pdp7Assembler.h and
// ../x86_8086/I8086Assembler.h's own versions of this comment.
//
// Throws std::runtime_error on any assembly error.
std::string assembleM6502(const std::string &asmText);

}  // namespace pcc
