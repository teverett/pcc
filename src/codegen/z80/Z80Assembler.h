#pragma once

#include <cstdint>
#include <string>

#include "objectformat/ObjectFile.h"

namespace pcc {

// This target's own custom, non-registered COFF/object-format
// machine ID (no real one exists for the Z80 - see
// ../../coff/CoffWriter.h's own header comment) - exported so
// ../../linker/Z80Linker.cpp can check an input object's embedded
// machine type against `--target=z80` without duplicating this
// literal.
constexpr uint16_t kZ80CoffMachine = 0x5A38;  // ASCII "Z8"

// Where the whole program loads and starts running once real
// separate compilation (`--format=coff`/`elf`/`macho`/`aout`, see
// `allowExternal` below) and `--link` are both involved - the same
// address Z80Codegen.cpp's own whole-program `--format=raw` entry
// sequence already uses (`kBaseAddress`), kept identical so a linked
// program behaves exactly like a single-file one. Exported so
// Z80Linker.cpp's own sequential object layout starts from the same
// place.
constexpr uint32_t kZ80BaseAddress = 0x8000;

// This dialect's one relocation kind: a little-endian 16-bit
// *additive* patch (see ../../linker/GenericLinker.h's own
// `RelocKinds::patchAbs16`) - every Z80 jump/call this backend ever
// emits is a full absolute address (see Z80Codegen.h's own header
// comment: "every jump/call is a full 16-bit absolute address"), so
// unlike 8086 (which also needs a *relative* Rel16 for its own near
// jmp/call - see I8086Assembler.h's own comment) this is the only
// relocation kind the Z80 dialect needs at all.
constexpr uint16_t kZ80RelocAbs16 = 1;

// A from-scratch, in-process two-pass assembler for the subset of
// real Z80 assembly Z80Codegen.cpp emits - not a general-purpose Z80
// assembler (no macros, no conditional assembly - see
// Z80Codegen.h's own header comment). Like ../i8080/I8080Assembler.h, this uses
// real, standard Z80 mnemonics and produces real Z80 machine code -
// every opcode byte emitted here matches Zilog's own published
// encoding (including the `CB`-prefixed bit/rotate group and the
// `ED`-prefixed extended group), so the output is valid input to any
// accurate Z80 (or 8080, for the opcodes the two chips share) emulator
// or disassembler.
//
// Parses via the same shared grammar ../../../grammar/PccAsm.g4 every
// other backend's own in-process assembler uses. Unlike the 8080
// dialect (which rejects bracketed operands outright), this one
// gives brackets a real meaning: `[expr]` always means "the 16-bit
// address `expr` names", used for every instruction whose real Z80
// syntax uses parentheses for a direct memory reference (`LD HL,(nn)`
// spelled `ld hl, [nn]` here, `LD A,(BC)` spelled `ld a, [bc]`, and
// so on) - this disambiguates a register pair's own *value* (bare
// `bc`) from "the byte(s) at the address *in* that pair" (`[bc]`),
// which are two genuinely different real instructions. A register
// name (`a`, `b`, ..., `bc`, `af`) is otherwise just another bare
// identifier to
// the shared grammar - this file is what decides, per mnemonic and
// operand position, whether a given identifier names a register (or
// register pair) or a symbol to resolve against the label table.
//
// Every instruction's *size* is fixed by its mnemonic alone, never by
// any operand's resolved value - same convention, and the same
// reason, as every other backend's own in-process assembler.
//
// Throws std::runtime_error on any assembly error.
std::string assembleZ80(const std::string &asmText);

// The real separate-compilation entry point (`--format=coff`/`elf`/
// `macho`/`aout` - see Z80Codegen.h's own `objectMode`): assembles
// `asmText` (generated with `org 0`, not `kBaseAddress` - see
// Z80Codegen.cpp's own `generate()`) exactly like assembleZ80() does,
// except an undefined symbol is deferred as a relocation (see
// ../../objectformat/ObjectFile.h's own `ObjRelocation`) instead of a
// hard error, and *every* address reference - even to a label defined
// in this same file - also gets an additional relocation against the
// reserved pseudo-symbol ".." (this object's own eventual linked
// base, resolved only by a linker - see ../../linker/GenericLinker.h's
// own header comment on why: this text's own labels all resolve to
// "offset from this object's own org 0" until that base is added,
// exactly I8086Assembler.cpp's own `emitExprAsWord`/`needsBaseShift`
// convention). Returns the format-neutral ObjectFile a codegen
// backend builds once per compile (see ObjectFile.h) - this function
// never touches a real COFF/ELF/Mach-O/a.out byte layout itself; that
// happens only in whichever ObjectFormatWriter the caller picks for
// the requested `--format` (see ../../objectformat/ObjectFormat.h).
ObjectFile assembleZ80Object(const std::string &asmText);

}  // namespace pcc
