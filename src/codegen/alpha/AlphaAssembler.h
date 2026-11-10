#pragma once

#include <cstdint>
#include <string>

#include "fileformat/ObjectFile.h"

namespace rcc {

// Non-registered COFF machine ID this project uses for the Alpha dialect
// below; matches the real historical Windows NT/Alpha value (IMAGE_FILE_MACHINE_ALPHA).
constexpr uint16_t kAlphaCoffMachine = 0x0184;

// Load/entry address for a linked whole program; must match AlphaCodegen.cpp's own
// kBaseAddress so a linked program behaves like a single-file one.
constexpr uint32_t kAlphaBaseAddress = 0x00120000u;

// Local relocation kinds, not GenericLinker::RelocKinds: an `ldah`/`lda` split address load
// and a plain `dw` data word don't share one additive-patch shape.
//
// `ldah`/`lda` always appear as an adjacent pair (always 4 bytes apart - `ldah` first, `lda`
// immediately after), materializing a 32-bit address the same way a real Alpha compiler does
// when it can't assume a GP-relative literal pool: `lda`'s 16-bit displacement sign-extends
// (a real `lda rd,disp(rb)` encoding - Alpha has no zero-extending 16-bit-immediate
// instruction), so splitting an address naively at bit 16 is wrong whenever the low half's
// own bit 15 is set; `ldah`'s half must then absorb a +1 "high adjust", exactly the standard
// %hi/%lo convention real Alpha (and MIPS/PowerPC/SPARC) toolchains use for a non-GP-relative
// 32-bit constant. kAlphaRelocHigh carries that adjustment; kAlphaRelocLow is a plain additive
// 16-bit low half, no adjustment of its own.
constexpr uint16_t kAlphaRelocHigh = 1;
constexpr uint16_t kAlphaRelocLow = 2;
// A plain 32-bit data word (`dw`); ordinary additive patch.
constexpr uint16_t kAlphaRelocAbs32 = 3;

// In-process two-pass assembler for the narrow Alpha AXP dialect AlphaCodegen.cpp emits, not a
// general-purpose Alpha assembler. Parses via the shared grammar/RccAsm.g4. Every encoding here
// is a real, standard Alpha AXP instruction word (little-endian, fixed 4 bytes, no branch-delay
// slot), bit-for-bit - this dialect only narrows which mnemonics/operand shapes exist:
//
//   Memory format   (opcode<<26 | Ra<<21 | Rb<<16 | disp16):
//     lda/ldah rd,rb,disp        - address arithmetic, no memory access (`lda` sign-extends
//                                  disp into a 64-bit add; `ldah` scales it by 0x10000 first)
//     ldindbu/ldindhu/ldindl/ldindq base,reg[,disp]  - load, zero-extending (byte/word -
//                                  Alpha's BWX extension has no signed byte/word load),
//                                  sign-extending (`l`, 32-bit - real LDL always does),
//                                  or full 64 bits (`q`); disp defaults to 0
//     stindb/stindh/stindl/stindq base,reg[,disp]    - store (truncating)
//   Jump format     (opcode 0x1A, Ra<<21 | Rb<<16 | action<<14 | hint):
//     jmp/jsr/ret rd,(rb)        - rd is the link register (written with the return address
//                                  by `jsr`; `zero` for `jmp`/`ret`, which don't need one),
//                                  rb (in real VAX-style register-deferred syntax, since this
//                                  grammar has no `0(rb)`) the branch target
//   Branch format   (opcode<<26 | Ra<<21 | disp21, word-granular, PC-relative to the next
//                     instruction):
//     br label                  - unconditional (Ra forced to `zero`; written out as a plain
//                                  `br`, since this dialect's calls go through `jsr` instead -
//                                  see AlphaCodegen.h - so no mnemonic ever needs Ra=`ra`)
//     beq/bne/blt/ble/bgt/bge reg,label  - tests `reg` against zero
//   Operate format  (opcode<<26 | Ra<<21 | (Rb<<16 | SBZ<<13) or (lit8<<13 | 1<<12) | function<<5 | Rc):
//     addq/subq/mulq/umulh/cmpeq/cmplt/cmple/cmpult/cmpule/and/bis/xor/sll/srl/sra
//         rd,ra,rb-or-lit8       - `rb-or-lit8`'s literal form is used whenever that operand
//                                  is a plain number 0..255 rather than a register name
//
// `db`/`dh`/`dw` emit 1/2/4-byte little-endian data. Throws std::runtime_error on any assembly
// error.
std::string assembleAlpha(const std::string &asmText);

// The separate-compilation entry point (`--format=coff`/`elf`/`macho`/`aout`): assembles
// `asmText` (generated at `org 0`) like assembleAlpha(), except an undefined symbol defers as
// a relocation instead of erroring, and every `ldah`/`lda`/`dw` address field - even to a label
// in this same file - also relocates against the reserved pseudo-symbol ".." (every label
// resolves only to "offset from org 0" until that base is added).
ObjectFile assembleAlphaObject(const std::string &asmText);

}  // namespace rcc
