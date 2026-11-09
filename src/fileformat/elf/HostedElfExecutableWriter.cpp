#include "HostedElfExecutableWriter.h"

#include <algorithm>
#include <map>
#include <stdexcept>
#include <unordered_map>

namespace pcc {
namespace {

using Bytes = std::string;
constexpr uint32_t kPage = 4096;

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

Bytes makeElf(const ELFImage &image) {
    constexpr uint32_t ehsize = 52, phsize = 32, codeOff = 0x1000, interpOff = 0x200;
    bool dynamic = !image.dynamicLibraries.empty();
    uint32_t phnum = dynamic ? 4 : 2;
    Bytes strings(1, '\0');
    std::unordered_map<std::string, uint32_t> stringOffsets;
    for (const auto &library : image.dynamicLibraries) {
        stringOffsets[library.name] = static_cast<uint32_t>(strings.size());
        strings += library.name;
        strings.push_back('\0');
    }
    for (const auto &item : image.dynamicImports) {
        if (!stringOffsets.count(item.symbol)) {
            stringOffsets[item.symbol] = static_cast<uint32_t>(strings.size());
            strings += item.symbol;
            strings.push_back('\0');
        }
        if (!item.version.empty() && !stringOffsets.count(item.version)) {
            stringOffsets[item.version] = static_cast<uint32_t>(strings.size());
            strings += item.version;
            strings.push_back('\0');
        }
    }
    auto elfHash = [](const std::string &name) {
        uint32_t hash = 0;
        for (unsigned char c : name) {
            hash = (hash << 4) + c;
            uint32_t high = hash & 0xf0000000;
            if (high) hash ^= high >> 24;
            hash &= ~high;
        }
        return hash;
    };
    std::map<std::string, std::map<std::string, uint16_t>> requiredVersions;
    uint16_t versionCount = 2;
    for (const auto &item : image.dynamicImports) {
        if (!item.version.empty() && !requiredVersions[item.library].count(item.version)) {
            if (versionCount == 0x7fff) throw std::runtime_error("too many ELF symbol version requirements");
            requiredVersions[item.library][item.version] = versionCount++;
        }
    }
    Bytes versym((image.dynamicImports.size() + 1) * 2, '\0'), verneed;
    for (size_t i = 0; i < image.dynamicImports.size(); ++i) {
        const auto &item = image.dynamicImports[i];
        put16(versym, (i + 1) * 2, item.version.empty() ? 1 : requiredVersions[item.library][item.version]);
    }
    for (const auto &group : requiredVersions) {
        size_t start = verneed.size(), size = 16 + group.second.size() * 16;
        verneed.resize(start + size, '\0');
        put16(verneed, start, 1);
        put16(verneed, start + 2, static_cast<uint16_t>(group.second.size()));
        put32(verneed, start + 4, stringOffsets.at(group.first));
        put32(verneed, start + 8, 16);
        put32(verneed, start + 12, group.first == requiredVersions.rbegin()->first ? 0 : static_cast<uint32_t>(size));
        size_t at = start + 16;
        for (const auto &version : group.second) {
            put32(verneed, at, elfHash(version.first));
            put16(verneed, at + 6, version.second);
            put32(verneed, at + 8, stringOffsets.at(version.first));
            put32(verneed, at + 12, at + 16 < start + size ? 16 : 0);
            at += 16;
        }
    }
    Bytes dynsym((image.dynamicImports.size() + 1) * 16, '\0');
    for (size_t i = 0; i < image.dynamicImports.size(); ++i) {
        size_t symbol = (i + 1) * 16;
        put32(dynsym, symbol, stringOffsets[image.dynamicImports[i].symbol]);
        dynsym[symbol + 12] = 0x12;
    }
    uint32_t symbolCount = static_cast<uint32_t>(image.dynamicImports.size() + 1);
    Bytes hash(12 + symbolCount * 4, '\0');
    put32(hash, 0, 1);
    put32(hash, 4, symbolCount);
    put32(hash, 8, image.dynamicImports.empty() ? 0 : 1);
    for (uint32_t i = 1; i < symbolCount; ++i) put32(hash, 12 + i * 4, i + 1 < symbolCount ? i + 1 : 0);
    Bytes relocations(image.dynamicImports.size() * 8, '\0');
    for (size_t i = 0; i < image.dynamicImports.size(); ++i) {
        put32(relocations, i * 8, kElfBase + codeOff + image.dynamicImports[i].gotOffset);
        put32(relocations, i * 8 + 4, static_cast<uint32_t>((i + 1) << 8) | 7);
    }
    uint32_t dynamicOff = 0, strOff = 0, symOff = 0, hashOff = 0, relOff = 0, versymOff = 0, verneedOff = 0;
    Bytes dynamicEntries;
    Bytes metadata;
    uint32_t dynamicSize = 0;
    if (dynamic) {
        metadata.resize(alignTo(metadata.size(), 4), '\0');
        strOff = codeOff + static_cast<uint32_t>(image.code.size() + metadata.size());
        metadata += strings;
        metadata.resize(alignTo(metadata.size(), 4), '\0');
        symOff = codeOff + static_cast<uint32_t>(image.code.size() + metadata.size());
        metadata += dynsym;
        hashOff = codeOff + static_cast<uint32_t>(image.code.size() + metadata.size());
        metadata += hash;
        if (!relocations.empty()) {
            metadata.resize(alignTo(metadata.size(), 4), '\0');
            relOff = codeOff + static_cast<uint32_t>(image.code.size() + metadata.size());
            metadata += relocations;
        }
        if (!verneed.empty()) {
            metadata.resize(alignTo(metadata.size(), 4), '\0');
            versymOff = codeOff + static_cast<uint32_t>(image.code.size() + metadata.size());
            metadata += versym;
            metadata.resize(alignTo(metadata.size(), 4), '\0');
            verneedOff = codeOff + static_cast<uint32_t>(image.code.size() + metadata.size());
            metadata += verneed;
        }
        metadata.resize(alignTo(metadata.size(), 4), '\0');
        dynamicOff = codeOff + static_cast<uint32_t>(image.code.size() + metadata.size());
        auto dynamicEntry = [&](uint32_t tag, uint32_t value) {
            size_t at = dynamicEntries.size();
            dynamicEntries.resize(at + 8, '\0');
            put32(dynamicEntries, at, tag);
            put32(dynamicEntries, at + 4, value);
        };
        for (const auto &library : image.dynamicLibraries) dynamicEntry(1, stringOffsets[library.name]);
        if (!image.dynamicImports.empty()) {
            dynamicEntry(3, kElfBase + codeOff + image.gotOffset);
            dynamicEntry(23, kElfBase + relOff);
            dynamicEntry(2, static_cast<uint32_t>(relocations.size()));
            dynamicEntry(20, 17);
            dynamicEntry(19, 8);
        }
        dynamicEntry(4, kElfBase + hashOff);
        dynamicEntry(5, kElfBase + strOff);
        dynamicEntry(6, kElfBase + symOff);
        dynamicEntry(10, static_cast<uint32_t>(strings.size()));
        dynamicEntry(11, 16);
        if (!verneed.empty()) {
            dynamicEntry(0x6ffffff0, kElfBase + versymOff);
            dynamicEntry(0x6ffffffe, kElfBase + verneedOff);
            dynamicEntry(0x6fffffff, static_cast<uint32_t>(requiredVersions.size()));
        }
        dynamicEntry(0, 0);
        dynamicSize = static_cast<uint32_t>(dynamicEntries.size());
        metadata += dynamicEntries;
    }
    uint32_t total = codeOff + static_cast<uint32_t>(image.code.size() + metadata.size());
    Bytes out(total, '\0');
    out[0] = 0x7f; out[1] = 'E'; out[2] = 'L'; out[3] = 'F';
    out[4] = 1; out[5] = 1; out[6] = 1;
    put16(out, 16, 2); put16(out, 18, 3); put32(out, 20, 1);
    put32(out, 24, kElfBase + codeOff + image.entry); put32(out, 28, ehsize);
    put16(out, 40, ehsize); put16(out, 42, phsize); put16(out, 44, static_cast<uint16_t>(phnum));
    size_t p = ehsize;
    put32(out, p, 1); put32(out, p + 4, 0); put32(out, p + 8, kElfBase);
    put32(out, p + 12, kElfBase); put32(out, p + 16, total);
    put32(out, p + 20, total + image.bss); put32(out, p + 24, 7); put32(out, p + 28, kPage);
    p += phsize;
    if (dynamic) {
        std::string interpreter = "/lib/ld-linux.so.2";
        out.replace(interpOff, interpreter.size() + 1, interpreter + '\0');
        put32(out, p, 3); put32(out, p + 4, interpOff); put32(out, p + 8, kElfBase + interpOff);
        put32(out, p + 12, kElfBase + interpOff); put32(out, p + 16, interpreter.size() + 1);
        put32(out, p + 20, interpreter.size() + 1); put32(out, p + 24, 4); put32(out, p + 28, 1);
        p += phsize;
        put32(out, p, 2); put32(out, p + 4, dynamicOff); put32(out, p + 8, kElfBase + dynamicOff);
        put32(out, p + 12, kElfBase + dynamicOff); put32(out, p + 16, dynamicSize);
        put32(out, p + 20, dynamicSize); put32(out, p + 24, 6); put32(out, p + 28, 4);
        p += phsize;
    }
    put32(out, p, 0x6474e551); put32(out, p + 4, 0); put32(out, p + 8, 0);
    put32(out, p + 12, 0); put32(out, p + 16, 0); put32(out, p + 20, 0);
    put32(out, p + 24, 6); put32(out, p + 28, 16);
    std::copy(image.code.begin(), image.code.end(), out.begin() + codeOff);
    if (dynamic) {
        std::copy(metadata.begin(), metadata.end(), out.begin() + codeOff + image.code.size());
        if (image.gotOffset != 0) put32(out, codeOff + image.gotOffset, kElfBase + dynamicOff);
    }
    return out;
}

}  // namespace pcc
