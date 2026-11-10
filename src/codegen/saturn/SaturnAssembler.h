#pragma once

#include <cstdint>
#include <string>

#include "fileformat/ObjectFile.h"

namespace rcc {

// A from-scratch, in-process two-pass assembler for exactly the narrow Saturn assembly dialect
// SaturnCodegen.cpp emits.
//
// This backend models the HP Saturn (the 4-bit/nibble-oriented CPU in the HP48/HP49 calculator
// series) at a simplified word level rather than its real nibble-addressed one: memory is treated
// as an array of 32-bit words (8 nibbles each - not a named field on the real chip, just this
// backend's own "native int" unit) instead of individually addressable nibbles, the same kind of
// simplification pdp7's backend makes by being word- rather than bit-addressed. Five Saturn-style
// registers are modeled - `a`/`c`/`b` (general-purpose, real Saturn register letters) and `d0`/`d1`
// (real Saturn address registers, used here as this backend's stack pointer/frame pointer) - each
// holding one such 32-bit word. Mnemonics (`goto`/`gosbvl`/`rtn` among them) are chosen to evoke
// real Saturn assembly, but the encoding below is this backend's own invention, not real Saturn
// nibble-level machine code; see docs/platforms.md for the full rationale.
//
// Throws std::runtime_error on any assembly error (undefined symbol, malformed input), since the
// only input this ever sees is SaturnCodegen's own generated text, so an error here means a bug in
// that codegen.

// This target's own custom, non-registered COFF machine ID (the real Saturn predates COFF, and
// was never a COFF target in reality) - exported so SaturnLinkerFactory.cpp can check an input
// object's embedded machine type against `--target`.
constexpr uint16_t kSaturnCoffMachine = 0x5A74;  // "Zt" - arbitrary, unregistered

// The byte address every whole-program Saturn compile's code+data starts from; SaturnLinkerFactory
// .cpp's sequential object layout starts from the same place. One word is 4 bytes, so this is also
// word-aligned.
constexpr uint32_t kSaturnBaseAddress = 0x1000;

// Initial value for `d0` (this backend's stack pointer, see SaturnCodegen.h) before `main` is
// ever called - there's no hardware reset value to rely on, so both the whole-program compile
// path (SaturnCodegen.cpp's generate()) and the linked-program entry object
// (SaturnLinkerFactory.cpp) set it explicitly to this same address, the stack growing downward
// from here. Chosen well above kSaturnBaseAddress with generous headroom for code+data below and
// call depth above, the same kind of fixed, unchecked arrangement pdp7's own `__sp` uses.
constexpr uint32_t kSaturnStackTop = 0xF000;

// Relocation `type` values a Saturn object's `ObjRelocation` entries use. kSaturnRelocAbs32 is a
// plain additive 32-bit absolute patch (GenericLinker::RelocKinds::patchAbs32); every address this
// backend ever embeds (a `ldi`/`dw`/branch-target operand) uses it - there's no split hi/lo load
// and no PC-relative addressing mode to need a different kind.
constexpr uint16_t kSaturnRelocAbs32 = 1;

// `allowExternal` is real separate compilation's switch (`--format=coff`): when true, an undefined
// symbol is deferred as a relocation in the COFF output instead of erroring, and `org 0` (the
// implicit base every object assembles from) stays 0 rather than being shifted to
// kSaturnBaseAddress, since the linker places the object's text wherever it lays out the program.
std::string assembleSaturn(const std::string &asmText, bool allowExternal = false);

// Assembles `asmText` (generated with relocation base `..`/`org 0`, not kSaturnBaseAddress) like
// assembleSaturn(asmText, true), but returns the format-neutral ObjectFile directly instead of
// serialized COFF bytes. Used by SaturnLinkerFactory.cpp's makeSaturnLinker() for both real user
// objects and its synthesized entry object, avoiding a serialize/deserialize round trip.
ObjectFile assembleSaturnObject(const std::string &asmText);

}  // namespace rcc
