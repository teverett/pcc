#pragma once

#include <cstdint>
#include <string>

#include "objectformat/ObjectFile.h"

namespace pcc {

// This target's own custom, non-registered COFF/object-format
// machine ID (no real one exists for this project's own PDP-11
// dialect - see ../../coff/CoffWriter.h's own header comment) -
// exported so ../../linker/factory/Pdp11LinkerFactory.cpp can check
// an input object's embedded machine type against `--target=pdp11`
// without duplicating this literal.
constexpr uint16_t kPdp11CoffMachine = 0x1121;

// Where the whole program loads and starts running once real
// separate compilation (`--format=coff`/`elf`/`macho`/`aout`, see
// `allowExternal` below) and `--link` are both involved - the same
// address Pdp11Codegen.cpp's own whole-program `--format=raw` entry
// sequence already uses (its own private `kBaseAddress`), kept
// identical so a linked program behaves exactly like a single-file
// one. Exported so Pdp11LinkerFactory.cpp's own sequential object
// layout starts from the same place.
constexpr uint32_t kPdp11BaseAddress = 0x1000;

// This dialect's two relocation kinds - see Pdp11Assembler.cpp's own
// `emitEAExtension`/`encodeLea` for exactly which operand shape emits
// which:
//
// `kPdp11RelocAbs16` - a plain little-endian 16-bit *additive* patch
// (see ../../linker/GenericLinker.h's own `RelocKinds::patchAbs16`),
// used only by `lea label,Rn`'s own PC-*immediate* extension word -
// the one place this dialect ever treats a label as a true absolute
// address value rather than a PC-relative displacement.
constexpr uint16_t kPdp11RelocAbs16 = 1;
// `kPdp11RelocRel16` - this dialect's own PC-*relative* extension-word
// formula, `value - siteFinalAddr` (the site's own final linked
// address, with **no +2 adjustment** - unlike
// ../../linker/GenericLinker.h's own `RelocKinds::patchRel16`, which
// assumes the 8086-style "offset from the end of this field"
// convention). Confirmed from `emitEAExtension`'s own `Mem` case
// below: the extension word is written as `target - extAddr` where
// `extAddr` is that *same* word's own address, not the address right
// after it. Used by every bare-label operand *other* than `lea`'s
// source - an ordinary global variable read/write, and `jsr label`
// (which routes through this exact same general EA path - see
// Pdp11Assembler.cpp's own `encodeInstr`'s `jsr` case) - so a cross-
// file `jsr fn_main` needs this same relocation kind, not a distinct
// call-specific one. A *local* (same-object) Mem-mode reference never
// needs this relocation at all: both the target and the extension
// word's own address are already final, position-independent, zero-
// based offsets within this one object, so their difference is
// already the correct final answer with no base-shift ever required -
// see Pdp11LinkerFactory.cpp's own locally-scoped patch function for
// the arithmetic this kind needs at link time.
constexpr uint16_t kPdp11RelocRel16 = 2;

// A from-scratch, in-process two-pass assembler for exactly the
// narrow PDP-11 dialect Pdp11Codegen.cpp emits - not a general-
// purpose PDP-11 (MACRO-11) assembler. Parses via the same shared
// grammar ../../../grammar/PccAsm.g4 the other backends' own in-
// process assemblers use (see M68kAssembler.h/M6502Assembler.h/
// Pdp7Assembler.h's own header comments). Every operand is a plain
// expression (a number, a register name, or a label) classified by
// Pdp11Codegen.cpp's own convention, not real MACRO-11 `(Rn)`/`#n`/
// `@#n` syntax - the grammar has no `#`/`@` token and no way to
// distinguish "(expr)" (a parenthesized group, already used
// elsewhere) from "(Rn)" (register-deferred), so every addressing
// mode this dialect can't express as a bare register-or-label
// operand gets its own dedicated mnemonic instead (`ldind`/`stind`
// for `(Rn)`, `push`/`pop` for `-(SP)`/`(SP)+`, `lea` for "the
// address of a label" rather than MACRO-11's own `MOV #label,Rn`
// idiom) - see Pdp11Assembler.cpp's own comment for the full
// mnemonic list.
//
// A bare label operand (anywhere other than `lea`'s own source) is
// always PDP-11 "relative" addressing (mode 067: index mode with
// PC as the base register) - the real hardware addressing mode
// MACRO-11 itself assembles a bare symbol to - which reads/writes
// the *value* stored at that label, exactly like an ordinary global
// variable access; `lea`'s own source is the one place a label means
// "give me this address as a value instead", via PC-relative
// *immediate* addressing (mode 027) with the label's absolute
// address as the literal.
//
// This project's own 16-bit `int` matches the PDP-11's native word
// size, so every instruction here is an ordinary word (no `.B`-
// suffixed byte op is ever emitted).
//
// Throws std::runtime_error on any assembly error.
std::string assemblePdp11(const std::string &asmText);

// The real separate-compilation entry point (`--format=coff`/`elf`/
// `macho`/`aout` - see Pdp11Codegen.h's own `objectMode`): assembles
// `asmText` (generated with `org 0`, not `kPdp11BaseAddress` - see
// Pdp11Codegen.cpp's own `generate()`) exactly like assemblePdp11()
// does, except an undefined symbol is deferred as a relocation
// instead of a hard error - see `kPdp11RelocAbs16`/`kPdp11RelocRel16`
// above for exactly which kind each operand shape needs. Returns the
// format-neutral ObjectFile a codegen backend builds once per compile
// (see ../../objectformat/ObjectFile.h) - this function never touches
// a real COFF/ELF/Mach-O/a.out byte layout itself.
ObjectFile assemblePdp11Object(const std::string &asmText);

}  // namespace pcc
