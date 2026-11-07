#pragma once

#include <cstdint>
#include <string>

#include "fileformat/ObjectFile.h"

namespace pcc {

// A from-scratch, in-process two-pass assembler for exactly the
// narrow pdp7 assembly dialect Pdp7Codegen.cpp emits, not the full
// as.s/as7 language ../../../../pdp-assembler's Assembler handles.
// Every label Pdp7Codegen emits is already globally unique, so this
// needs no local-label scoping or multi-file concatenation - just the
// core two-pass model and opcode table, and the same
// `Ptr`/`Rim`/`A7Out`/`List` output-format byte-packing.
//
// Throws std::runtime_error on any assembly error (undefined symbol,
// a location below the relocation base `..`, or malformed input),
// since the only input this ever sees is Pdp7Codegen's own generated
// text, so an error here means a bug in that codegen.
enum class Pdp7OutputFormat { A7Out, List, Ptr, Rim, Coff };

// This target's own custom, non-registered COFF machine ID (PDP-7
// predates COFF) - exported so Pdp7LinkerFactory.cpp can check an
// input object's embedded machine type against `--target`.
constexpr uint16_t kPdp7CoffMachine = 0x1987;

// The relocation base (word address) every whole-program pdp7 compile
// starts from; Pdp7LinkerFactory.cpp's sequential object layout starts
// from the same place.
constexpr uint32_t kPdp7BaseAddress = 4096;

// PDP-7 instruction words OR a resolved address into the low 13 bits
// of the opcode word; an address at or above 8192 (bit 13) silently
// collides with the opcode bits instead of erroring. Exported because
// both this file's `Assembler::storeWord()` (whole-program path) and
// Pdp7LinkerFactory.cpp's relocation patch functions (separate-
// compilation/`--link` path) must enforce it independently.
constexpr uint32_t kAddressFieldLimit = 010000 << 1;  // 020000 octal = 8192 decimal

// The runtime call stack (`__sp`, declared 8191 in both
// Pdp7Codegen.cpp::generate() and Pdp7LinkerFactory.cpp's entry
// object) starts at the top of the address space and grows downward
// per call, with no reserved region or bounds check - a symbol too
// close to that top gets silently overwritten by ordinary stack
// usage. `kStackReserve` is a safety margin well beyond any real call
// depth this project's test programs need.
constexpr uint32_t kStackReserve = 512;
constexpr uint32_t kStackSafeLimit = kAddressFieldLimit - kStackReserve;

// Relocation `type` values a pdp7 object's `ObjRelocation` entries
// use: the syllable combine-op a pending reference used (`+`, `-`, or
// the default OR). Exported so Pdp7LinkerFactory.cpp applies the same
// combine semantics when patching a resolved symbol into a word.
constexpr uint16_t kPdp7RelocOr = 1;
constexpr uint16_t kPdp7RelocAdd = 2;
constexpr uint16_t kPdp7RelocSub = 3;

// `allowExternal` is real separate compilation's switch
// (`--format=coff`): when true, an undefined symbol is deferred as a
// relocation in the COFF output instead of erroring, and the
// relocation base (`..`) starts at 0 instead of `kPdp7BaseAddress`,
// since the linker places the object's text wherever it lays out the
// program. Only meaningful for `Coff`; every other format is a
// complete, final, single-file program.
std::string assemblePdp7(const std::string &asmText, Pdp7OutputFormat format, bool allowExternal = false);

// Assembles `asmText` (generated with relocation base `..` at 0, not
// `kPdp7BaseAddress`) like `assemblePdp7(asmText, Coff, true)`, but
// returns the format-neutral ObjectFile directly instead of
// serialized COFF bytes. Used by Pdp7LinkerFactory.cpp's
// `makePdp7Linker()` for both real user objects and its synthesized
// entry object, avoiding a serialize/deserialize round trip.
ObjectFile assemblePdp7Object(const std::string &asmText);

}  // namespace pcc
