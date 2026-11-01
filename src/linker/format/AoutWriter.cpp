#include "AoutWriter.h"

#include <stdexcept>

namespace pcc {

namespace {

void put16(std::string &out, uint16_t v) {
    out.push_back(static_cast<char>(v & 0xFF));
    out.push_back(static_cast<char>((v >> 8) & 0xFF));
}

// OMAGIC - the oldest, simplest V7 Unix a.out magic number: an
// "impure" executable, text and data not split into separate pages.
// The real OMAGIC/NMAGIC/ZMAGIC distinction existed for a memory-
// management reason (whether the text segment can be mapped read-
// only and shared) that doesn't apply here at all - a LinkedImage has
// no text/data split of its own (see IR.h/Codegen.h: `pcc` never
// separates code from data in the first place) - so OMAGIC, "just
// one blob", is the only one of the three that's actually honest
// about that.
constexpr uint16_t kOMagic = 0407;

}  // namespace

std::string AoutWriter::write(const LinkedImage &image) const {
    // Only the two shapes any TargetLinker actually produces today
    // are supported - same reasoning as CoffLinkWriter.cpp's own
    // check.
    if (image.unitBits != 8 && image.unitBits != 18) {
        throw std::runtime_error("AoutWriter: don't know how to pack a " + std::to_string(image.unitBits) +
                                  "-bit-word image into a.out bytes");
    }
    uint32_t bytesPerUnit = (image.unitBits + 7) / 8;

    std::string text;
    text.reserve(image.words.size() * bytesPerUnit);
    for (uint32_t w : image.words) {
        for (uint32_t b = bytesPerUnit; b-- > 0;) {
            text.push_back(static_cast<char>((w >> (8 * b)) & 0xFF));
        }
    }

    // struct exec (V7 Unix, see a.out(5)): a_magic, a_text, a_data,
    // a_bss, a_syms, a_entry, a_unused, a_flag - 8 16-bit fields, 16
    // bytes total. No data/bss/symbol-table split here (see kOMagic's
    // own comment above) and no relocation bits in `a_flag`: this
    // image is already fully linked, same as everything else `--link`
    // produces. `a_entry` is this project's own documented choice for
    // a format whose real-world variants disagree on what that field
    // even means: here it's the absolute address execution starts at
    // (`image.base` - every writer in this project already places
    // word/byte 0 of `words` at the entry sequence synthesized by
    // the matching TargetLinker::merge()), not a text-relative offset
    // the way some later a.out variants used it.
    std::string header;
    put16(header, kOMagic);
    put16(header, static_cast<uint16_t>(text.size()));  // a_text
    put16(header, 0);                                   // a_data
    put16(header, 0);                                   // a_bss
    put16(header, 0);                                   // a_syms
    put16(header, static_cast<uint16_t>(image.base));   // a_entry
    put16(header, 0);                                   // a_unused
    put16(header, 0);                                   // a_flag

    return header + text;
}

}  // namespace pcc
