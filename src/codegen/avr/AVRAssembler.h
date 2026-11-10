#pragma once

#include <cstdint>
#include <string>

#include "fileformat/ObjectFile.h"

namespace pcc {

// Ad-hoc COFF machine ID for this dialect, ASCII "AV" (no real AVR COFF ID exists).
constexpr uint16_t kAVRCoffMachine = 0x4156;

// Flash (program memory) load/entry address for a linked whole program: the real ATmega328P
// reset vector, address 0. Must match AVRCodegen.cpp's kFlashBaseAddress.
constexpr uint32_t kAVRFlashBaseAddress = 0x00000000u;

// SRAM (data memory) start address on the ATmega328P - everything below this (0x0000-0x00FF) is
// the 32 GPRs and the I/O/extended-I/O space, never used for a Global's storage. Must match
// AVRCodegen.cpp's own use and AVRLinkerFactory.cpp's GenericLinkerConfig::dataBaseAddress.
constexpr uint32_t kAVRSramBaseAddress = 0x00000100u;

// A plain 16-bit little-endian absolute SRAM address field (a Global's `lds`/`sts` operand).
constexpr uint16_t kAVRRelocAbs16 = 1;

// `call`/`jmp`'s absolute program-memory target: a 16-bit little-endian *word* address (flash
// byte address / 2 - see the instruction set manual's `CALL`/`JMP` encoding). The generic linker
// always works in byte units, so this patch kind halves `value` itself rather than relying on
// GenericLinkerConfig::addressUnitBytes (which would also rescale the unrelated byte-addressed
// `.text` layout and the `lds`/`sts` relocations above).
constexpr uint16_t kAVRRelocCallWord = 2;

// `ldiw`/`ldiwc`'s address, split across two back-to-back `ldi` words (see AVRAssembler.cpp's
// `encodeLdiw()`): `ldi`'s immediate field is itself split non-contiguously within one 16-bit
// instruction word (bits 11:8 and 3:0), so patching each word's byte independently would drop the
// carry between them. This one relocation instead sits at the first word's offset and its patch
// function (AVRLinkerFactory.cpp) decodes both words' scattered immediate bits back into a plain
// 16-bit value, adds `value` to *that*, and re-splits the sum - safe to add more than once at the
// same site, same as any other additive relocation kind here.
constexpr uint16_t kAVRRelocLdiw16 = 3;

// In-process two-pass assembler for the ATmega328P-scoped AVR dialect AVRCodegen.cpp emits.
// Every instruction encodes real AVR machine code bit-for-bit (16-bit words, little-endian;
// `call`/`jmp`/`lds`/`sts` are two words, the second a plain 16-bit field); mnemonics and operand
// syntax are spelled out so they fit the shared PccAsm grammar, which has no `#`, `@` or trailing
// `+` token:
//
//   r0..r31               a register, by number (no ap/fp/sp names - Y (r28:r29) is this
//                         dialect's frame pointer, addressed as plain r28/r29)
//   (r26)/(r28)/(r30)     plain indirect via X/Y/Z                  ("ld"/"st")
//   ai (r26)/(r28)/(r30)  indirect via X/Y/Z, post-increment        ("ld"/"st")
//   -(r26)/(r28)/(r30)    indirect via X/Y/Z, pre-decrement         ("ld"/"st")
//   D(r28)/D(r30)         indirect via Y/Z with displacement (0-63) ("ld"/"st", encodes LDD/STD)
//   imm V                 an immediate (ldi/cpi/adiw/sbiw's K, in/out's A)
//   a bare label           call/jmp/rjmp/rcall/the branch pseudo-mnemonics' target, or
//                         lds/sts's absolute SRAM operand (optionally `label+<constant>`)
//
// `ldiw lo, label` and `ldiwc lo, label` are the one addition beyond real AVR mnemonics: two
// real `ldi` words (`lo`/`lo+1` each get one byte) materializing a label's address as a runtime
// value, since plain `ldi` only ever takes a byte - `ldiw` resolves against the data/SRAM space
// (a `&global`/`&array[i]` value) and `ldiwc` against the code/flash space at its plain *byte*
// address (AVRCodegen.cpp's own CallPtr halves it into a word address with `lsr`/`ror` right
// before `icall`, rather than this dialect inventing a second, word-addressed symbol table).
//
// `ram <count>` reserves `count` SRAM bytes for every label on that line (ObjectFile::dataSize/
// isData), the same idea as MCS51Assembler.h's own `xdata`, instead of emitting any instruction
// word - a Global's storage, since SRAM (unlike flash) has no static initial contents.
//
// Only the real `breq`/`brne`/`brge`/`brlt`/`brlo`/`brsh` short conditional branches are
// implemented (7-bit PC-relative, -64..+63 words) - AVRCodegen.cpp's own `safeBranch()` composes
// the always-in-range idiom (a short branch over a `jmp`) itself, the same split MCS51Codegen.cpp
// uses, rather than this assembler inventing a pseudo-branch the way VaxAssembler.h's `j<cc>`
// does. Throws std::runtime_error on any assembly error.
std::string assembleAVR(const std::string &asmText);

// The separate-compilation entry point (`--format=coff`/`elf`/`macho`/`aout`): assembles
// `asmText` (generated at `org 0`) like assembleAVR(), except an undefined symbol defers as a
// relocation instead of erroring, and every symbolic flash address - even to a label in this same
// file - also relocates against the reserved pseudo-symbol ".." (every label resolves only to
// "offset from org 0" until that base is added). A `ram`-declared label (`sts`/`lds`/`ldiw` only)
// relocates against the Harvard-data pseudo-symbol "..d" instead, matching MCS51Assembler.h's own
// convention; a data-space label this file never `ram`-declared itself (an `extern` global
// genuinely defined in another object) instead relocates by name, the same as an external
// code-space symbol - the linker resolves it to that other object's own already-absolute data
// address.
ObjectFile assembleAVRObject(const std::string &asmText);

}  // namespace pcc
