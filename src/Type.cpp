#include "Type.h"

namespace rcc {

int rank(BaseType base) {
    switch (base) {
        case BaseType::Char:
            return 0;
        case BaseType::Short:
            return 1;
        case BaseType::Int:
            return 2;
        case BaseType::Long:
            return 3;
        case BaseType::LongLong:
            return 4;
    }
    return 2;
}

int widthOf(Type t, const TypeWidths &widths) {
    if (t.ptrDepth > 0) return widths.pointerUnits > 0 ? widths.pointerUnits : widths.intUnits;
    switch (t.base) {
        case BaseType::Char:
            return widths.charUnits;
        case BaseType::Short:
            return widths.shortUnits;
        case BaseType::Int:
            return widths.intUnits;
        case BaseType::Long:
            return widths.longUnits;
        case BaseType::LongLong:
            return widths.longLongUnits;
    }
    return widths.intUnits;
}

Type commonType(Type a, Type b) {
    if (rank(a.base) == rank(b.base)) {
        Type result = a;
        result.isUnsigned = a.isUnsigned || b.isUnsigned;
        return result;
    }
    return rank(a.base) > rank(b.base) ? a : b;
}

}  // namespace rcc
