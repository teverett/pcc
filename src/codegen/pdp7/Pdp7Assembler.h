#pragma once

#include <cstdint>
#include <string>

#include "fileformat/ObjectFile.h"

namespace pcc {

// A from-scratch, in-process two-pass assembler for exactly the
// narrow pdp7 assembly dialect Pdp7Codegen.cpp emits - not the full
// historical as.s/as7 language ../../../../pdp-assembler's own
// Assembler handles. That narrower scope is what makes "from
// scratch, no external tool" tractable here: Pdp7Codegen never emits
// packed-char literals (`<x`/`x>`), numeric relative labels
// (`1f`/`9b`), or an `@local` directive, and every label it produces
// is already globally unique by construction (`S<id>`, `F<id>`,
// `T<N>`, `N<N>`, `J<N>`, plus the fixed internal names like
// `__saveloc`/`__one`/`__divmod`) - so this doesn't need local-label
// scoping, multi-file concatenation, or any of pdp-assembler's
// historical-compatibility machinery, just the same core two-pass
// model and opcode table (copied verbatim from
// ../../../../pdp-assembler/src/Assembler.cpp, already verified
// correct by that project) and the same `Ptr`/`Rim`/`A7Out`/`List`
// output-format byte-packing.
//
// Throws std::runtime_error on any assembly error (undefined symbol,
// a location below the relocation base `..`, or malformed input) -
// same convention the rest of pcc's own frontend (BuildAst.cpp,
// IRBuilder.cpp) already uses. Since the only input this ever sees is
// Pdp7Codegen's own generated text, an error here means a genuine bug
// in that codegen, not malformed third-party source - so it's meant
// to fail loudly, not tolerate and continue the way pdp-assembler's
// own Assembler does for real historical kernel sources.
enum class Pdp7OutputFormat { A7Out, List, Ptr, Rim, Coff };

// This target's own custom, non-registered COFF machine ID (PDP-7
// predates COFF, so there's no real one to use - see CoffWriter.h's
// own header comment) - exported so
// ../../linker/factory/Pdp7LinkerFactory.cpp can check an input
// object's embedded machine type against `--target` without
// duplicating this literal.
constexpr uint16_t kPdp7CoffMachine = 0x1987;

// The relocation base (word address) every whole-program pdp7 compile
// starts from - see this file's own `allowExternal` comment above.
// Exported so ../../linker/factory/Pdp7LinkerFactory.cpp's own
// sequential object layout starts from the same place (converted to
// GenericLinkerConfig::baseAddress's own packed-byte scale there -
// see that file's own comment on why).
constexpr uint32_t kPdp7BaseAddress = 4096;

// Relocation `type` values a pdp7 object's own `ObjRelocation`
// entries use (see CoffWriter.h's own `ObjRelocation` comment) - the
// syllable combine-op a pending reference used (`+`, `-`, or the
// default OR). Exported so ../../linker/factory/Pdp7LinkerFactory.cpp
// can apply the same combine semantics when patching a resolved
// symbol's value into a word - none of these are additive/relative
// byte patches, so none reuse GenericLinker.h's own shared
// `RelocKinds`; all three are local patch functions of that factory's
// own.
constexpr uint16_t kPdp7RelocOr = 1;
constexpr uint16_t kPdp7RelocAdd = 2;
constexpr uint16_t kPdp7RelocSub = 3;

// `allowExternal` is real separate compilation's own switch
// (`--format=coff`, from a single `.c` file compiled on its own -
// see IRBuilder.h/main.cpp): when true, a symbol this text's own
// labels don't define is no longer a hard error - it's deferred as a
// relocation in the COFF output (see CoffWriter.h) for a future
// `--link` step to resolve, and this assembler's own relocation base
// (`..`) starts at 0 instead of the usual 4096 (octal `10000`),
// since an object file's own text is meant to be placed wherever the
// linker lays it out, not fixed at the whole-program convention's
// own starting point. Only meaningful for `Coff`; every other format
// is still a complete, final, single-file program, so an unresolved
// symbol there is exactly as much an error as it's always been.
std::string assemblePdp7(const std::string &asmText, Pdp7OutputFormat format, bool allowExternal = false);

// Assembles `asmText` (generated with the relocation base `..` at 0,
// not `kPdp7BaseAddress` - see this file's own `allowExternal`
// comment above) exactly like `assemblePdp7(asmText, Coff, true)`
// does, except it returns the format-neutral ObjectFile a codegen
// backend builds once per compile (see ../../fileformat/ObjectFile.h)
// directly, instead of already-serialized COFF bytes.
// ../../linker/factory/Pdp7LinkerFactory.cpp's own `makePdp7Linker()`
// uses this both for every real user object (via `--format=coff`,
// same as any other object-mode compile) and to assemble its own
// synthesized entry object, without a serialize-then-immediately-
// deserialize round trip through `writeCoffObject`/`readCoffObject`.
ObjectFile assemblePdp7Object(const std::string &asmText);

}  // namespace pcc
