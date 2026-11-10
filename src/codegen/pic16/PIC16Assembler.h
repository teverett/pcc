#pragma once

#include <cstdint>
#include <string>

#include "fileformat/ObjectFile.h"

namespace rcc {

// Custom machine ID for this target (no real one registered); checked by
// PIC16LinkerFactory.cpp against an input object's embedded machine type.
constexpr uint16_t kPIC16CoffMachine = 0x16F8;

// Code-space load/start address: the PIC16 hardware reset vector, word 0.
constexpr uint32_t kPIC16BaseAddress = 0;

// GPR file-register bank start (SFRs occupy 0x00-0x0B). In object mode this
// is where the fixed scratch pseudo-registers/software stack below live
// instead, as plain compile-time equates shared by every object so a
// register common to all files sits at one address never overlapping the
// relocatable per-object data space (`kPIC16DataBaseAddress` starts right
// after this block).
constexpr uint32_t kPIC16RamBase = 0x0C;

// Fixed addresses of ac/bc/cx/dx/ptr/SP/FP/SSTACK, matching the layout
// declareRam() uses in whole-program mode (ac/bc/cx/dx 2 bytes, ptr/SP/FP
// 1 byte, SSTACK `kPIC16SStackSize` bytes). `FP` is the current call's
// frame base within `SSTACK`, saved/restored across each `call`.
constexpr uint32_t kPIC16RegAc = kPIC16RamBase;        // 0x0C
constexpr uint32_t kPIC16RegBc = kPIC16RegAc + 2;      // 0x0E
constexpr uint32_t kPIC16RegCx = kPIC16RegBc + 2;      // 0x10
constexpr uint32_t kPIC16RegDx = kPIC16RegCx + 2;      // 0x12
constexpr uint32_t kPIC16RegPtr = kPIC16RegDx + 2;     // 0x14
constexpr uint32_t kPIC16RegSP = kPIC16RegPtr + 1;     // 0x15
constexpr uint32_t kPIC16RegFP = kPIC16RegSP + 1;      // 0x16
constexpr uint32_t kPIC16RegSStack = kPIC16RegFP + 1;  // 0x17
constexpr uint32_t kPIC16SStackSize = 32;

// Start of the Harvard data address space (Globals only - Locals/Params
// live in a frame inside `SSTACK`), right after the fixed pseudo-register
// block so GenericLinker's per-object data-space layout never collides.
constexpr uint32_t kPIC16DataBaseAddress = kPIC16RegSStack + kPIC16SStackSize;  // 0x2E

// Both relocation kinds are additive patches into a sub-field of an
// already-emitted 16-bit instruction cell, not a whole-cell patch, since
// GOTO/CALL targets and file-register addresses are packed alongside
// fixed opcode bits in that same cell. See PIC16LinkerFactory.cpp's
// patchPIC16GotoCall/patchPIC16Data for the masking.
//
// Patches GOTO/CALL's 11-bit field (bits [10:0]). Code space is
// word-addressed, so the resolved byte address is halved before patching.
constexpr uint16_t kPIC16RelocGotoCall = 1;
// Patches the FD/FB family's 7-bit "f" field, or movlw's 8-bit literal
// field when loading a data label's address - both masked the same way
// since the whole GPR bank (0x0C-0x4F) fits in 7 bits. No word/byte
// scaling needed; file-register space is already byte-addressed 1:1.
constexpr uint16_t kPIC16RelocData = 2;
// Patches movlwcl/movlwch's 8-bit literal field with the low/high byte of
// a function's code-space byte address (the same value genAddrOf's
// same-file case already splits locally - see PIC16Codegen.cpp's
// ir::ExprKind::Func case). No word/byte scaling: movlwcl/movlwch always
// operate on the full byte address, unlike GOTO/CALL's word-address field.
constexpr uint16_t kPIC16RelocCodeLo = 3;
constexpr uint16_t kPIC16RelocCodeHi = 4;

// In-process two-pass assembler for the PIC16 mid-range (14-bit core)
// subset PIC16Codegen.cpp emits - not general purpose (no macros, no
// conditional assembly). Produces real PIC16 machine code (each 14-bit
// word packed into a 16-bit little-endian cell) wrapped in Intel HEX.
//
// Parses via the shared grammar ../../../grammar/RccAsm.g4. Two private
// directives exist only for this dialect: `orgram <addr>` sets the
// data-space cursor (the data-space equivalent of `org`, which is the
// code-space word address here); `res <count>` reserves `count` bytes at
// the current `orgram` cursor and binds the line's label to that address.
// A `name = value` line is a compile-time equate, used for the named SFRs
// (INDF/STATUS/FSR/INTCON) and, in object mode, the fixed pseudo-registers
// (`kPIC16Reg*` above).
//
// Throws std::runtime_error on any assembly error.
std::string assemblePIC16(const std::string &asmText);

// Separate-compilation entry point (`--format=coff`/`elf`/`aout`/`objraw`):
// assembles `asmText` like assemblePIC16() does, except an undefined
// symbol becomes a relocation instead of a hard error, and every address
// reference also gets a relocation against whichever reserved pseudo-
// symbol matches its address space: `".."` (linked code-space base) for a
// GOTO/CALL target, `"..d"` (linked data-space base) for a file-register
// address. A compile-time equate (fixed pseudo-registers, SFR names) never
// needs either relocation. Returns the format-neutral ObjectFile a codegen
// backend builds once per compile.
ObjectFile assemblePIC16Object(const std::string &asmText);

}  // namespace rcc
