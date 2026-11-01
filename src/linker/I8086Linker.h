#pragma once

#include "Linker.h"

namespace pcc {

// Merges several 8086 COFF objects (I8086Assembler.cpp's own
// `--format=coff` output) into one resolved LinkedImage (see
// LinkedImage.h - `unitBits=8`, one entry per byte). Objects are laid
// out sequentially by byte, starting right after a synthesized entry
// sequence (zero the segment registers, set up the stack, `call
// main`, halt) this linker itself writes at the very front, address
// kI8086BaseAddress (see I8086Assembler.h) - exactly the inline
// sequence I8086Codegen.cpp's own whole-program mode used to emit,
// just assembled here instead as ordinary 8086 text (`call fn_main`
// becomes a genuine external relocation, resolved exactly like any
// other cross-object call - see I8086Linker.cpp), once every
// object's `main` is resolvable (see Codegen.h's `objectMode`/
// I8086Codegen.cpp's own comment on why no object ever emits it
// itself any more).
//
// Every relocation's symbol is resolved one of two ways: the reserved
// pseudo-symbol ".." always means "this specific object's own
// assigned base" (never looked up globally - see I8086Assembler.cpp's
// own comment on why even a same-file address reference needs this
// in object mode); every other name is looked up in the merged global
// symbol table - built from every object's own defined symbol whose
// name starts with "g_"/"p_"/"fn_" (the only ones I8086Codegen.h's
// label() ever gives real cross-file meaning - see merge()'s own
// comment).
class I8086Linker : public TargetLinker {
   public:
    LinkedImage merge(const std::vector<CoffObject> &objects) const override;
    std::vector<std::string> outputFormats() const override { return {"bin", "coff", "aout"}; }
};

}  // namespace pcc
