#pragma once

#include "../Type.h"
#include "ast/AST.h"
#include "IR.h"

namespace pcc {

// Lowers the AST (parsed straight from source - see AST.h/BuildAst.h)
// into IR.h's resolved form: every Var/Index/Assign gets a direct
// ir::Slot*, every Call a direct ir::Function*, scoping is fully
// resolved, and the semantic checks that don't depend on any target
// (redeclaration, "no main", "main takes no parameters", a bare
// array used without a subscript) all happen here, once, instead of
// separately in every backend. Throws std::runtime_error with a
// "line N: ..." message on any such error, the same as BuildAst does
// for syntax-level ones.
//
// `allowExternal` is real separate compilation's own switch
// (`--format=coff`, see main.cpp): when true, a name not defined in
// `prog` becomes an external `ir::Function`/`ir::Slot`
// (`isExternal=true`, see IR.h) instead of a compile error, and `prog`
// isn't required to define `main` at all (an ordinary, non-entry
// object file never does). When false (the default - every other
// format), behavior is unchanged: a missing name is always an error,
// and `prog` must define a parameterless `main`.
//
// `widths` is the selected backend's own TypeWidths (Backend::
// typeWidths(), see codegen/Codegen.h) - used here only to fold
// `sizeof` into a constant and to compute every Local/Param's own
// Slot::frameOffset/Function::frameSize in real storage units (see
// their own comments in IR.h); the promotion/usual-arithmetic-
// conversion rules a type-checking pass needs are deliberately
// target-independent and never consult this table - see Type.h's own
// header comment.
ir::Program buildIR(const Program &prog, const TypeWidths &widths, bool allowExternal = false);

// Re-walks every function body's own `LocalDecl`s and reassigns each
// one's `Slot::frameOffset` (and its `Function::frameSize`) from
// scratch, in whatever declaration order the tree currently has -
// exactly what `buildIR` itself already does once, right after
// building a function's body. An optimizer that introduces a brand
// new local (see `LocalCSE.cpp`) runs after that one-time assignment
// already happened, so without a second pass here, the new local's
// `frameOffset` would be left at its default (0), silently aliasing
// whatever real local or parameter already sits there. Needs the same
// `widths` buildIR itself was called with.
void recomputeFrameLayout(ir::Program &prog, const TypeWidths &widths);

}  // namespace pcc
