#pragma once

#include <cstdint>
#include <string>

#include "fileformat/ObjectFile.h"

namespace pcc {

// Custom machine ID; no standard COFF/ELF machine type exists for MCS-51.
constexpr uint16_t kMCS51CoffMachine = 0x8051;

// Real 8051 reset vector (CODE address 0); used as the org for both
// --format=raw and object-mode output.
constexpr uint32_t kMCS51BaseAddress = 0;

// Big-endian 16-bit additive relocation (RelocKinds::patchAbs16BE).
// Used for LJMP/LCALL CODE-space targets (against a symbol or the
// pseudo-symbol ".." for same-file targets) and MOV DPTR,#imm16
// XDATA-space addresses (against pseudo-symbol "..d"). Short
// conditional branches are always intra-function, so never relocated.
constexpr uint16_t kMCS51RelocAbs16 = 1;

// Two-pass in-process assembler for this project's own MCS-51 dialect
// (not a general-purpose 8051 assembler), parsed via the shared
// ../../../grammar/PccAsm.g4 grammar. No register operand syntax: each
// mnemonic implies its own registers ("mova"/"movai"/"movd"/"movdi",
// since real 8051 `MOV` alone is ambiguous between several encodings),
// and XDATA indirection uses dedicated `movxr`/`movxw` mnemonics
// instead of bracketed memory operands.
//
// Keeps two separate symbol tables: `labels_` for CODE-space
// (function/branch labels, plus pseudo-register equates), and
// `dataLabels_` for XDATA-space, populated by `xdata <count>`
// directives and resolved only by `MOV DPTR,#imm16` operands.
//
// `--format=raw` (assembleMCS51() below) requires every CODE- and
// XDATA-space symbol to resolve within this one file (hard error
// otherwise) and produces a fully-resolved image with no relocations.
//
// An instruction's size is fixed by its mnemonic alone, never by an
// operand's resolved value, so two-pass sizing never needs a second
// pass to re-size anything.
//
// Throws std::runtime_error on any assembly error.
std::string assembleMCS51(const std::string &asmText);

// Separate-compilation entry point (--format=coff/elf/macho/aout/
// objraw). Like assembleMCS51(), except every LJMP/LCALL CODE-space
// target - even one defined in this same file - gets a relocation
// against pseudo-symbol ".." (since this file's own labels only
// resolve relative to its own org 0 until linked), and every MOV
// DPTR,#imm16 XDATA-space operand likewise gets one against "..d", so
// the linker can assign distinct, non-colliding addresses across
// objects. A global declared `extern` but not defined here has no
// xdata label emitted, so referencing it fails loudly at resolve time
// rather than miscompiling silently.
ObjectFile assembleMCS51Object(const std::string &asmText);

}  // namespace pcc
