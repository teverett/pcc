#pragma once

#include <cstdint>
#include <string>

#include "fileformat/ObjectFile.h"

namespace pcc {

// This target's own custom, non-registered COFF/object-format
// machine ID (no real one exists for the MC68328 - see
// ../../fileformat/coff/CoffFormat.h's own header comment) - exported so
// ../../linker/MC68328LinkerFactory.cpp can check an input object's
// embedded machine type against `--target=mc68328` without
// duplicating this literal. Deliberately different from ../m68k's
// own `kM68kCoffMachine` even though both targets execute a bit-for-
// bit identical instruction set (see MC68328Codegen.h's own header
// comment) - an object compiled for one is never meant to link
// against one compiled for the other, since only this backend's own
// object model expects the linker to lay out a real reset vector
// table (see MC68328Codegen.cpp's own `generate()`).
constexpr uint16_t kMC68328CoffMachine = 0x6328;

// Where `_start` would live in a real, hardware-accurate reset-vector
// image - see MC68328Codegen.h's own header comment. Only meaningful
// outside object mode (a whole-program `--format=raw` compile lays
// the 2-word vector table out itself, right before this address);
// in object mode this backend instead uses `org 0`, the same "the
// linker lays it out, not me" convention every other GenericLinker-
// based target's own assembler uses (see Z80Assembler.h's own header
// comment) - the real reset vector table becomes
// ../../linker/MC68328LinkerFactory.cpp's own synthesized entry
// object's job, exactly once, for the final linked program.
constexpr uint32_t kMC68328StartAddress = 8;

// This dialect's one relocation kind: a big-endian 32-bit *additive*
// patch (see ../../linker/GenericLinker.h's own `RelocKinds::
// patchAbs32BE`) - every absolute-address reference this backend's
// own `emitEAExtension()` ever emits (`JSR <abs32>`, `MOVE.L
// <abs32>,Dn`) is already a full 32-bit field, and Bcc/BRA are
// always intra-function-only (never a cross-file reference), so
// there's no second, relative, relocation kind needed the way 8086's
// own dialect needs one.
constexpr uint16_t kMC68328RelocAbs32 = 1;

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

// The real separate-compilation entry point (`--format=coff`/`elf`/
// `macho`/`aout` - see MC68328Codegen.h's own `objectMode`):
// assembles `asmText` (generated with `org 0`, not a real reset
// vector table - see generate()'s own comment) exactly like
// assembleMC68328() does, except an undefined symbol is deferred as
// a relocation instead of a hard error, and *every* absolute-address
// reference - even to a label defined in this same file - also gets
// an additional relocation against the reserved pseudo-symbol ".."
// (this object's own eventual linked base, resolved only by a linker
// - see ../../linker/GenericLinker.h's own header comment), since
// this text's own labels all resolve to "offset from this object's
// own org 0" until that base is added. Returns the format-neutral
// ObjectFile a codegen backend builds once per compile (see
// ../../fileformat/ObjectFile.h) - this function never touches a
// real COFF/ELF/Mach-O/a.out byte layout itself.
ObjectFile assembleMC68328Object(const std::string &asmText);

}  // namespace pcc
