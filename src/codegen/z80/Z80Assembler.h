#pragma once

#include <string>

namespace pcc {

// A from-scratch, in-process two-pass assembler for the subset of
// real Z80 assembly Z80Codegen.cpp emits - not a general-purpose Z80
// assembler (no macros, no conditional assembly, no relocations/
// external symbols/`--format=coff` - see Z80Codegen.h's own header
// comment: this backend only supports a single, fully self-contained
// `--format=bin` compile). Like ../i8080/I8080Assembler.h, this uses
// real, standard Z80 mnemonics and produces real Z80 machine code -
// every opcode byte emitted here matches Zilog's own published
// encoding (including the `CB`-prefixed bit/rotate group and the
// `ED`-prefixed extended group), so the output is valid input to any
// accurate Z80 (or 8080, for the opcodes the two chips share) emulator
// or disassembler.
//
// Parses via the same shared grammar ../../../grammar/PccAsm.g4 every
// other backend's own in-process assembler uses. Unlike the 8080
// dialect (which rejects bracketed operands outright), this one
// gives brackets a real meaning: `[expr]` always means "the 16-bit
// address `expr` names", used for every instruction whose real Z80
// syntax uses parentheses for a direct memory reference (`LD HL,(nn)`
// spelled `ld hl, [nn]` here, `LD A,(BC)` spelled `ld a, [bc]`, and
// so on) - this disambiguates a register pair's own *value* (bare
// `bc`) from "the byte(s) at the address *in* that pair" (`[bc]`),
// which are two genuinely different real instructions. A register
// name (`a`, `b`, ..., `bc`, `af`) is otherwise just another bare
// identifier to
// the shared grammar - this file is what decides, per mnemonic and
// operand position, whether a given identifier names a register (or
// register pair) or a symbol to resolve against the label table.
//
// Every instruction's *size* is fixed by its mnemonic alone, never by
// any operand's resolved value - same convention, and the same
// reason, as every other backend's own in-process assembler.
//
// Throws std::runtime_error on any assembly error.
std::string assembleZ80(const std::string &asmText);

}  // namespace pcc
