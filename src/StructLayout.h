#pragma once

#include <string>
#include <vector>

#include "Type.h"

namespace rcc {

// One resolved field of a struct, in declaration order. `type` is already
// fully resolved (nested struct members have their own structSize/
// structAlign filled in too). `arraySize` is 0 for a scalar member, the
// element count for an array member (same "0 = scalar" convention as
// ir::Slot::arraySize).
struct StructMember {
    std::string name;
    Type type;
    int arraySize = 0;
    int byteOffset = 0;
};

struct StructLayout {
    std::string tag;
    std::vector<StructMember> members;
    int size = 0;
    int align = 0;
};

// Computes byte offsets, overall size, and overall alignment for a
// struct's members, given their already-resolved types. `members` must
// be supplied in declaration order; this function does not look
// anything up by name/tag.
//
// When `packed` is false: each member is aligned to its own width (an
// array member to its element width, a nested struct member to its own
// structAlign), overall alignment is the widest member's alignment, and
// overall size is padded up to that alignment (so an array of this
// struct stays aligned).
//
// When `packed` is true ('@packed' on the struct's own definition):
// members are laid out back-to-back with no inter-member padding,
// overall alignment is 1, and overall size is the exact byte sum - no
// trailing padding either.
StructLayout computeStructLayout(const std::string &tag, const std::vector<StructMember> &members,
                                  const TypeWidths &widths, bool packed = false);

}  // namespace rcc
