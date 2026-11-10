#include "HostedElfReader.h"

#include <filesystem>
#include <map>
#include <stdexcept>

namespace rcc {
namespace {

using Bytes = std::string;

[[noreturn]] void fail(const std::string &message) {
    throw std::runtime_error("hosted ELF reader: " + message);
}

uint16_t get16(const Bytes &s, size_t p) {
    if (p > s.size() || s.size() - p < 2) fail("truncated object file");
    return static_cast<uint8_t>(s[p]) | (static_cast<uint16_t>(static_cast<uint8_t>(s[p + 1])) << 8);
}
uint32_t get32(const Bytes &s, size_t p) {
    if (p > s.size() || s.size() - p < 4) fail("truncated object file");
    return static_cast<uint32_t>(get16(s, p)) | (static_cast<uint32_t>(get16(s, p + 2)) << 16);
}

}  // namespace

DynamicLibrary readElfDynamicLibrary(const Bytes &bytes, const std::string &name) {
    if (bytes.size() < 52 || bytes.compare(0, 4, "\x7f" "ELF", 4) != 0 ||
        static_cast<uint8_t>(bytes[4]) != 1 || static_cast<uint8_t>(bytes[5]) != 1 ||
        get16(bytes, 16) != 3 || get16(bytes, 18) != 3) {
        fail("shared-library input must be an i386 ELF ET_DYN file: '" +
             name + "'");
    }
    uint32_t sectionOffset = get32(bytes, 32);
    uint16_t sectionEntrySize = get16(bytes, 46), sectionCount = get16(bytes, 48);
    if (sectionEntrySize < 40 || sectionOffset > bytes.size() ||
        sectionCount > (bytes.size() - sectionOffset) / sectionEntrySize) fail("invalid ELF shared-library section table");
    DynamicLibrary library;
    library.name = std::filesystem::path(name).filename().string();
    auto sectionRange = [&](size_t section) {
        uint32_t offset = get32(bytes, section + 16), size = get32(bytes, section + 20);
        if (offset > bytes.size() || size > bytes.size() - offset) fail("invalid ELF shared-library section range");
        return std::make_pair(offset, size);
    };
    auto stringAt = [&](uint32_t offset, uint32_t size, uint32_t index) {
        if (index >= size) fail("ELF dynamic string offset outside string table");
        size_t end = bytes.find('\0', offset + index);
        if (end == Bytes::npos || end >= offset + size) fail("unterminated ELF dynamic string");
        return bytes.substr(offset + index, end - offset - index);
    };
    std::map<uint16_t, std::string> versions;
    for (uint16_t i = 0; i < sectionCount; ++i) {
        size_t section = sectionOffset + i * sectionEntrySize;
        uint32_t type = get32(bytes, section + 4);
        if (type != 6 && type != 0x6ffffffd) continue;
        auto range = sectionRange(section);
        uint32_t link = get32(bytes, section + 24);
        if (link >= sectionCount) fail("invalid ELF shared-library string link");
        auto strings = sectionRange(sectionOffset + link * sectionEntrySize);
        if (type == 6) {
            if (range.second % 8) fail("invalid ELF shared-library dynamic table");
            for (size_t at = range.first; at < range.first + range.second; at += 8) {
                if (get32(bytes, at) == 14) library.name = stringAt(strings.first, strings.second, get32(bytes, at + 4));
            }
        } else {
            size_t at = range.first, end = at + range.second;
            for (uint32_t n = 0; n < get32(bytes, section + 28); ++n) {
                if (at > end || end - at < 20 || get16(bytes, at) != 1) fail("invalid ELF version definition");
                uint32_t aux = get32(bytes, at + 12), next = get32(bytes, at + 16);
                if (aux < 20 || aux > end - at || end - at - aux < 8) fail("invalid ELF version definition auxiliary");
                versions[get16(bytes, at + 4) & 0x7fff] =
                    stringAt(strings.first, strings.second, get32(bytes, at + aux));
                if (!next) {
                    if (n + 1 != get32(bytes, section + 28)) fail("truncated ELF version definitions");
                    break;
                }
                if (next < 20 || next > end - at) fail("invalid ELF version definition chain");
                at += next;
            }
        }
    }
    for (uint16_t i = 0; i < sectionCount; ++i) {
        size_t section = sectionOffset + i * sectionEntrySize;
        if (get32(bytes, section + 4) != 11) continue;
        uint32_t symbolOffset = get32(bytes, section + 16), symbolSize = get32(bytes, section + 20);
        uint32_t stringIndex = get32(bytes, section + 24), symbolEntrySize = get32(bytes, section + 36);
        if (symbolEntrySize < 16 || stringIndex >= sectionCount ||
            symbolSize % symbolEntrySize || symbolOffset > bytes.size() ||
            symbolSize > bytes.size() - symbolOffset) fail("invalid ELF dynamic symbol table");
        size_t stringsSection = sectionOffset + stringIndex * sectionEntrySize;
        uint32_t stringsOffset = get32(bytes, stringsSection + 16), stringsSize = get32(bytes, stringsSection + 20);
        if (stringsOffset > bytes.size() || stringsSize > bytes.size() - stringsOffset) {
            fail("invalid ELF dynamic string table");
        }
        uint32_t versionOffset = 0;
        for (uint16_t j = 0; j < sectionCount; ++j) {
            size_t versionSection = sectionOffset + j * sectionEntrySize;
            if (get32(bytes, versionSection + 4) != 0x6fffffff ||
                get32(bytes, versionSection + 24) != i) continue;
            auto range = sectionRange(versionSection);
            if (range.second != (symbolSize / symbolEntrySize) * 2) fail("invalid ELF symbol version table");
            versionOffset = range.first;
        }
        for (uint32_t at = 0; at < symbolSize; at += symbolEntrySize) {
            size_t symbol = symbolOffset + at;
            uint32_t string = get32(bytes, symbol);
            uint8_t binding = static_cast<uint8_t>(bytes[symbol + 12]) >> 4;
            uint16_t sectionIndex = get16(bytes, symbol + 14);
            if (string == 0 || sectionIndex == 0 || (binding != 1 && binding != 2)) continue;
            uint8_t visibility = static_cast<uint8_t>(bytes[symbol + 13]) & 3;
            if (visibility == 1 || visibility == 2) continue;
            uint16_t version = versionOffset ? get16(bytes, versionOffset + (at / symbolEntrySize) * 2) : 1;
            if (version & 0x8000) continue;
            std::string versionName;
            if (version > 1) {
                auto found = versions.find(version);
                if (found == versions.end()) fail("missing ELF symbol version definition");
                versionName = found->second;
            }
            library.exports[stringAt(stringsOffset, stringsSize, string)] =
                {versionName, static_cast<uint8_t>(static_cast<uint8_t>(bytes[symbol + 12]) & 15)};
        }
    }
    return library;
}

bool isElfDynamicImage(const Bytes &bytes) {
    return bytes.size() >= 20 && bytes.compare(0, 4, "\x7f" "ELF", 4) == 0 &&
           get16(bytes, 16) == 3;
}

void validateHostedElfObject(const Bytes &bytes) {
    if (bytes.size() < 52 || bytes.compare(0, 4, "\x7f" "ELF", 4) != 0 ||
        static_cast<uint8_t>(bytes[4]) != 1 || static_cast<uint8_t>(bytes[5]) != 1 ||
        get16(bytes, 16) != 1 || get16(bytes, 18) != 3)
        fail("hosted ELF objects must be little-endian i386 ET_REL");
    uint32_t sections = get32(bytes, 32);
    uint16_t size = get16(bytes, 46), count = get16(bytes, 48), textIndex = 0xffff;
    if (size != 40 || sections > bytes.size() || count > (bytes.size() - sections) / size)
        fail("invalid hosted ELF object section table");
    unsigned bssCount = 0;
    for (uint16_t i = 0; i < count; ++i) {
        size_t at = sections + i * size;
        uint32_t type = get32(bytes, at + 4), flags = get32(bytes, at + 8);
        uint32_t length = get32(bytes, at + 20);
        if (type == 1 && (flags & 2) && length) {
            if (!(flags & 4) || textIndex != 0xffff)
                fail("unsupported allocated ELF object section; hosted static objects use one .text and .bss");
            textIndex = i;
        } else if (type == 8 && (flags & 2) && length) {
            if ((flags & 0x400) || ++bssCount > 1) fail("unsupported hosted ELF TLS/multiple BSS sections");
        } else if ((flags & 2) && length) fail("unsupported allocated ELF object section");
    }
    for (uint16_t i = 0; i < count; ++i) {
        size_t at = sections + i * size;
        if (get32(bytes, at + 4) == 9 && get32(bytes, at + 20) &&
            get32(bytes, at + 28) != textIndex)
            fail("unsupported ELF object relocation target section");
    }
}

}  // namespace rcc
