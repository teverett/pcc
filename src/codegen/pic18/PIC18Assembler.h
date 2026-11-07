#pragma once

#include <cstdint>
#include <string>

#include "fileformat/ObjectFile.h"

namespace pcc {

// Custom machine ID for this target (no real one registered); checked by
// PIC18LinkerFactory.cpp against an input object's embedded machine type.
// Distinct from PIC16's ID.
constexpr uint16_t kPIC18CoffMachine = 0xFC18;

// Code-space load/start address: the hardware reset vector (0x000000).
constexpr uint32_t kPIC18BaseAddress = 0;

// Start of the Harvard DATA address space. The first 84 bytes of the
// Access Bank GPR window (0x00-0x53) are permanently reserved, identically
// in every object, for the fixed pseudo-registers (ac/bc/cx/dx/ptr/SP/
// SSTACK/FP/NFP/FSTACK), declared as compile-time equates rather than
// relocated per object. 0x54 is where `ramOffset_` lands once those are
// allocated in order (ac 2, bc 2, cx 2, dx 2, ptr 1, SP 1, SSTACK 24, FP 1,
// NFP 1, FSTACK 48).
constexpr uint32_t kPIC18DataBaseAddress = 0x54;

// GOTO/CALL's 20-bit absolute word address needs three relocation kinds
// because GenericLinker always resolves to a byte address, which must be
// halved before adding to `k`'s word offset, and the resulting 20-bit sum
// spans two 16-bit instruction words: low 8 bits in the first word's low
// byte, next 8 in the second word's low byte, top 4 in the second word's
// high byte's low nibble. Unlike PIC16's 11-bit GOTO/CALL, this chip's
// full 20-bit field spans its entire 16K-word program memory, so no
// page-select bookkeeping is needed.
constexpr uint16_t kPIC18RelocGotoCallLo = 1;
constexpr uint16_t kPIC18RelocGotoCallMid = 2;
constexpr uint16_t kPIC18RelocGotoCallHiNibble = 3;

// Every data-space reference (the 8-bit Access-Bank `f` operand, and
// MOVFF's 12-bit fs/fd operand) needs one single-byte additive patch:
// already byte-addressed, and every resolved address stays well under
// 0x100 since this backend never lets linked data space exceed the
// 96-byte Access Bank window, so MOVFF's fixed high nibble never needs
// touching. PIC18LinkerFactory.cpp's `patchDataAbs8` bound-checks this.
constexpr uint16_t kPIC18RelocDataAbs8 = 4;

// In-process two-pass assembler for the PIC18 subset PIC18Codegen.cpp
// emits - not general purpose (no macros, no conditional assembly, no
// `--format=coff`). Produces real PIC18 machine code (16-bit words,
// little-endian) wrapped in Intel HEX.
//
// Same shared grammar and the same `orgram`/`res`/`=`-equate directives as
// PIC16Assembler.h's dialect, reserving space only in the 96-byte Access
// Bank GPR window (0x00-0x5F). The one real difference: MOVFF is a
// two-word, full-12-bit-addressed instruction whose operands are never
// truncated to the Access Bank's 8-bit short form (fileAddr12()).
//
// Throws std::runtime_error on any assembly error.
std::string assemblePIC18(const std::string &asmText);

// Separate-compilation entry point (`--format=coff`/`elf`/`macho`/`aout`/
// `objraw`): assembles `asmText` (generated with `org 0`, no hardware
// reset-vector sequence) like assemblePIC18() does, except an undefined
// symbol becomes a relocation instead of a hard error, and every absolute
// address reference also gets a relocation against the matching reserved
// pseudo-symbol (".." for code-space/GOTO/CALL, "..d" for data-space/`f`
// operand). Returns the format-neutral ObjectFile a codegen backend
// builds once per compile, with every CODE label's value a byte offset
// into `.textBytes` (word offset times two) and every DATA label's value
// a byte offset into this object's `dataSize` reservation.
ObjectFile assemblePIC18Object(const std::string &asmText);

}  // namespace pcc
