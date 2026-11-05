#pragma once

namespace pcc {

// The five integer ranks this subset's type system knows (char ..  long
// long) - the fixed ISO C conversion-rank order used by commonType()
// below, independent of how many storage units any given target actually
// gives each one (see TypeWidths).
enum class BaseType { Char, Short, Int, Long, LongLong };

struct Type {
    BaseType base = BaseType::Int;
    bool isUnsigned = false;

    bool operator==(const Type &other) const {
        return base == other.base && isUnsigned == other.isUnsigned;
    }
    bool operator!=(const Type &other) const { return !(*this == other); }
};

// One target's own storage unit for each type - bytes for every
// byte-addressable backend, or that target's own native word for a
// word-addressed one (pdp7, which has no byte addressing at all). Each
// backend supplies this once, next to its existing documented int-width
// comment (see Backend::typeWidths() in codegen/Codegen.h).
struct TypeWidths {
    int charUnits;
    int shortUnits;
    int intUnits;
    int longUnits;
    int longLongUnits;
};

// Char=0 .. LongLong=4 - the fixed rank order itself, used by
// commonType() and by integer promotion (anything below Int promotes to
// Int) - see Type.cpp's own header comment for why this never depends on
// a target's actual widths.
int rank(BaseType base);

// How many of `widths`'s storage units a value of type `t` occupies on
// that target.
int widthOf(Type t, const TypeWidths &widths);

// The result type of a binary operator applied to two already-built
// operands, per the "usual arithmetic conversions" - greater rank wins;
// equal rank, unsigned wins. Does not itself apply integer promotion to
// either operand - callers promote sub-Int operands first (see
// IRBuilder.cpp's buildExpr).
Type commonType(Type a, Type b);

}  // namespace pcc
