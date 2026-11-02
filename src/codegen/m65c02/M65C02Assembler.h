#pragma once

#include <string>

namespace pcc {

// A from-scratch, in-process two-pass assembler for exactly the
// narrow WDC 65C02 dialect M65C02Codegen.cpp emits - not a general-
// purpose 65C02 assembler. This is ../m6502/M6502Assembler.cpp's own
// dialect plus exactly one CMOS-only addition: `stz`/`stzp` (STZ,
// absolute/zero-page - see M65C02Codegen.cpp's own comment on where
// that's used and why); everything else here is identical to that
// file; see its own header comment for the shared rationale. Parses
// via the same shared grammar ../../../grammar/PccAsm.g4 the other
// backends' own in-process assemblers use (see Pdp7Assembler.h/
// I8086Assembler.h's own header comments) - every operand here is a
// plain expression (a number, a symbol, or a signed combination of
// them); there's no register operand syntax at all (every mnemonic
// already implies which real 65C02 register(s) it touches - see
// M65C02Codegen.cpp's own comment for why), and no bracketed memory-
// operand syntax either (the one addressing mode that needs it,
// zero-page indirect indexed, has its own dedicated mnemonics -
// `ldaind`/`staind` - instead).
//
// No relocations, no external symbols, no `--format=coff`: this
// backend only supports a single, fully self-contained
// `--format=raw` compile (see M65C02Codegen.h's own comment) - every
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
std::string assembleM65C02(const std::string &asmText);

}  // namespace pcc
