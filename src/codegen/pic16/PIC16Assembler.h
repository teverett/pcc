#pragma once

#include <cstdint>
#include <string>

#include "fileformat/ObjectFile.h"

namespace pcc {

// This target's own custom, non-registered COFF/object-format
// machine ID (no real one exists for the PIC16 mid-range core - see
// ../../fileformat/coff/CoffFormat.h's own header comment) - exported so
// ../../linker/factory/PIC16LinkerFactory.cpp can check an input
// object's embedded machine type against `--target=pic16` without
// duplicating this literal.
constexpr uint16_t kPIC16CoffMachine = 0x16F8;

// Where the whole program's code space loads and starts running once
// real separate compilation (`--format=coff`/`elf`/`aout`/`objraw`,
// see `assemblePIC16Object` below) and `--link` are both involved -
// the real PIC16 hardware reset vector, word 0 of program memory, the
// same address PIC16Codegen.cpp's own whole-program `--format=hex`
// entry sequence already starts at (`org 0`). Exported so
// PIC16LinkerFactory.cpp's own sequential object layout starts from
// the same place - this target's code space is word-addressed (see
// PIC16Codegen.h's own header comment), but every `ObjectFile`/
// `GenericLinker` byte offset here is still a plain *byte* count
// (ObjectFile.h's own convention), so `kPIC16BaseAddress` is in bytes
// too (0, the only value where "byte 0" and "word 0" coincide
// trivially).
constexpr uint32_t kPIC16BaseAddress = 0;

// This target's own real GPR file-register bank start (see
// PIC16Codegen.h's own header comment: SFRs occupy 0x00-0x0B, the
// 68-byte GPR bank 0x0C-0x4F starts here) - PIC16Assembler.cpp's own
// whole-program-mode `orgram` cursor already starts here. In object
// mode this is instead where PIC16Codegen.cpp's own fixed, shared
// scratch pseudo-registers and software stack live (see the
// `kPIC16Reg*`/`kPIC16SStackSize` constants below) - declared as
// plain compile-time equates, identically by *every* object (this
// backend's own entry object included - see PIC16LinkerFactory.cpp),
// exactly like ../i8008/I8008Assembler.h's own `ac`/`bc`/.../`sp` or
// ../mcs51/MCS51Codegen.h's own `ac`/`bc`/.../`ptr`: a register every
// function in every file shares has to sit at one fixed address no
// matter which object's own code touches it, so it can never be part
// of any one object's own relocatable per-object data-space
// allocation (`kPIC16DataBaseAddress` below starts immediately after
// this fixed block, so the two never overlap).
constexpr uint32_t kPIC16RamBase = 0x0C;

// `ac`/`bc`/`cx`/`dx`/`ptr`/`SP`/`FP`/`SSTACK`'s own fixed addresses -
// see `kPIC16RamBase`'s own comment above for why these are plain
// equates, not relocatable data-space symbols, in object mode. Packed
// in the same order, and at the same addresses, as
// PIC16Codegen.cpp's own non-object-mode `declareRam()` calls already
// place them (ac/bc/cx/dx 2 bytes each, ptr/SP/FP 1 byte each, SSTACK
// `kPIC16SStackSize` bytes). `FP` is the current call's own frame
// base within `SSTACK` (see PIC16Codegen.h's own header comment on
// the frame layout genFunction()'s prologue/epilogue build there) -
// every call gets a fresh one, saved/restored across a `call` the
// same way `ac`/`bc`/etc never need to be (see PIC16Codegen.cpp's own
// `emitPrologue`/`emitEpilogue`).
constexpr uint32_t kPIC16RegAc = kPIC16RamBase;        // 0x0C
constexpr uint32_t kPIC16RegBc = kPIC16RegAc + 2;      // 0x0E
constexpr uint32_t kPIC16RegCx = kPIC16RegBc + 2;      // 0x10
constexpr uint32_t kPIC16RegDx = kPIC16RegCx + 2;      // 0x12
constexpr uint32_t kPIC16RegPtr = kPIC16RegDx + 2;     // 0x14
constexpr uint32_t kPIC16RegSP = kPIC16RegPtr + 1;     // 0x15
constexpr uint32_t kPIC16RegFP = kPIC16RegSP + 1;      // 0x16
constexpr uint32_t kPIC16RegSStack = kPIC16RegFP + 1;  // 0x17
constexpr uint32_t kPIC16SStackSize = 32;

// Where this target's own second, Harvard data address space
// (ObjectFile::dataSize/ObjSymbol::isData - every real Global, the
// only kind of Slot still a named data-space `res` symbol; a Local or
// Param instead lives in a stack frame inside `SSTACK` above, addressed
// through `FP`, never through a data-space symbol of its own at all -
// see PIC16Codegen.h's own header comment) starts in real GPR-bank
// addressing - immediately after the fixed pseudo-register block, so
// GenericLinker's own sequential per-object data-space layout (see
// GenericLinker.h's own `dataBaseAddress`) never collides with it.
// Exported so PIC16LinkerFactory.cpp's own
// `GenericLinkerConfig::dataBaseAddress` uses the same value.
constexpr uint32_t kPIC16DataBaseAddress = kPIC16RegSStack + kPIC16SStackSize;  // 0x2E

// This dialect's two relocation kinds - both a masked, *additive*
// patch into a sub-field of one already-emitted 16-bit instruction
// cell, never a plain whole-cell patch (unlike every byte-addressed
// GenericLinker-based target's own Abs16/Abs32 - see GenericLinker.h's
// own `RelocKinds`): every GOTO/CALL's own target and every file-
// register address this backend's own encoder emits are already
// packed alongside that instruction's own fixed opcode bits in the
// very same 16-bit cell (see PIC16Assembler.cpp's own
// `encodeFD`/`encodeFB`/`encodeInstr` "movwf"/"clrf"/"goto"/"call"
// cases), so a patch has to add only into the field's own bit
// positions, never touch the opcode bits above it - see
// PIC16LinkerFactory.cpp's own `patchPIC16GotoCall`/`patchPIC16Data`
// for the exact masking.
//
// `kPIC16RelocGotoCall` patches GOTO/CALL's own 11-bit field (bits
// [10:0] of the cell) - this target's code space is word-addressed
// (see PIC16Codegen.h's own header comment), so unlike every byte-
// addressed target's own relocation this one also has to divide the
// resolved *byte* address (every ObjectFile/".."/"..d" value is
// always in bytes - see ObjectFile.h's own header comment) by two:
// every instruction this backend ever assembles is exactly one 16-bit
// (2-byte) cell, so a code-space byte address is always exactly twice
// its real word address.
constexpr uint16_t kPIC16RelocGotoCall = 1;
// `kPIC16RelocData` patches either of this dialect's own two data-
// space field shapes - the FD/FB/`movwf`/`clrf` family's own 7-bit
// "f" field (bits [6:0]) and `movlw`'s own 8-bit literal field (bits
// [7:0]) when it's loading a data label's own address
// (PIC16Codegen.cpp's own `genAddrOf()`/`genIndexAddr()`, never a
// plain numeric literal, which never carries a relocation at all) -
// both masked the same way (bits [6:0]), since this target's entire
// GPR bank (0x0C-0x4F) fits in 7 bits, so bit 7 of that 8-bit literal
// field is always 0 both before and after this patch. No word/byte
// scaling needed here (file-register space is already byte-addressed
// 1:1, matching every ObjectFile/".."/"..d" value's own units
// directly).
constexpr uint16_t kPIC16RelocData = 2;

// A from-scratch, in-process two-pass assembler for the subset of
// real PIC16 mid-range (14-bit core) assembly PIC16Codegen.cpp emits -
// not a general-purpose PIC assembler (no macros, no conditional
// assembly - see PIC16Codegen.h's own header comment). Uses real,
// standard PIC16 mnemonics and produces real PIC16 machine code (each
// 14-bit instruction word packed into a 16-bit little-endian cell,
// the exact on-disk shape Microchip's own Intel-HEX output uses)
// wrapped in a real Intel HEX container - the output is valid input
// to any accurate PIC16 (mid-range core) simulator/programmer, not
// just this project's own in-process one.
//
// Parses via the same shared grammar ../../../grammar/PccAsm.g4 every
// other backend's own in-process assembler uses. Two private
// directives exist only for this dialect (see PIC16Codegen.h's own
// header comment on the Harvard split between code and data space):
// `orgram <addr>` sets the data-space (file-register) address cursor,
// the data-space equivalent of the ordinary `org` directive (which
// here always means the code-space word address); `res <count>`
// reserves `count` bytes of data space at the current `orgram`
// cursor and binds the line's own label to that address, advancing
// the cursor by `count`. A plain `name = value` line is a compile-
// time equate (same dialect feature, and the same M6502Assembler.cpp
// precedent, as the 6502 backend's own `ptr = 0` SFR-style bindings) -
// used here both for the handful of named special-function registers
// (INDF/STATUS/FSR/INTCON) this codegen's own instructions reference
// by name, and (in object mode only - see `assemblePIC16Object` below)
// for the fixed, shared pseudo-registers (`kPIC16Reg*` above).
//
// Throws std::runtime_error on any assembly error.
std::string assemblePIC16(const std::string &asmText);

// The real separate-compilation entry point (`--format=coff`/`elf`/
// `aout`/`objraw` - see PIC16Codegen.h's own `objectMode`): assembles
// `asmText` (generated with `org 0` and, for every real global/local/
// param, an `orgram`-cursor-0-relative `res` directive - see
// PIC16Codegen.cpp's own `generate()`/`declareRam()`) exactly like
// assemblePIC16() does, except an undefined symbol is deferred as a
// relocation instead of a hard error, and *every* address reference -
// even to a label defined in this same file - also gets an additional
// relocation against whichever of the two reserved pseudo-symbols
// matches its own address space: `".."` (this object's own linked
// *code*-space base) for a GOTO/CALL target, `"..d"` (this object's
// own linked *data*-space base) for a file-register address - see
// ../../linker/GenericLinker.h's own header comment on both. A plain
// compile-time equate (the fixed pseudo-registers, or the SFR names)
// is never treated as needing either relocation - the same value in
// every object regardless of where it's linked, see
// PIC16Assembler.cpp's own `equateNames_`. Returns the format-neutral
// ObjectFile a codegen backend builds once per compile (see
// ../../fileformat/ObjectFile.h) - this function never touches a
// real COFF/ELF/a.out byte layout itself.
ObjectFile assemblePIC16Object(const std::string &asmText);

}  // namespace pcc
