#pragma once

#include <cstdint>
#include <string>

#include "fileformat/ObjectFile.h"

namespace pcc {

// A from-scratch, in-process two-pass assembler for exactly the
// narrow 16-bit real-mode x86 dialect I8086Codegen.cpp emits - not a
// general-purpose x86 assembler (no 32-bit operands, no segment
// overrides beyond the fixed boot-sector prologue, no addressing
// modes beyond the three this backend ever actually uses: `[label]`,
// `[bx]`, `[label+bx]`). Parses via the same shared grammar
// ../../../../grammar/PccAsm.g4 the pdp7 backend's own
// ../pdp7/Pdp7Assembler.h uses (see that file's header comment for
// the grammar's own rationale) - this backend's semantic layer just
// interprets the parsed mnemonic/operand-list differently: here, the mnemonic
// selects a real x86 instruction and each comma-separated operand is
// a distinct, structurally real argument to it (register, memory, or
// immediate), unlike pdp7's OR-combined-expression semantics.
//
// Every instruction's *size* is fixed by its syntactic form alone,
// never by any operand's resolved value (e.g. `jmp` always encodes
// as near/rel16, `jCC` always as short/rel8) - this keeps the
// two-pass model exactly as simple as pdp7's own: pass 1 walks every
// line once, accumulating a byte offset and recording label
// addresses, with no need to know any operand's real value yet;
// pass 2 re-walks with every label now resolvable and emits the real
// bytes. A conditional jump whose real target turns out to be out of
// short-jump range is a pass-2 error, not silently miscompiled -
// nothing pcc itself has ever generated comes close to that range in
// practice (everything fits in one 512-byte boot sector).
//
// Throws std::runtime_error on any assembly error, same convention as
// ../pdp7/Pdp7Assembler.h and the rest of pcc's own frontend.
enum class I8086OutputFormat { Bin, Coff, Elf };

// `allowExternal` is real separate compilation's own switch
// (`--format=coff` from a single `.c` file - see IRBuilder.h's
// `allowExternal` and ../pdp7/Pdp7Assembler.h's own version of this
// same comment): when true, a label this text doesn't define becomes
// a relocation in the COFF output instead of a hard error, and
// `ORG`'s own base is whatever the text itself says (an object file's
// `ORG 0` from I8086Codegen.cpp's per-file codegen - see that file -
// rather than the whole-program convention's `0x7C00`, since it's
// the linker's job to place this, not this assembler's).
std::string assembleI8086(const std::string &asmText, I8086OutputFormat format, bool allowExternal = false);

// This target's own custom, non-registered COFF machine ID (real
// 16-bit 8086/DOS tooling used OMF, not COFF - see CoffWriter.h's own
// header comment) - exported so ../../linker/Linker.cpp can check an
// input object's embedded machine type against `--target` without
// duplicating this literal.
constexpr uint16_t kI8086CoffMachine = 0x0086;

// The boot-sector load address every whole-program 8086 compile
// starts from - see I8086Codegen.h's own `objectMode` comment above.
// Exported so I8086Linker.cpp's own sequential object layout starts
// from the same place.
constexpr uint32_t kI8086BaseAddress = 0x7C00;

// Relocation `type` values an 8086 object's own `ObjRelocation`
// entries use (see I8086Assembler.cpp's own `RelocKind` comment) -
// exported so ../../linker/factory/I8086LinkerFactory.cpp can tell
// the two kinds apart when patching a resolved symbol's value into a
// word (both are plain, additive/relative 16-bit patches - see
// ../../linker/GenericLinker.h's own `RelocKinds::patchAbs16`/
// `patchRel16`, which this target reuses as-is rather than needing
// any local patch function of its own).
constexpr uint16_t kI8086RelocAbs16 = 1;
constexpr uint16_t kI8086RelocRel16 = 2;

// Assembles `asmText` (generated with `ORG 0`, not `kI8086BaseAddress`
// - see I8086Codegen.cpp's own `generate()`) exactly like
// `assembleI8086(asmText, Coff, true)` does, except it returns the
// format-neutral ObjectFile a codegen backend builds once per compile
// (see ../../fileformat/ObjectFile.h) directly, instead of already-
// serialized COFF bytes. `--format=coff` compiles (see
// I8086Codegen.h's own `objectMode`) still go through `assembleI8086`
// as before; this function exists solely for
// ../../linker/factory/I8086LinkerFactory.cpp's own `makeI8086Linker()`
// to assemble its synthesized entry object without a serialize-then-
// immediately-deserialize round trip through `writeCoffObject`/
// `readCoffObject`.
ObjectFile assembleI8086Object(const std::string &asmText);

}  // namespace pcc
