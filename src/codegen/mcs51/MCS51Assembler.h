#pragma once

#include <cstdint>
#include <string>

#include "objectformat/ObjectFile.h"

namespace pcc {

// This target's own custom, non-registered COFF/object-format
// machine ID (no real one exists for the MCS-51 family - see
// ../../coff/CoffWriter.h's own header comment) - exported so
// ../../linker/factory/MCS51LinkerFactory.cpp can check an input
// object's embedded machine type against `--target=8051`/`8052`/
// `8031`/`8032` without duplicating this literal. Checked against
// every other backend's own kXCoffMachine (0x0086, 0x1121, 0x16C3,
// 0x1987, 0x3332, 0x3386, 0x4000, 0x5043, 0x5708, 0x5A38, 0x6328,
// 0x6502, 0x6800, 0x8008, 0x8080, 0xA007, 0xA032, 0xAA64, 0xC502) to
// avoid any collision.
constexpr uint16_t kMCS51CoffMachine = 0x8051;

// Where the whole program loads and starts running - the real 8051
// reset vector (see MCS51Codegen.h's own header comment: this is a
// real microcontroller, not a simulator-sets-PC-directly convention
// the way m68k/i8008's own kBaseAddress is), already what this
// backend's own `--format=raw` entry sequence uses via MCS51Codegen.cpp's
// own `generate()` (`org 0`). Real separate-compilation object-mode
// compiles use this exact same `org 0` for the identical reason every
// other GenericLinker-based target's own object does (see
// ../i8008/I8008Assembler.h's own comment) - here it also happens to
// already be the only address a whole linked program could ever load
// at, so object mode and `--format=raw` share one constant instead of
// two coincidentally-equal literals.
constexpr uint32_t kMCS51BaseAddress = 0;

// This dialect's one relocation kind: a big-endian 16-bit *additive*
// patch (see ../../linker/GenericLinker.h's own
// `RelocKinds::patchAbs16BE`) - one field shape, shared by two
// genuinely different address spaces:
//   - `LJMP`/`LCALL`'s own absolute CODE-space target (see
//     MCS51Assembler.cpp's own `emitAbsOperand`), relocated against
//     either another file's own function name or (for a same-file
//     target - see GenericLinker.h's own header comment on why even
//     that needs it) the reserved pseudo-symbol `".."`.
//   - `MOV DPTR,#imm16`'s own XDATA address operand (see
//     MCS51Assembler.cpp's own `emitDataPtrOperand`), structurally the
//     same big-endian 16-bit field, but resolved against this
//     object's own XDATA label table (see the `xdata` directive
//     below) instead of its CODE-space one, and relocated against the
//     reserved pseudo-symbol `"..d"` - the data-space counterpart of
//     `".."`, always needed (never just an external reference)
//     exactly like `".."` is for a same-file CODE-space target, since
//     this object's own data-space base is just as unknown until
//     linked (see GenericLinker.h's own header comment).
// Both shapes are a single contiguous 16-bit field, high byte first
// (real 8051 encoding order), with no split hi/lo halves the way
// I8008's own byte-at-a-time `mvihi`/`mvi` pair needs. Every short
// conditional branch (`JZ`/`JNZ`/`JC`/`JNC`/the fixed-bit branches/
// `DJNZ`/`CJNE`'s own rel8 field) is always intra-function-only by
// this backend's own `safeBranch()`/`safeDjnz()` convention (see
// MCS51Codegen.h's own header comment), so a relative displacement is
// identical whether computed before or after this object's own base
// is added at link time - no relocation needed there either.
constexpr uint16_t kMCS51RelocAbs16 = 1;

// A from-scratch, in-process two-pass assembler for exactly the
// narrow MCS-51 dialect MCS51Codegen.cpp emits - not a general-
// purpose 8051 assembler. Parses via the same shared grammar
// ../../../grammar/PccAsm.g4 every other in-process-assembler
// backend uses (see M6502Assembler.h/I8086Assembler.h's own header
// comments) - every operand is a plain expression (a number, a
// symbol, or a signed combination of them); there's no register
// operand syntax at all (every mnemonic already implies which real
// register(s) it touches - "mova"/"movai"/"movd"/"movdi" spell out
// direction and operand kind the way M6502Assembler.cpp's own
// "ldai"/"lda" split already does, since the real 8051 `MOV`
// mnemonic alone is ambiguous between at least six different
// encodings - see MCS51Codegen.cpp's own instruction choices), and
// no bracketed memory-operand syntax either (the one real indirect
// addressing mode this dialect ever needs - XDATA via DPTR - has its
// own dedicated implied-operand mnemonics, `movxr`/`movxw`, instead).
//
// This assembler keeps two entirely separate symbol/label tables, one
// per real address space this target has (internal RAM's own fixed
// pseudo-register equates are a third, but - being identical plain
// numeric equates in every object - never need a table entry beyond
// `labels_` below at all, see MCS51Codegen.h's own header comment):
//   - `labels_` - CODE-space: every function/branch-target label, and
//     (harmlessly mixed in, see `resolveSyllable()`'s own comment)
//     every pseudo-register equate. Resolved by `LJMP`/`LCALL`'s own
//     absolute operand and every direct/immediate/rel8 operand.
//   - `dataLabels_` - XDATA-space: every global/local/param's own
//     storage, declared by an `xdata <count>` directive (see below)
//     rather than by an ordinary instruction's own label.
//     Resolved only by `MOV DPTR,#imm16`'s own operand (the only
//     instruction MCS51Codegen.cpp ever gives a data-space symbol to
//     - every subsequent MOVX/direct-register step just operates on
//     whatever DPTR/the pseudo-register now holds, with no further
//     symbol involved).
//
// The `xdata <count>` directive reserves `count` consecutive bytes in
// this object's own data-space layout for whichever label(s) (the
// ordinary `label:` syntax every other line already supports) are
// attached to it, exactly mirroring how an ordinary instruction's own
// label gets bound to "the next CODE byte", but via a dedicated,
// separate cursor (`dataCursor_`) that starts at 0 and is untouched by
// `org`/CODE-space byte emission - MCS51Codegen.cpp's own allocXdata()
// emits one `<lbl>: xdata <count>` line per global/local/param, and
// never computes an XDATA address itself.
//
// `--format=raw` (assembleMCS51() below) has no relocations and no
// external symbols at all: every CODE-space symbol must resolve
// within the one file being assembled, a hard error otherwise, and
// every XDATA-space symbol (`dataLabels_`) always must too, in either
// mode - see `resolveDataSyllable()`'s own comment. A `MOV
// DPTR,#imm16` operand resolves to the plain final address directly,
// with no relocation, since the whole program is already a single,
// fully-resolved image.
//
// Every instruction's *size* is fixed by its mnemonic alone, never by
// any operand's resolved value - same convention, and the same
// reason (a stable two-pass size computation with no phase-sizing
// ambiguity), as every other backend's own in-process assembler here.
//
// Throws std::runtime_error on any assembly error.
std::string assembleMCS51(const std::string &asmText);

// The real separate-compilation entry point (`--format=coff`/`elf`/
// `macho`/`aout`/`objraw` - see MCS51Codegen.h's own `objectMode`):
// assembles `asmText` (generated with `org 0`, same as
// `kMCS51BaseAddress` above - see MCS51Codegen.cpp's own
// `generate()`) exactly like assembleMCS51() does, except:
//   - An undefined symbol in `LJMP`/`LCALL`'s own CODE-space target
//     operand is deferred as a relocation instead of a hard error,
//     and *every* such target - even a label defined in this same
//     file - also gets an additional relocation against the reserved
//     pseudo-symbol ".." (see ../../linker/GenericLinker.h's own
//     header comment on why: this text's own labels all resolve to
//     "offset from this object's own org 0" until that base is
//     added).
//   - Every `MOV DPTR,#imm16` XDATA-space operand - even one
//     resolved within this same file - also gets a relocation against
//     the reserved pseudo-symbol `"..d"`, the exact data-space
//     counterpart of `".."` above (see GenericLinker.h's own header
//     comment), so GenericLinker.cpp's own data-space layout pass can
//     give two separately-compiled objects' own distinct globals/
//     locals/params distinct, non-colliding linked XDATA addresses.
// A global *variable* defined in one file and merely declared
// `extern` in another still has no equivalent here at all: an
// `isExternal` global's own `xdata` directive is never emitted (see
// MCS51Codegen.cpp's own `generate()` pre-pass), so referencing one
// from a file that doesn't define it still fails loudly, via
// `resolveDataSyllable()`'s own "no XDATA address" error - a loud
// compile-time failure, never a silent miscompile. Returns the
// format-neutral ObjectFile a codegen backend builds once per compile
// (see ../../objectformat/ObjectFile.h) - this function never touches a
// real COFF/ELF/Mach-O/a.out byte layout itself.
ObjectFile assembleMCS51Object(const std::string &asmText);

}  // namespace pcc
