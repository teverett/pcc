#include "HostedPeExecutableWriter.h"

#include <algorithm>
#include <map>
#include <set>

namespace pcc {
namespace {

using Bytes = std::string;

void put16(Bytes &s, size_t p, uint16_t v) {
    s[p] = static_cast<char>(v);
    s[p + 1] = static_cast<char>(v >> 8);
}
void put32(Bytes &s, size_t p, uint32_t v) {
    put16(s, p, static_cast<uint16_t>(v));
    put16(s, p + 2, static_cast<uint16_t>(v >> 16));
}
size_t alignTo(size_t value, size_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

}  // namespace

Bytes makePe(const PEImage &image) {
    constexpr uint32_t peOffset = 0x80, optSize = 224, headers = 0x200, rawAlign = 0x200, sectionAlign = 0x1000;
    uint32_t virtualSize = static_cast<uint32_t>(image.code.size()) + image.bss;
    uint32_t rawSize = static_cast<uint32_t>(alignTo(image.code.size(), rawAlign));
    uint32_t relocationRva = static_cast<uint32_t>(alignTo(sectionAlign + virtualSize, sectionAlign));
    std::map<uint32_t, std::set<uint16_t>> relocationPages;
    for (uint32_t address : image.baseRelocations) {
        relocationPages[address & ~0xfffu].insert(static_cast<uint16_t>(0x3000u | (address & 0xfffu)));
    }
    Bytes relocations;
    for (const auto &page : relocationPages) {
        size_t start = relocations.size();
        size_t blockSize = alignTo(8 + page.second.size() * 2, 4);
        relocations.resize(start + blockSize, '\0');
        put32(relocations, start, page.first);
        put32(relocations, start + 4, static_cast<uint32_t>(blockSize));
        size_t at = start + 8;
        for (uint16_t entry : page.second) {
            put16(relocations, at, entry);
            at += 2;
        }
    }
    uint32_t relocationRawSize = static_cast<uint32_t>(alignTo(relocations.size(), rawAlign));
    uint32_t imageSize = static_cast<uint32_t>(alignTo(relocationRva + relocations.size(), sectionAlign));
    Bytes out(headers + rawSize + relocationRawSize, '\0');
    out[0] = 'M'; out[1] = 'Z'; put32(out, 0x3c, peOffset);
    out.replace(peOffset, 4, "PE\0\0", 4);
    size_t coff = peOffset + 4;
    put16(out, coff, 0x14c); put16(out, coff + 2, relocations.empty() ? 1 : 2); put32(out, coff + 4, 0);
    put32(out, coff + 8, 0); put32(out, coff + 12, 0); put16(out, coff + 16, optSize);
    put16(out, coff + 18, 0x0102);
    size_t opt = coff + 20;
    put16(out, opt, 0x10b); out[opt + 2] = 14;
    put32(out, opt + 4, rawSize); put32(out, opt + 8, relocationRawSize);
    put32(out, opt + 16, sectionAlign + image.entry);
    put32(out, opt + 20, sectionAlign); put32(out, opt + 24, sectionAlign);
    put32(out, opt + 28, kPeBase); put32(out, opt + 32, sectionAlign);
    put32(out, opt + 36, rawAlign); put16(out, opt + 40, 6); put16(out, opt + 48, 6);
    put32(out, opt + 56, imageSize); put32(out, opt + 60, headers);
    put16(out, opt + 68, 3); put16(out, opt + 70, relocations.empty() ? 0x0100 : 0x0140);
    put32(out, opt + 72, 0x100000); put32(out, opt + 76, 0x1000);
    put32(out, opt + 80, 0x100000); put32(out, opt + 84, 0x1000);
    put32(out, opt + 92, 16);
    if (image.hasWindowsImports) {
        put32(out, opt + 96 + 8, sectionAlign + image.importOffset);
        put32(out, opt + 96 + 12, image.importSize);
        put32(out, opt + 96 + 8 * 12, sectionAlign + image.iatOffset);
        put32(out, opt + 96 + 8 * 12 + 4, image.iatSize);
    }
    if (!relocations.empty()) {
        put32(out, opt + 96 + 8 * 5, relocationRva);
        put32(out, opt + 96 + 8 * 5 + 4, static_cast<uint32_t>(relocations.size()));
    }
    size_t sh = opt + optSize;
    out.replace(sh, 8, ".text\0\0\0", 8);
    put32(out, sh + 8, virtualSize); put32(out, sh + 12, sectionAlign);
    put32(out, sh + 16, rawSize); put32(out, sh + 20, headers);
    put32(out, sh + 36, 0xe0000020);
    std::copy(image.code.begin(), image.code.end(), out.begin() + headers);
    if (!relocations.empty()) {
        sh += 40;
        out.replace(sh, 8, ".reloc\0\0", 8);
        put32(out, sh + 8, static_cast<uint32_t>(relocations.size()));
        put32(out, sh + 12, relocationRva);
        put32(out, sh + 16, relocationRawSize);
        put32(out, sh + 20, headers + rawSize);
        put32(out, sh + 36, 0x42000040);
        std::copy(relocations.begin(), relocations.end(), out.begin() + headers + rawSize);
    }
    return out;
}

}  // namespace pcc
