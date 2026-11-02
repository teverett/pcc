#pragma once

#include <string>

namespace pcc {

// A from-scratch, in-process two-pass assembler for exactly the
// narrow ARMv7-A "A32" dialect ArmCodegen.cpp emits - not a
// general-purpose ARM assembler. Parses via the same shared grammar
// ../../../grammar/PccAsm.g4 every other backend's own in-process
// assembler uses (see M68kAssembler.h's own header comment). Every
// operand is a plain expression (a number, a register name, or a
// label) classified by ArmCodegen.cpp's own convention, not real ARM
// UAL syntax - no `#` immediate prefix, no `[Rn, #imm]` memory
// operand, no `{Rd}, {Rn} LSL Rm` shifted-register operand2 syntax -
// the grammar has no way to express any of those, so every
// addressing mode this dialect can't express as a bare
// register-or-label operand instead gets its own dedicated mnemonic
// (`ldind`/`stind`, always a fixed zero offset - see ArmCodegen.h's
// own header comment on why every load/store here is always through
// a freshly computed address; `lsl`/`lsr` with either a register or
// an immediate third operand - see encodeShift()'s own comment), and
// `MOVW`/`MOVT` resolve a *label* operand (as opposed to a plain
// number) to that label's own lower/upper 16 bits automatically
// instead of needing any `:lower16:`/`:upper16:` syntax spelled out
// at all.
//
// Only real A32 (32-bit ARM, not Thumb) encodings are emitted - this
// project's own `int` is 32 bits, matching this target's native
// register width, same reasoning as every other backend's own
// word-size choice (see ArmCodegen.h). Every instruction here is a
// fixed 4 bytes, same convention (and the same reason) as every
// other backend's own assembler - A32 has no branch-delay slot the
// way MIPS does, so (unlike MipsAssembler.h) there's nothing here for
// ArmCodegen.cpp to need to fill at all.
//
// Throws std::runtime_error on any assembly error.
std::string assembleArm(const std::string &asmText);

}  // namespace pcc
