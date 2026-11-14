#pragma once

#include <string>

#include "ir/IR.h"

namespace rcc {

// Hoists the base address of a real array (a Local/Global Slot with
// arraySize > 0) out of every repeated Index/Assign/CompoundAssign that
// names it within one function, into a single pointer-typed temporary
// computed once. Without this, every backend re-derives this address
// from scratch at each use, which is cheap on most targets but costs
// e.g. 6 words every time on pdp7, whose `law` instruction loads its
// own raw opcode bits alongside the address and needs an extra masking
// `and`.
//
// Deliberately conservative: unlike a plain pointer variable, a real
// array's storage address never changes during one function activation
// (no reassignment, no aliasing through it to reason about), so no
// invalidation analysis is needed at all - every Index/Assign/
// CompoundAssign anywhere in the function body naming the same array
// Slot is a candidate, hoisted unconditionally once 2+ occurrences
// exist. Needs a target's TypeWidths (to scale the index by the array's
// element width, the same convention IRBuilder.cpp's own pointer
// arithmetic uses), so unlike the other optimizer passes this isn't an
// Optimizer - it's called directly from main.cpp, like
// recomputeFrameLayout().
class AddressHoisting {
   public:
    std::string name() const { return "address-hoisting"; }
    bool run(ir::Program &prog, const TypeWidths &widths, const std::string &sourceFile) const;
};

}  // namespace rcc
