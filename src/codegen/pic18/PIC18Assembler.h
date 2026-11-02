#pragma once

#include <cstdint>
#include <string>

#include "objectformat/ObjectFile.h"

namespace pcc {

// This target's own custom, non-registered COFF/object-format
// machine ID (no real one exists for the PIC18 - see
// ../../coff/CoffWriter.h's own header comment) - exported so
// ../../linker/factory/PIC18LinkerFactory.cpp can check an input
// object's embedded machine type against `--target=pic18` without
// duplicating this literal. Distinct from every other registered
// machine ID in this project (checked directly against the full
// `grep -rn "CoffMachine = 0x"` list across `src/` - not shared with
// pic16, which gets its own distinct ID for the same reason m68k and
// mc68328 do despite sharing an instruction set - see
// M68kAssembler.h's own header comment).
constexpr uint16_t kPIC18CoffMachine = 0xFC18;

// Where the whole program's own CODE space loads and starts running -
// the real hardware reset vector address (0x000000), the same
// convention PIC18Codegen.cpp's own whole-program `org 0 / goto
// START` already uses. Exported so PIC18LinkerFactory.cpp's own
// sequential object layout (see GenericLinker.h's own
// `baseAddress`) starts from the same place.
constexpr uint32_t kPIC18BaseAddress = 0;

// Where this target's own second, Harvard DATA address space (see
// ObjectFile.h's own `ObjSymbol::isData` comment) starts in real
// hardware addressing, for PIC18LinkerFactory.cpp's own
// `GenericLinkerConfig::dataBaseAddress`. Real Access Bank GPR space
// starts at 0x00 (see PIC18Codegen.h's own header comment on the
// 0x00-0x5F window), but the first 34 bytes of it (0x00-0x21) are
// permanently reserved, identically in every object and in
// PIC18LinkerFactory.cpp's own synthesized entry object, for this
// backend's fixed memory-cell pseudo-registers (`ac`/`bc`/`cx`/`dx`/
// `ptr`/`SP`/`SSTACK` - PIC18Codegen.cpp's own object-mode `generate()`
// declares these as plain compile-time equates at these exact
// addresses, never through `res`/the data-space relocation mechanism
// at all - the same "a fixed memory cell needs no relocation" choice
// ../i8008/I8008Assembler.h's own `ac`/`bc`/.../`sp` equates and
// ../mcs51/MCS51Codegen.cpp's own identical pseudo-registers already
// make, and for the identical reason: nothing ever needs one file's
// own `ac` to alias another's, so there's no reason to spend a real,
// scarce data-space byte on relocating something that's the same
// everywhere). `0x22` is exactly where PIC18Assembler.cpp's own
// `ramOffset_` naturally lands once `ac`(2)/`bc`(2)/`cx`(2)/`dx`(2)/
// `ptr`(1)/`SP`(1)/`SSTACK`(24) are allocated in that order - see
// PIC18Codegen.cpp's own raw-mode (non-`objectMode`) `declareRam`
// call sequence, which still uses `res` (unchanged) and so still
// genuinely counts this out at runtime rather than hardcoding it.
constexpr uint32_t kPIC18DataBaseAddress = 0x22;

// GOTO/CALL's own 20-bit absolute *word* address `k` (see
// PIC18Assembler.cpp's own `controlAddr20` comment: real PIC18
// program memory has no byte granularity at all) needs three
// relocation kinds, not one, because GenericLinker.cpp's own merge()
// always resolves a symbol to a *byte* address (every GenericLinker-
// based target lays out `.text` byte-wise), which a PIC18LinkerFactory.cpp's
// own patch function must first divide by two (exact and lossless -
// see that file's own header comment on why every PIC18 object's own
// `.textBytes` size is always even) before it's even the right unit
// to add to `k`'s own baked-in local word offset, and only then does
// splitting that 20-bit sum across GOTO/CALL's own two 16-bit
// instruction words need three separate patch sites (the low 8 bits
// in the first word's own low byte, the next 8 in the second word's
// own low byte, the top 4 in the second word's own high byte's own
// low nibble) - mirroring ../i8008/I8008Assembler.h's own Lo/Hi
// carry-correct pair (see I8008LinkerFactory.cpp's own header
// comment on the technique), just with one extra level since this
// field is 20 bits, not 16. Unlike PIC16's own direct-11-bit GOTO/
// CALL (see PIC16Codegen.h's own header comment on why *that*
// backend needs a real paging workaround), this chip's full 20-bit
// field comfortably spans its own entire 16K-word program memory, so
// no page-select bookkeeping is needed here at all - genuinely
// simpler than its mid-range sibling, exactly as PIC18Codegen.h's own
// header comment already notes for the unrelocated case.
constexpr uint16_t kPIC18RelocGotoCallLo = 1;
constexpr uint16_t kPIC18RelocGotoCallMid = 2;
constexpr uint16_t kPIC18RelocGotoCallHiNibble = 3;

// Every data-space reference this dialect ever emits - MOVWF/MOVF/
// CLRF/ADDWF/.../BSF/BCF/BTFSS/BTFSC's own plain 8-bit Access-Bank
// `f` operand (see PIC18Assembler.cpp's own `fileAddr`), and MOVFF's
// own 12-bit `fs`/`fd` operand (see that file's own `fileAddr12`) -
// needs one and the same simple single-byte *additive* patch: unlike
// GOTO/CALL's own code-space field, a data-space address here is
// already byte-addressed (no word-vs-byte shift needed at all), and
// every real address this backend ever resolves stays well under
// 0x100 even for MOVFF's nominally-12-bit field (this backend's own
// scope restriction - see PIC18Codegen.h's own header comment - never
// lets linked data space reach past the 96-byte Access Bank's own
// 0x00-0x5F window at all, so the field's own high nibble, fixed at
// assembly time to 0xC/0xF, never needs touching) - see
// PIC18LinkerFactory.cpp's own `patchDataAbs8` for the link-time bound
// check that turns "silently wrapped/corrupted" into a loud failure
// if a linked program's own combined globals/locals/params ever did
// try to exceed that window.
constexpr uint16_t kPIC18RelocDataAbs8 = 4;

// A from-scratch, in-process two-pass assembler for the subset of
// real PIC18 assembly PIC18Codegen.cpp emits - not a general-purpose
// PIC18 assembler (no macros, no conditional assembly, no
// `--format=coff`). Uses real, standard PIC18 mnemonics and produces
// real PIC18 machine code (each 16-bit instruction word packed little-
// endian, the exact on-disk shape Microchip's own Intel-HEX output
// uses) wrapped in a real Intel HEX container.
//
// Same shared grammar and the same two private directives as
// PIC16Assembler.h's own dialect (`orgram`/`res`/`=`-equate - see
// that file's own header comment for what each one means); this
// target's own `res`/`orgram` still only ever reserve space in the
// 96-byte Access Bank GPR window (0x00-0x5F - see PIC18Codegen.h's
// own header comment), the one real difference being that this core
// also has one genuine two-word, full-12-bit-addressed instruction
// (MOVFF) whose operands are never truncated to the Access Bank's own
// 8-bit short form the way every other instruction's `f` operand is -
// see `fileAddr12()`'s own comment.
//
// Throws std::runtime_error on any assembly error.
std::string assemblePIC18(const std::string &asmText);

// The real separate-compilation entry point (`--format=coff`/`elf`/
// `macho`/`aout`/`objraw` - see PIC18Codegen.h's own `objectMode`):
// assembles `asmText` (generated with `org 0`, not a real hardware
// reset-vector sequence - see PIC18Codegen.cpp's own `generate()`)
// much like assemblePIC18() does, except an undefined symbol is
// deferred as a relocation instead of a hard error, and *every*
// absolute-address reference - even to a label defined in this same
// file - also gets an additional relocation against the matching
// reserved pseudo-symbol (".." for a CODE-space/GOTO/CALL reference,
// "..d" for a DATA-space/`f`-operand reference - see
// ../../linker/GenericLinker.h's own header comment on why both are
// needed for a genuinely Harvard target, unlike every von-Neumann
// target's own single ".."). Returns the format-neutral ObjectFile a
// codegen backend builds once per compile (see
// ../../objectformat/ObjectFile.h), with every CODE label's own
// `ObjSymbol::value` a *byte* offset into `.textBytes` (word offset
// times two - see `kPIC18RelocGotoCallLo`'s own comment on why
// GenericLinker.cpp's own byte-wise layout needs that unit) and every
// DATA label's own value a byte offset into this object's own
// `dataSize` reservation - this function never touches a real COFF/
// ELF/Mach-O/a.out byte layout itself.
ObjectFile assemblePIC18Object(const std::string &asmText);

}  // namespace pcc
