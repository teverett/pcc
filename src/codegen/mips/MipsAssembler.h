#pragma once

#include <string>

namespace pcc {

// A from-scratch, in-process two-pass assembler for exactly the
// narrow MIPS dialect MipsCodegen.cpp emits - not a general-purpose
// MIPS assembler. Parses via the same shared grammar
// ../../../grammar/PccAsm.g4 every other backend's own in-process
// assembler uses (see M68kAssembler.h's own header comment). Every
// operand is a plain expression (a number, a register name, or a
// label) classified by MipsCodegen.cpp's own convention, not real
// `0(reg)`/`%hi()`/`%lo()` MIPS syntax - the grammar has no way to
// express a displacement-from-register memory operand or a function-
// style `%hi`/`%lo` modifier, so every addressing mode this dialect
// can't express as a bare register-or-label operand instead gets its
// own dedicated mnemonic (`ldind`/`stind`, always a fixed zero
// displacement - see MipsCodegen.h's own header comment on why every
// load/store here is always through a freshly computed address), and
// `LUI`/`ORI` resolve a *label* operand (as opposed to a plain
// number) to that label's own upper/lower 16 bits automatically
// instead of needing `%hi`/`%lo` spelled out at all.
//
// Only 32-bit MIPS I instructions are emitted (this project's own
// `int`, chosen to match this target's native register width, same
// reasoning as every other backend's own word-size choice - see
// MipsCodegen.h) - every one of them already part of the MIPS I
// instruction set, so it runs unchanged on a real R10000 (MIPS IV).
//
// Every instruction here is a fixed 4 bytes, same convention (and
// the same reason) as every other backend's own assembler. Branch-
// delay slots are MipsCodegen.cpp's own concern (it always emits an
// explicit `NOP` right after any branch/jump - see its own header
// comment); this assembler just encodes whatever instruction stream
// it's given, delay slot or not.
//
// Throws std::runtime_error on any assembly error.
std::string assembleMips(const std::string &asmText);

}  // namespace pcc
