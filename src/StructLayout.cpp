#include "StructLayout.h"

#include <algorithm>

namespace rcc {

namespace {

int alignUp(int value, int align) {
    if (align <= 1) return value;
    return ((value + align - 1) / align) * align;
}

// A member's own alignment: an array member aligns like one element, a
// struct member (nested or array-of-struct) aligns to its own structAlign.
int memberAlign(const StructMember &member, const TypeWidths &widths) {
    if (member.type.base == BaseType::Struct) return member.type.structAlign;
    return widthOf(member.type, widths);
}

int memberElemSize(const StructMember &member, const TypeWidths &widths) {
    if (member.type.base == BaseType::Struct) return member.type.structSize;
    return widthOf(member.type, widths);
}

}  // namespace

StructLayout computeStructLayout(const std::string &tag, const std::vector<StructMember> &members,
                                  const TypeWidths &widths, bool packed) {
    StructLayout layout;
    layout.tag = tag;
    layout.members = members;
    int offset = 0;
    int maxAlign = 1;
    for (auto &member : layout.members) {
        if (!packed) {
            int align = memberAlign(member, widths);
            offset = alignUp(offset, align);
            maxAlign = std::max(maxAlign, align);
        }
        member.byteOffset = offset;
        int count = member.arraySize > 0 ? member.arraySize : 1;
        offset += memberElemSize(member, widths) * count;
    }
    layout.align = packed ? 1 : maxAlign;
    layout.size = packed ? offset : alignUp(offset, maxAlign);
    return layout;
}

}  // namespace rcc
