#pragma once

#include <cstdint>
#include <string>

#include "objectformat/ObjectFile.h"

namespace pcc {

// This target's own custom, non-registered COFF/object-format
// machine ID (no real one exists for this project's own AArch64
// dialect - see ../../coff/CoffWriter.h's own header comment) -
// exported so ../../linker/factory/Arm64LinkerFactory.cpp can check
// an input object's embedded machine type against `--target=armv8a`
// without duplicating this literal.
constexpr uint16_t kArm64CoffMachine = 0xAA64;

// Where the whole program loads and starts running once real
// separate compilation (`--format=coff`/`elf`/`macho`/`aout`, see
// `allowExternal` below) and `--link` are both involved - the same
// address Arm64Codegen.cpp's own whole-program `--format=raw` entry
// sequence already uses (`kBaseAddress`), kept identical so a linked
// program behaves exactly like a single-file one. Exported so
// Arm64LinkerFactory.cpp's own sequential object layout starts from
// the same place.
constexpr uint64_t kArm64BaseAddress = 0x40080000ull;

// This dialect's three relocation kinds - see Arm64LinkerFactory.cpp
// for their own patch arithmetic (kept local there, not in the
// shared GenericLinker.h, same precedent as M6502LinkerFactory.cpp's
// own split-byte kinds):
//   Rel26  - `b`/`bl`'s own 26-bit word-granularity PC-relative
//            branch-immediate field.
//   MovZLo - `movz`'s own 16-bit immediate: the low 16 bits of a
//            full address.
//   MovKHi - `movk`'s own 16-bit immediate: the high 16 bits.
constexpr uint16_t kArm64RelocRel26 = 1;
constexpr uint16_t kArm64RelocMovZLo = 2;
constexpr uint16_t kArm64RelocMovKHi = 3;

// A from-scratch, in-process two-pass assembler for exactly the
// narrow AArch64 ("A64") dialect Arm64Codegen.cpp emits - not a
// general-purpose A64 assembler. Parses via the same shared grammar
// ../../../grammar/PccAsm.g4 every other backend's own in-process
// assembler uses (see M68kAssembler.h's own header comment). Every
// operand is a plain expression (a number, a register name, or a
// label) classified by Arm64Codegen.cpp's own convention, same "no
// bracketed memory operand" choice MIPS/M16C/STM8 already make (see
// MipsAssembler.h's own header comment on why) - every load/store
// here goes through the dedicated `ldind`/`stind` mnemonics instead,
// always a fixed zero displacement.
//
// Register names recognize both the 32-bit ("W") and 64-bit ("X")
// view of each of the 31 general registers (`w0`..`w30`, `x0`..`x30`)
// plus `wzr`/`xzr` (the zero register) and `sp` (the real hardware
// stack pointer) - exactly real A64 assembly syntax. Which view a
// register is named with on a given instruction's *destination*
// operand is what selects that instruction's own 32-bit-vs-64-bit
// ("sf") encoding, the same way a real A64 assembler infers it - see
// encodeInstr()'s own comment. `ldind`/`stind`'s own base-register
// operand is always 64-bit regardless of its own `w`/`x` spelling
// (real A64 load/store base registers always are), so Arm64Codegen.cpp
// always spells it with an `x` name purely for readability, not
// because this assembler requires it.
//
// `cset<cc>` is spelled as six fixed, single-operand mnemonics
// (`csetlt`/`csetgt`/`csetle`/`csetge`/`cseteq`/`csetne`) rather than
// one `cset rd,cc` mnemonic taking a condition-code operand - this
// grammar's own `expr` has no way to write a bare condition mnemonic
// like `lt` that isn't itself either a register name or a label (an
// `IDENT` is always one or the other to buildOperand() below), so a
// second structurally-distinct operand kind would need inventing just
// for this one mnemonic; six fixed mnemonics avoid that entirely,
// same spirit as `ldind`/`stind` existing instead of a bracketed
// memory operand.
//
// Only the ARMv8-A "A64" 32-bit data-processing instruction subset is
// emitted (this project's own `int`, matching every other backend's
// own word-size choice - see Arm64Codegen.h) wherever the destination
// uses a `w`-register; every instruction here is a real, standard A64
// encoding, so it runs unchanged on any real Armv8-A core. Every
// instruction here is a fixed 4 bytes. Unlike MIPS, A64 branches have
// no delay slot at all, so this assembler (unlike MipsAssembler.cpp)
// never needs to account for one.
//
// Throws std::runtime_error on any assembly error.
std::string assembleArm64(const std::string &asmText);

// The real separate-compilation entry point (`--format=coff`/`elf`/
// `macho`/`aout` - see Arm64Codegen.h's own `objectMode`): assembles
// `asmText` (generated with `org 0`, not `kArm64BaseAddress` - see
// Arm64Codegen.cpp's own `generate()`) exactly like assembleArm64()
// does, except an undefined symbol is deferred as a relocation
// instead of a hard error, and *every* absolute-address reference -
// even to a label defined in this same file - also gets an
// additional relocation against the reserved pseudo-symbol ".." (see
// ../../linker/GenericLinker.h's own header comment on why: this
// text's own labels all resolve to "offset from this object's own
// org 0" until that base is added). Returns the format-neutral
// ObjectFile a codegen backend builds once per compile (see
// ../../objectformat/ObjectFile.h).
ObjectFile assembleArm64Object(const std::string &asmText);

}  // namespace pcc
