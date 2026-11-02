#pragma once

#include <cstdint>
#include <string>

#include "objectformat/ObjectFile.h"

namespace pcc {

// This target's own custom, non-registered COFF/object-format
// machine ID (no real one exists for this project's own ARM A32
// dialect - see ../../coff/CoffWriter.h's own header comment) -
// exported so ../../linker/factory/ArmLinkerFactory.cpp can check an
// input object's embedded machine type against `--target=armv7a`
// without duplicating this literal.
constexpr uint16_t kArmCoffMachine = 0xA032;

// Where the whole program loads and starts running once real
// separate compilation (`--format=coff`/`elf`/`macho`/`aout`, see
// `allowExternal` below) and `--link` are both involved - the same
// address ArmCodegen.cpp's own whole-program `--format=raw` entry
// sequence already uses (`kBaseAddress`), kept identical so a linked
// program behaves exactly like a single-file one. Exported so
// ArmLinkerFactory.cpp's own sequential object layout starts from the
// same place.
constexpr uint32_t kArmBaseAddress = 0x1000u;

// This dialect's three relocation kinds - see
// ../../linker/factory/ArmLinkerFactory.cpp's own local patch
// functions (not shared in GenericLinker.h, since classic ARM A32's
// own B/BL "+8" pipeline quirk and MOVW/MOVT imm4:imm12 split aren't
// reusable by any other migrated target):
//   Rel24    - `B`/`BL`'s own 24-bit PC-relative branch field.
//   MovWLo   - `MOVW`'s own low 16 bits of a full address.
//   MovTHi   - `MOVT`'s own high 16 bits of a full address.
constexpr uint16_t kArmRelocRel24 = 1;
constexpr uint16_t kArmRelocMovWLo = 2;
constexpr uint16_t kArmRelocMovTHi = 3;

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

// The real separate-compilation entry point (`--format=coff`/`elf`/
// `macho`/`aout` - see ArmCodegen.h's own `objectMode`): assembles
// `asmText` (generated with `org 0`, not `kArmBaseAddress` - see
// ArmCodegen.cpp's own `generate()`) exactly like assembleArm() does,
// except an undefined symbol is deferred as a relocation instead of a
// hard error, and *every* absolute-address reference - even to a
// label defined in this same file - also gets an additional
// relocation against the reserved pseudo-symbol ".." (see
// ../../linker/GenericLinker.h's own header comment on why: this
// text's own labels all resolve to "offset from this object's own
// org 0" until that base is added). Returns the format-neutral
// ObjectFile a codegen backend builds once per compile (see
// ../../objectformat/ObjectFile.h) - this function never touches a
// real COFF/ELF/Mach-O/a.out byte layout itself.
ObjectFile assembleArmObject(const std::string &asmText);

}  // namespace pcc
