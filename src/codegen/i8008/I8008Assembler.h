#pragma once

#include <string>

namespace pcc {

// A from-scratch, in-process two-pass assembler for the subset of
// real Intel 8008 assembly I8008Codegen.cpp emits - not a general-
// purpose 8008 assembler (no macros, no conditional assembly, no
// relocations/external symbols/`--format=coff` - see
// I8008Codegen.h's own header comment: this backend only supports a
// single, fully self-contained `--format=bin` compile).
//
// Uses real, standard mnemonics and the 8008's own real register
// codes (a=0,b=1,c=2,d=3,e=4,h=5,l=6,m=7 - *not* the same numbering
// the 8080/Z80 backends' own assemblers use, see I8008Codegen.h's own
// header comment) and produces real 8008 machine code, verified
// against Intel's own published instruction set (Bryan Blackburn's
// 8008 Instruction Set code-sort reference was used to check every
// opcode byte this file emits).
//
// Parses via the same shared grammar ../../../grammar/PccAsm.g4 every
// other backend's own in-process assembler uses; a register name is
// otherwise just another bare identifier to that grammar - this file
// decides, per mnemonic and operand position, whether a given
// identifier names a register or a symbol to resolve against the
// label table. Unlike the 8080/Z80 dialects, this one *does* support
// `name = expr` equates (`Line::Equate` below) - used only to pin
// this backend's own fixed memory cells (`ac`/`bc`/.../the `sv*`
// save-stack - see I8008Codegen.h's own header comment) to addresses
// tests/run_tests.sh's own from-scratch Python 8008 emulator can
// read directly by fixed address, without needing to parse this
// assembler's own symbol table at all - a test-harness convenience,
// not a hardware addressing restriction the way the 6502's own
// zero-page equates are (see M6502Assembler.h's own comment).
//
// Every instruction's *size* is fixed by its mnemonic alone, never by
// any operand's resolved value - same convention, and the same
// reason, as every other backend's own in-process assembler.
//
// Throws std::runtime_error on any assembly error.
std::string assembleI8008(const std::string &asmText);

}  // namespace pcc
