#pragma once

#include <cstdint>
#include <string>

namespace pcc {

// In-process two-pass assembler for the narrow TMS320C6713 (TI C67x) dialect
// Tms320C6713Codegen.cpp emits, not a general-purpose C6000 assembler. Parses via the shared
// grammar/PccAsm.g4 (see AlphaAssembler.h's own comment on that grammar). Every encoding below
// is a real, verified TMS320C6713/C62x+ bit-level opcode (extracted from binutils' own
// include/opcode/tic6x*.h - see Tms320C6713Codegen.h's header comment on why), narrowed to a
// fixed, always-valid subset: every word's `p` bit is 0 (never packs two instructions into one
// execute packet), `s` bit is 1 (side B only), `x` bit is 0 (no cross-path read) - see
// Tms320C6713Codegen.h's own header comment for why that's a real, always-correct restriction
// rather than an approximation. Every 32-bit word is little-endian, fixed 4 bytes, no delay
// slot the hardware fills for you - Tms320C6713Codegen.cpp is responsible for emitting every
// required NOP itself; this assembler only encodes exactly the instructions it's given.
//
//   S-unit MVK/MVKH   (base 0x28): `MVK rd,imm16`  dst = sign_extend_16(imm16)
//                                  `MVKH rd,imm16` dst = (dst&0xFFFF) | (imm16<<16)
//                                  `imm16` may be a label; the assembler takes its address's
//                                  low (MVK) or high (MVKH) 16 bits.
//   L-unit ALU        (base 0x18): `ADD/SUB/AND/OR/XOR/CMPEQ/CMPGT/CMPGTU/CMPLT/CMPLTU
//                                  rd,ra,rb` - three register operands, dst first.
//   S-unit ALU        (base 0x20): `SHL/SHR/SHRU rd,rvalue,rshift` - dst = rvalue shifted by
//                                  rshift (SHL left, SHR arithmetic right, SHRU logical right).
//   M-unit multiply    (base 0x0): `MPY/MPYH/MPYHL/MPYLH rd,ra,rb` - 16x16->32 multiply (see
//                                  Tms320C6713Codegen.h's own header comment for each one's
//                                  exact halfword selection).
//   D-unit load/store   (base 0x4): `LDB/LDH/LDW rbase,rd` (load, dest zero/sign-extended only
//                                  by the codegen that issued it, never by hardware here) /
//                                  `STB/STH/STW rbase,rsrc` (store) - always `mode=1,offsetR=0`
//                                  (i.e. the effective address is exactly `rbase`'s own value;
//                                  see Tms320C6713Codegen.h's own header comment on why this
//                                  backend always computes a full address into a register
//                                  first rather than ever using a nonzero offsetR).
//   S-unit branch, register form (base 0x360): `BR (rb)` - branch to the address in `rb`.
//   S-unit branch, constant form   (base 0x10): `B label` (creg=0, unconditional) /
//                                  `BZ label` (creg=1,z=1 - branches if B0==0) /
//                                  `BNZ label` (creg=1,z=0 - branches if B0!=0).
//   `NOP` - the all-zero 32-bit word.
//
// `db`/`dh`/`dw` emit 1/2/4-byte little-endian data, same as every other target's dialect here.
// Throws std::runtime_error on any assembly error.
std::string assembleTms320C6713(const std::string &asmText);

}  // namespace pcc
