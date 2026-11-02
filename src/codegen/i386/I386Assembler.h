#pragma once

#include <cstdint>
#include <string>

#include "objectformat/ObjectFile.h"

namespace pcc {

// This target's own custom, non-registered COFF/object-format
// machine ID (no real one exists for this project's own i386 dialect
// - see ../../coff/CoffWriter.h's own header comment) - exported so
// ../../linker/factory/I386LinkerFactory.cpp can check an input
// object's embedded machine type against `--target=i386` without
// duplicating this literal.
constexpr uint16_t kI386CoffMachine = 0x3386;

// Where the whole program loads and runs once real separate
// compilation (`--format=coff`/`elf`/`macho`/`aout`, see
// `allowExternal` below) and `--link` are both involved - the same
// Multiboot load address I386Codegen.cpp's own whole-program
// `--format=raw` entry sequence already uses (`kLoadAddr`), kept
// identical so a linked program behaves exactly like a single-file
// one. Exported so I386LinkerFactory.cpp's own sequential object
// layout, and its synthesized entry object's own Multiboot header,
// both start from the same place.
constexpr uint32_t kI386LoadAddr = 0x100000;

// This dialect's two relocation kinds - see I386Assembler.cpp's own
// `emitModRMMem`/`mov`/`call`/`jmp` comments for exactly where each is
// recorded. Both are plain contiguous 32-bit fields (no hi/lo split,
// unlike ARM/MIPS/PowerPC), patched via the shared
// ../../linker/GenericLinker.h's own `RelocKinds::patchAbs32`/
// `patchRel32`.
constexpr uint16_t kI386RelocAbs32 = 1;  // `[label]` direct memory operand (mov eax,[label] / mov [label],eax)
constexpr uint16_t kI386RelocRel32 = 2;  // `call`/`jmp label` (E8/E9 rel32)

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

// The real separate-compilation entry point (`--format=coff`/`elf`/
// `macho`/`aout` - see I386Codegen.h's own `objectMode`): assembles
// `asmText` (generated with `org 0`, not `kI386LoadAddr` - see
// I386Codegen.cpp's own `generate()`) exactly like assembleI386() does,
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
ObjectFile assembleI386Object(const std::string &asmText);

}  // namespace pcc
