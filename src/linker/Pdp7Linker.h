#pragma once

#include "Linker.h"

namespace pcc {

// Merges several pdp7 COFF objects (Pdp7Assembler.cpp's own
// `--format=coff` output) into one resolved LinkedImage (see
// LinkedImage.h - `unitBits=18`, one entry per 18-bit word).
// Objects are laid out sequentially by word (same unit
// Pdp7Assembler.cpp's own `..` uses - a word is 3 packed bytes in the
// COFF `.text` section, see that file's `writeCoff`), starting right
// after a synthesized 2-word entry sequence (`jms <main's linked
// address>` / `hlt`) this linker itself writes at the very front,
// address kPdp7BaseAddress (see Pdp7Assembler.h) - exactly the inline
// entry sequence Pdp7Codegen.cpp's own whole-program mode used to
// emit, just written here instead, once every object's `main` is
// resolvable (see Codegen.h's `objectMode`/Pdp7Codegen.cpp's own
// comment on why no object ever emits it itself any more).
//
// Every relocation's symbol is resolved one of two ways: the reserved
// pseudo-symbol ".." always means "this specific object's own
// assigned base" (never looked up globally - see Pdp7Assembler.cpp's
// own comment on why even a same-file address reference needs this
// in object mode); every other name is looked up in the merged global
// symbol table - built from every object's own defined symbol whose
// name starts with "G_"/"P_"/"FN_" (the only ones Pdp7Codegen.h's
// label() ever gives real cross-file meaning - see merge()'s own
// comment).
class Pdp7Linker : public TargetLinker {
   public:
    LinkedImage merge(const std::vector<CoffObject> &objects) const override;
    std::vector<std::string> outputFormats() const override { return {"a7out", "list", "ptr", "rim", "coff", "aout"}; }
};

}  // namespace pcc
