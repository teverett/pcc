#include "CoffLinkWriter.h"

#include <algorithm>
#include <stdexcept>
#include <vector>

#include "coff/CoffWriter.h"

namespace pcc {

std::string CoffLinkWriter::write(const LinkedImage &image) const {
    // Only the two shapes any TargetLinker actually produces today
    // are supported - anything else would need a packing convention
    // nobody's defined yet (see this file's own header comment).
    if (image.unitBits != 8 && image.unitBits != 18) {
        throw std::runtime_error("CoffLinkWriter: don't know how to pack a " + std::to_string(image.unitBits) +
                                  "-bit-word image into COFF bytes");
    }
    uint32_t bytesPerUnit = (image.unitBits + 7) / 8;

    std::vector<uint8_t> text;
    text.reserve(image.words.size() * bytesPerUnit);
    for (uint32_t w : image.words) {
        for (uint32_t b = bytesPerUnit; b-- > 0;) {
            text.push_back(static_cast<uint8_t>((w >> (8 * b)) & 0xFF));
        }
    }

    std::vector<ObjSymbol> symbols;
    for (const auto &kv : image.symbols) {
        if (kv.second < image.base) continue;  // not part of the emitted .text (shouldn't happen once linked)
        symbols.push_back({kv.first, (kv.second - image.base) * bytesPerUnit, /*isDefined=*/true});
    }
    std::sort(symbols.begin(), symbols.end(), [](const ObjSymbol &x, const ObjSymbol &y) { return x.name < y.name; });

    return writeCoffObject(image.machineType, text, symbols);
}

}  // namespace pcc
