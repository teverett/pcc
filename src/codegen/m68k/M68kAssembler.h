#pragma once

#include <cstdint>
#include <string>

#include "fileformat/ObjectFile.h"

namespace pcc {

// This target's own custom, non-registered COFF/object-format
// machine ID (no real one exists for this project's own 68000
// dialect - see ../../fileformat/coff/CoffFormat.h's own header comment) -
// exported so ../../linker/M68kLinkerFactory.cpp can check an input
// object's embedded machine type against `--target=68000` without
// duplicating this literal. Distinct from mc68328's own machine ID
// even though both share the exact same 68000 instruction set (see
// MC68328Assembler.h) - the two backends' own entry sequences differ
// (this one's simulator-sets-PC-directly convention vs. mc68328's
// real reset-vector table), so they're never meant to be linked
// against each other's own objects.
constexpr uint16_t kM68kCoffMachine = 0x6800;

// Where the whole program loads and starts running once real
// separate compilation (`--format=coff`/`elf`/`macho`/`aout`, see
// `allowExternal` below) and `--link` are both involved - the same
// address M68kCodegen.cpp's own whole-program `--format=raw` entry
// sequence already uses (`kBaseAddress`), kept identical so a linked
// program behaves exactly like a single-file one. Exported so
// M68kLinkerFactory.cpp's own sequential object layout starts from
// the same place.
constexpr uint32_t kM68kBaseAddress = 0x1000;

// This dialect's one relocation kind: a big-endian 32-bit *additive*
// patch (see ../../linker/GenericLinker.h's own
// `RelocKinds::patchAbs32BE` - this target is real Motorola 68000
// hardware, which is big-endian throughout, unlike z80/8086/6502).
// Every absolute-address reference this backend ever emits - `JSR`'s
// own absolute-long effective address, `LEA`'s own, `MOVE.L`'s own -
// goes through the one shared `emitEAExtension()` choke point (see
// M68kAssembler.cpp), and `Bcc`/`BRA` are always intra-function-only
// (M68kCodegen.h's own header comment: real condition-code branches,
// never used to reach another file's own code), so this is the only
// relocation kind this dialect needs at all - no split hi/lo halves
// (unlike ARM/MIPS/PowerPC) and no separate relative-branch kind
// (unlike 8086's extra Rel16).
constexpr uint16_t kM68kRelocAbs32 = 1;

// A from-scratch, in-process two-pass assembler for exactly the
// narrow 68000 dialect M68kCodegen.cpp emits - not a general-purpose
// 68000 assembler. Parses via the same shared grammar
// ../../../grammar/PccAsm.g4 the other backends' own in-process
// assemblers use (see Pdp7Assembler.h/I8086Assembler.h/
// M6502Assembler.h's own header comments). Every operand is a plain
// expression (a number, a register name, or a label) classified by
// M68kCodegen.cpp's own convention, not real `#imm`/`(An)`-style
// 68000 syntax - the grammar has no `#` token and no way to
// distinguish "(expr)" (a parenthesized group, already used
// elsewhere) from "(An)" (register-indirect), so every addressing
// mode this dialect can't express as a bare register-or-label
// operand gets its own dedicated mnemonic instead (`ldind`/`stind`
// for `(An)`, `pushl`/`popl` for `-(A7)`/`(A7)+`) - see
// M68kAssembler.cpp's own comment for the full mnemonic list.
//
// Only this project's 32-bit (.L) forms are implemented (the
// language's own `int`, chosen to match this target's native
// register width - see M68kCodegen.h), plus the two 16-bit (.W)
// forms (`MULS.W`/`DIVS.W`) that are the *only* multiply/divide this
// CPU has at all.
//
// Every instruction's size is fixed by its mnemonic alone, same
// convention (and the same reason) as every other backend's own
// assembler.
//
// Throws std::runtime_error on any assembly error.
std::string assembleM68k(const std::string &asmText);

// The real separate-compilation entry point (`--format=coff`/`elf`/
// `macho`/`aout` - see M68kCodegen.h's own `objectMode`): assembles
// `asmText` (generated with `org 0`, not `kM68kBaseAddress` - see
// M68kCodegen.cpp's own `generate()`) exactly like assembleM68k()
// does, except an undefined symbol is deferred as a relocation
// instead of a hard error, and *every* absolute-address reference -
// even to a label defined in this same file - also gets an
// additional relocation against the reserved pseudo-symbol ".." (see
// ../../linker/GenericLinker.h's own header comment on why: this
// text's own labels all resolve to "offset from this object's own
// org 0" until that base is added). Returns the format-neutral
// ObjectFile a codegen backend builds once per compile (see
// ../../fileformat/ObjectFile.h) - this function never touches a
// real COFF/ELF/Mach-O/a.out byte layout itself.
ObjectFile assembleM68kObject(const std::string &asmText);

}  // namespace pcc
