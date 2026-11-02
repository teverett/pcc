#pragma once

#include <string>

namespace pcc {

// A from-scratch, in-process two-pass assembler for exactly the
// narrow 32-bit i386 dialect I386Codegen.cpp emits - not a general-
// purpose x86 assembler, the same scope as
// ../x86_8086/I8086Assembler.h (that file's own header comment on
// "exactly the mnemonic/operand shapes [the codegen] ever emits"
// applies unchanged here). Parses via the same shared grammar
// ../../../grammar/PccAsm.g4; every encoding below was confirmed
// against a real toolchain (`clang --target=i386-none-elf` +
// `llvm-objdump`) during development, not derived from the x86
// reference alone.
//
// `[label]`, `[ebx]`, `[label+ebx]` are this dialect's only three
// addressing-mode shapes (direct/indirect/indexed - exactly
// I8086Codegen.cpp's own three, just `ebx` instead of `bx`); every
// other memory access (anything not through one of those) simply
// doesn't exist in what I386Codegen.cpp ever emits.
//
// One flat blob, `org` called exactly once (see I386Codegen.cpp's own
// `generate()` - the whole image, Multiboot header included, loads
// and runs at one fixed address) - the same single-`org` shape
// ../m68k/M68kAssembler.h's own dialect uses, unlike STM32Assembler.h's
// own flash/RAM split (this target has no such split: a real i386
// booted this way has one flat address space, code and data both).
//
// `db`/`dw`/`dd` emit raw 8/16/32-bit words, used for the Multiboot
// header's own fixed fields and for ordinary global/array data.
//
// Throws std::runtime_error on any assembly error.
std::string assembleI386(const std::string &asmText);

}  // namespace pcc
