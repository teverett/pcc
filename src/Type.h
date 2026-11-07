#pragma once

namespace pcc {

// Fixed ISO C conversion-rank order (char .. long long), used by
// commonType(), independent of any target's actual storage widths.
enum class BaseType { Char, Short, Int, Long, LongLong };

// A pointer is the same Type as its ultimate pointee, with `ptrDepth`
// counting levels of pointer-to (char* = {Char, false, 1}, int** =
// {Int, false, 2}); base/isUnsigned always describe the pointee, never the
// pointer itself (whose own storage is always one native int's width).
// ptrDepth == 0 means an ordinary, non-pointer value.
struct Type {
    BaseType base = BaseType::Int;
    bool isUnsigned = false;
    int ptrDepth = 0;

    bool operator==(const Type &other) const {
        return base == other.base && isUnsigned == other.isUnsigned && ptrDepth == other.ptrDepth;
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

}  // namespace pcc
