#pragma once

#include <cstdint>
#include <string>

namespace rcc {

// A from-scratch, in-process two-pass assembler for exactly the narrow
// pdp1 assembly dialect Pdp1Codegen.cpp emits - see Pdp7Assembler.h's own
// comment for the shared design this follows. This target has no separate-
// compilation/linking support (see Pdp1Codegen.h's class comment), so
// there's no COFF/relocation machinery here at all, unlike Pdp7Assembler.
//
// Throws std::runtime_error on any assembly error, since the only input
// this ever sees is Pdp1Codegen's own generated text.
enum class Pdp1OutputFormat { List, Rim };

// The relocation base (word address) a pdp1 whole-program compile starts
// from. Kept at 0 (unlike pdp7's reserved low region): this target's
// 4096-word address space is a quarter of pdp7's 8192, and nothing about
// this backend's own runtime or calling convention needs a reserved low
// region of its own.
constexpr uint32_t kPdp1BaseAddress = 0;

// PDP-1 instruction words place a 12-bit address field in the low 12 bits
// (mask 07777) - an address at or above 4096 collides with the opcode/
// indirect-bit fields above it instead of erroring.
constexpr uint32_t kPdp1AddressFieldLimit = 4096;  // 010000 octal

// The runtime call stack (`__sp`) starts at 4095, the top of the 12-bit
// address field, and grows downward with no bounds check - same shape as
// pdp7's, scaled to this target's quarter-size address space.
constexpr uint32_t kPdp1StackReserve = 256;
constexpr uint32_t kPdp1StackSafeLimit = kPdp1AddressFieldLimit - kPdp1StackReserve;

std::string assemblePdp1(const std::string &asmText, Pdp1OutputFormat format);

}  // namespace rcc
