#pragma once

#include <cstdint>
#include <string>

#include "fileformat/ObjectFile.h"

namespace pcc {

// Custom, non-registered COFF/object-format machine ID for this project's
// ARM A32 dialect, checked by ArmLinkerFactory.cpp against `--target=armv7a`.
constexpr uint16_t kArmCoffMachine = 0xA032;

// Where the whole program loads and starts running for `--format=raw`,
// and the base ArmLinkerFactory.cpp's sequential object layout starts from.
constexpr uint32_t kArmBaseAddress = 0x1000u;

// This dialect's three relocation kinds, patched by
// ../../linker/factory/ArmLinkerFactory.cpp:
//   Rel24    - `B`/`BL`'s 24-bit PC-relative branch field.
//   MovWLo   - `MOVW`'s low 16 bits of a full address.
//   MovTHi   - `MOVT`'s high 16 bits of a full address.
constexpr uint16_t kArmRelocRel24 = 1;
constexpr uint16_t kArmRelocMovWLo = 2;
constexpr uint16_t kArmRelocMovTHi = 3;

// An in-process two-pass assembler for the narrow ARMv7-A "A32" dialect
// ArmCodegen.cpp emits, not a general-purpose ARM assembler. Parses via
// the shared grammar ../../../grammar/PccAsm.g4. Every operand is a plain
// expression (a number, register name, or label), not real ARM UAL
// syntax: no `#` immediate prefix, no `[Rn, #imm]` memory operand, no
// shifted-register operand2 syntax. Addressing modes the grammar can't
// express get a dedicated mnemonic instead (`ldind`/`stind`, always a
// fixed zero offset; `lsl`/`lsr` with a register or immediate third
// operand), and `MOVW`/`MOVT` resolve a label operand to its lower/upper
// 16 bits automatically, with no `:lower16:`/`:upper16:` syntax needed.
//
// Only A32 (not Thumb) encodings are emitted; every instruction is a
// fixed 4 bytes.
//
// Throws std::runtime_error on any assembly error.
std::string assembleArm(const std::string &asmText);

// The separate-compilation entry point (`--format=coff`/`elf`/`macho`/
// `aout`): assembles `asmText` (generated with `org 0`) like assembleArm()
// does, except an undefined symbol is deferred as a relocation instead of
// a hard error, and every absolute-address reference - even to a label
// defined in this same file - also gets a relocation against the
// reserved pseudo-symbol ".." (this text's labels resolve to "offset
// from org 0" until that base is added). Returns the format-neutral
// ObjectFile a codegen backend builds once per compile.
ObjectFile assembleArmObject(const std::string &asmText);

}  // namespace pcc
