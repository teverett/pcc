#pragma once

#include <string>

namespace pcc {

// A from-scratch, in-process two-pass assembler for exactly the
// narrow MCS-51 dialect MCS51Codegen.cpp emits - not a general-
// purpose 8051 assembler. Parses via the same shared grammar
// ../../../grammar/PccAsm.g4 every other in-process-assembler
// backend uses (see M6502Assembler.h/I8086Assembler.h's own header
// comments) - every operand is a plain expression (a number, a
// symbol, or a signed combination of them); there's no register
// operand syntax at all (every mnemonic already implies which real
// register(s) it touches - "mova"/"movai"/"movd"/"movdi" spell out
// direction and operand kind the way M6502Assembler.cpp's own
// "ldai"/"lda" split already does, since the real 8051 `MOV`
// mnemonic alone is ambiguous between at least six different
// encodings - see MCS51Codegen.cpp's own instruction choices), and
// no bracketed memory-operand syntax either (the one real indirect
// addressing mode this dialect ever needs - XDATA via DPTR - has its
// own dedicated implied-operand mnemonics, `movxr`/`movxw`, instead).
//
// No relocations, no external symbols, no `--format=coff`: this
// backend only supports a single, fully self-contained
// `--format=raw` compile, assembled from CODE address 0 - every
// symbol must resolve within the one file being assembled, a hard
// error otherwise. Every symbol this assembler itself ever resolves
// is a CODE-space address (a function or branch-target label, or one
// of the fixed internal-RAM pseudo-register equates) - ordinary
// variable storage lives in the separate XDATA address space that
// MCS51Codegen.cpp tracks entirely on its own (see that file's
// allocXdata()) and which this assembler never sees a symbol for at
// all; every MOVX this dialect emits always uses a plain numeric
// `mov dptr,#<addr>` immediate, never a symbol lookup here.
//
// Every instruction's *size* is fixed by its mnemonic alone, never by
// any operand's resolved value - same convention, and the same
// reason (a stable two-pass size computation with no phase-sizing
// ambiguity), as every other backend's own in-process assembler here.
//
// Throws std::runtime_error on any assembly error.
std::string assembleMCS51(const std::string &asmText);

}  // namespace pcc
