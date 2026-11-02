#pragma once

#include <string>

namespace pcc {

// A from-scratch, in-process two-pass assembler for exactly the
// narrow STM8 dialect STM8Codegen.cpp emits - not a general-purpose
// STM8 assembler. Parses via the same shared grammar
// ../../../grammar/PccAsm.g4 every other backend's own in-process
// assembler uses; a register name (`a`, `x`, `y`, `sp`) is
// syntactically just another bare identifier to that grammar - this
// file is what decides, per mnemonic and operand position, whether a
// given identifier names a register or a symbol to resolve against
// the label table (same convention as I8080Assembler.h).
//
// Every mnemonic here is a real STM8 one, but the *encoding* is this
// project's own, not guaranteed byte-identical to a real STM8
// toolchain's output - unlike the 6502/8080 backends, there's no
// available from-scratch STM8 emulator on PyPI to cross-check a real
// opcode encoding against (see STM8Codegen.h's own header comment),
// so correctness here means "self-consistent with what STM8Codegen.cpp
// emits", not "matches ST's own assembler byte-for-byte". Two
// deliberate departures from the real chip for exactly that reason:
//
//   - every conditional jump (`jreq`/`jrne`/`jrult`/`jruge`) is full
//     16-bit absolute, not the real STM8's 8-bit signed relative
//     `JRxx` - see STM8Codegen.h.
//   - indirect-through-`x` addressing (real `LD A,(X)`/`LD (X),A`)
//     has its own dedicated zero-operand mnemonics, `ldxind`/
//     `stxind`, instead of a literal `(x)` operand - the shared
//     grammar's `(` already means "parenthesized sub-expression" (see
//     Pdp11Assembler.h's own comment on the exact same problem with
//     `(Rn)`), so there's no way to tell "indirect through register x"
//     apart from "the symbol x, grouped" without a dedicated mnemonic.
//
// Every instruction's *size* is fixed by its mnemonic alone, never by
// any operand's resolved value - same convention, and the same
// reason, as every other backend's own in-process assembler.
//
// No relocations, no external symbols, no `--format=coff`: this
// backend only supports a single, fully self-contained `--format=bin`
// compile, same restriction as the 6502/8080 backends.
//
// Throws std::runtime_error on any assembly error.
std::string assembleSTM8(const std::string &asmText);

}  // namespace pcc
