#pragma once

#include "../Type.h"
#include "ast/AST.h"
#include "IR.h"

namespace rcc {

// Lowers the AST into IR.h's resolved form: every Var/Index/Assign gets a
// direct ir::Slot*, every Call a direct ir::Function*, and all
// target-independent semantic checks (redeclaration, missing/malformed
// main, bare-array-without-subscript) happen here once. Throws
// std::runtime_error with a "line N: ..." message on error.
//
// `allowExternal` (--format=coff) makes an undefined name become an
// external ir::Function/Slot instead of an error, and drops the
// requirement that `prog` define main.
//
// `widths` (the backend's TypeWidths) is used only to fold `sizeof` and to
// compute Local/Param frame layout in real storage units; type-checking
// itself stays target-independent.
ir::Program buildIR(const Program &prog, const TypeWidths &widths, bool allowExternal = false);

// Reassigns every Slot::frameOffset/Function::frameSize from the tree's
// current declaration order. Needed after an optimizer (e.g. LocalCSE)
// introduces a new local past buildIR's one-time layout pass, which would
// otherwise leave it at frameOffset 0, aliasing an existing slot.
void recomputeFrameLayout(ir::Program &prog, const TypeWidths &widths);

}  // namespace rcc
