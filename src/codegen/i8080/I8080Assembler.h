#pragma once

#include <string>

namespace pcc {

// A from-scratch, in-process two-pass assembler for the subset of
// real Intel 8080 assembly I8080Codegen.cpp emits - not a general-
// purpose 8080 assembler (no macros, no conditional assembly, no
// relocations/external symbols/`--format=coff` - see
// I8080Codegen.h's own header comment: this backend only supports a
// single, fully self-contained `--format=raw` compile). Unlike the
// 6502/68000 backends' own in-process assemblers (which invent a
// narrow private mnemonic dialect - see M6502Assembler.h), this one
// uses real, standard 8080 mnemonics and produces real 8080 machine
// code: every opcode byte emitted here matches Intel's own published
// encoding, so the output is valid input to any accurate 8080 (or
// object-code-compatible Z80) emulator or disassembler, not just this
// project's own in-process one.
//
// Parses via the same shared grammar ../../../grammar/PccAsm.g4 every
// other backend's own in-process assembler uses; a register name
// (`a`, `b`, ..., `h`, `l`, `m`, `sp`, `psw`) is syntactically just
// another bare identifier to that grammar - this file is what decides,
// per mnemonic and operand position, whether a given identifier names
// a register or a symbol to resolve against the label table.
//
// Every instruction's *size* is fixed by its mnemonic alone, never by
// any operand's resolved value - same convention, and the same
// reason, as every other backend's own in-process assembler.
//
// Throws std::runtime_error on any assembly error.
std::string assembleI8080(const std::string &asmText);

}  // namespace pcc
