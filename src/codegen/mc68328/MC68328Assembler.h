#pragma once

#include <string>

namespace pcc {

// A from-scratch, in-process two-pass assembler for exactly the
// narrow 68000 dialect MC68328Codegen.cpp emits - the real MC68328
// "DragonBall" CPU core executes a bit-for-bit identical instruction
// set to a plain MC68000 (see MC68328Codegen.h's own header
// comment), so this is otherwise identical to ../m68k/M68kAssembler's
// own encoder (same operand classification, same mnemonic vocabulary,
// same fixed-size-per-mnemonic convention - see that file's own
// header comment for the full rationale this one shares unchanged).
//
// Throws std::runtime_error on any assembly error.
std::string assembleMC68328(const std::string &asmText);

}  // namespace pcc
