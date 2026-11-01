#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace pcc {

// The output of a TargetLinker's own architecture-specific merge
// step (see Linker.h) - a fully resolved program, every relocation
// already patched in, in a shape any FormatWriter (see
// FormatWriter.h) can write out without needing to know which
// architecture produced it. `words` is a sequence of address units
// starting at `base` - pdp7's own unit is an 18-bit word (so `base`
// and every `symbols` value is a *word* address); 8086's is a plain
// byte (so they're *byte* addresses). `unitBits` records which,
// purely so a writer that only makes sense for one or the other
// (pdp7's own paper-tape/octal-listing formats all assume 18-bit
// words; 8086's boot-sector format assumes bytes) can fail loudly if
// it's ever wired up to the wrong one, rather than silently
// misinterpreting the image - no writer actually branches on it.
// `machineType` is the same custom COFF `Machine` value each
// assembler's own `--format=coff` writes (see CoffWriter.h) -
// CoffLinkWriter is the one writer that does need it, to write a
// correct container regardless of which architecture built `words`.
struct LinkedImage {
    uint32_t unitBits = 0;
    uint32_t base = 0;
    uint16_t machineType = 0;
    std::vector<uint32_t> words;
    std::unordered_map<std::string, uint32_t> symbols;
};

}  // namespace pcc
