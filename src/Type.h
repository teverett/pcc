#pragma once

#include <string>

namespace rcc {

// Fixed ISO C conversion-rank order (char .. long long), used by
// commonType(), independent of any target's actual storage widths. Struct
// is never an arithmetic operand (buildExpr rejects that before rank() or
// commonType() would see it), so its placement in the rank order is
// otherwise unused.
enum class BaseType { Char, Short, Int, Long, LongLong, Struct };

// A pointer is the same Type as its ultimate pointee, with `ptrDepth`
// counting levels of pointer-to (char* = {Char, false, 1}, int** =
// {Int, false, 2}); base/isUnsigned always describe the pointee, never the
// pointer itself (whose own storage is always one native int's width).
// ptrDepth == 0 means an ordinary, non-pointer value.
//
// base == Struct: structTag names the struct; structSize/structAlign are
// the resolved, target-specific storage size and required alignment (in
// this target's storage units). BuildAst.cpp sets only structTag, since it
// runs with no TypeWidths in scope; IRBuilder fills structSize/structAlign
// in via resolveStructType() once the target's widths are known.
struct Type {
    BaseType base = BaseType::Int;
    bool isUnsigned = false;
    int ptrDepth = 0;
    std::string structTag;
    int structSize = 0;
    int structAlign = 0;

    // A constructor (rather than plain aggregate init) so every existing
    // `Type{base, isUnsigned}` call site - none of which know or care about
    // structTag/structSize/structAlign - keeps compiling warning-free.
    Type() = default;
    Type(BaseType b, bool u = false, int depth = 0) : base(b), isUnsigned(u), ptrDepth(depth) {}

    bool operator==(const Type &other) const {
        return base == other.base && isUnsigned == other.isUnsigned && ptrDepth == other.ptrDepth &&
               structTag == other.structTag;
    }
    bool operator!=(const Type &other) const { return !(*this == other); }
};

// One target's storage unit for each type: bytes for a byte-addressable
// backend, or the native word for a word-addressed one (e.g. pdp7).
struct TypeWidths {
    int charUnits;
    int shortUnits;
    int intUnits;
    int longUnits;
    int longLongUnits;
    int unitBits = 8;  // bits in one storage unit
    int pointerUnits = 0;  // 0 uses intUnits
};

// Char=0 .. LongLong=4.
int rank(BaseType base);

// Storage units of type `t` on this target. A pointer always occupies one
// native int regardless of pointee; only a dereference's width depends on
// the pointee type.
int widthOf(Type t, const TypeWidths &widths);

// Usual arithmetic conversions for a binary operator's two operands:
// greater rank wins, equal rank unsigned wins. Does not promote either
// operand first (callers promote sub-Int operands themselves) and is not
// meaningful on a pointer operand (buildExpr special-cases those first).
Type commonType(Type a, Type b);

inline bool isPointer(Type t) { return t.ptrDepth > 0; }

// Error to call on a non-pointer.
inline Type pointeeOf(Type t) {
    t.ptrDepth--;
    return t;
}

inline Type pointerTo(Type t) {
    t.ptrDepth++;
    return t;
}

}  // namespace rcc
