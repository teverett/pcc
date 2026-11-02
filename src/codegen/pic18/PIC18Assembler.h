#pragma once

#include <string>

namespace pcc {

// A from-scratch, in-process two-pass assembler for the subset of
// real PIC18 assembly PIC18Codegen.cpp emits - not a general-purpose
// PIC18 assembler (no macros, no conditional assembly, no
// `--format=coff`). Uses real, standard PIC18 mnemonics and produces
// real PIC18 machine code (each 16-bit instruction word packed little-
// endian, the exact on-disk shape Microchip's own Intel-HEX output
// uses) wrapped in a real Intel HEX container.
//
// Same shared grammar and the same two private directives as
// PIC16Assembler.h's own dialect (`orgram`/`res`/`=`-equate - see
// that file's own header comment for what each one means); this
// target's own `res`/`orgram` still only ever reserve space in the
// 96-byte Access Bank GPR window (0x00-0x5F - see PIC18Codegen.h's
// own header comment), the one real difference being that this core
// also has one genuine two-word, full-12-bit-addressed instruction
// (MOVFF) whose operands are never truncated to the Access Bank's own
// 8-bit short form the way every other instruction's `f` operand is -
// see `fileAddr12()`'s own comment.
//
// Throws std::runtime_error on any assembly error.
std::string assemblePIC18(const std::string &asmText);

}  // namespace pcc
