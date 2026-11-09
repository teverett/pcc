#include "HostedCoffReader.h"

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace pcc {
namespace {

[[noreturn]] void fail(const std::string &message) {
    throw std::runtime_error("hosted COFF reader: " + message);
}

uint32_t field(const std::string &bytes, size_t offset, size_t width) {
    if (offset > bytes.size() || width > bytes.size() - offset) fail("truncated object");
    uint32_t value = 0;
    for (size_t i = 0; i < width; ++i) value |= static_cast<uint32_t>(static_cast<uint8_t>(bytes[offset + i])) << (i * 8);
    return value;
}

}

ObjectFile readHostedCoff(const std::string &bytes) {
    if (field(bytes, 0, 2) != 0x14c || field(bytes, 16, 2) != 0) fail("expected i386 relocatable COFF");
    size_t count = field(bytes, 2, 2);
    if (count == 0 || bytes.size() < 20 || count > (bytes.size() - 20) / 40) fail("invalid section table");
    size_t symbolOffset = field(bytes, 8, 4);
    size_t symbolCount = field(bytes, 12, 4);
    if (symbolOffset > bytes.size() || symbolCount > (bytes.size() - symbolOffset) / 18) fail("invalid symbol table");
    size_t strings = symbolOffset + symbolCount * 18;
    size_t stringsSize = field(bytes, strings, 4);
    if (stringsSize < 4 || stringsSize > bytes.size() - strings) fail("invalid string table");
    auto stringAt = [&](size_t offset) {
        if (offset < 4 || offset >= stringsSize) fail("invalid symbol name offset");
        size_t end = bytes.find('\0', strings + offset);
        if (end == std::string::npos || end >= strings + stringsSize) fail("unterminated symbol name");
        return bytes.substr(strings + offset, end - strings - offset);
    };
    struct Section { uint32_t base, size; bool loaded; };
    std::vector<Section> sections;
    ObjectFile object;
    object.machineType = 0x14c;
    for (size_t i = 0; i < count; ++i) {
        size_t header = 20 + i * 40;
        std::string name = bytes.substr(header, 8);
        size_t nameEnd = name.find('\0');
        if (nameEnd != std::string::npos) name.resize(nameEnd);
        if (!name.empty() && name[0] == '/') {
            size_t offset = 0;
            if (name.size() == 1) fail("invalid section name");
            for (size_t n = 1; n < name.size(); ++n) {
                if (name[n] < '0' || name[n] > '9' || offset > UINT32_MAX / 10) fail("invalid section name");
                offset = offset * 10 + static_cast<size_t>(name[n] - '0');
            }
            name = stringAt(offset);
        }
        uint32_t flags = field(bytes, header + 36, 4);
        uint32_t rawSize = field(bytes, header + 16, 4);
        uint32_t rawOffset = field(bytes, header + 20, 4);
        uint32_t virtualSize = field(bytes, header + 8, 4);
        if (rawSize && (name.rfind(".tls", 0) == 0 || name.rfind(".CRT", 0) == 0 ||
                        name == ".ctors" || name == ".dtors" || name == ".drectve")) {
            fail("unsupported runtime section " + name);
        }
        bool loaded = (flags & (0x20 | 0x40 | 0x80)) != 0 && !(flags & 0x800) &&
                      name.rfind(".debug", 0) != 0;
        if ((flags & 0x1000) && loaded) fail("COMDAT sections are not yet supported");
        uint32_t alignmentBits = (flags >> 20) & 15;
        if (alignmentBits > 14) fail("invalid section alignment");
        size_t alignment = alignmentBits ? size_t{1} << (alignmentBits - 1) : 1;
        if (loaded) object.textAlignment = std::max(object.textAlignment, static_cast<uint32_t>(alignment));
        size_t base = (object.textBytes.size() + alignment - 1) & ~(alignment - 1);
        uint32_t size = std::max(rawSize, virtualSize);
        if (base > UINT32_MAX || size > UINT32_MAX - base) fail("section layout exceeds i386 limits");
        sections.push_back({static_cast<uint32_t>(base), size, loaded});
        if (!loaded) continue;
        object.textBytes.resize(base + size, 0);
        if (rawSize && rawOffset) {
            if (rawOffset > bytes.size() || rawSize > bytes.size() - rawOffset) fail("section data outside object");
            std::copy_n(bytes.begin() + rawOffset, rawSize, object.textBytes.begin() + base);
        } else if (rawSize && !(flags & 0x80)) {
            fail("initialized section has no file data");
        }
    }
    std::vector<std::string> names(symbolCount);
    std::vector<bool> valid(symbolCount, false);
    for (size_t i = 0; i < symbolCount;) {
        size_t offset = symbolOffset + i * 18;
        std::string name;
        if (field(bytes, offset, 4) == 0) name = stringAt(field(bytes, offset + 4, 4));
        else {
            name = bytes.substr(offset, 8);
            size_t end = name.find('\0');
            if (end != std::string::npos) name.resize(end);
        }
        int16_t section = static_cast<int16_t>(field(bytes, offset + 12, 2));
        uint32_t value = field(bytes, offset + 8, 4);
        uint8_t storage = static_cast<uint8_t>(bytes[offset + 16]);
        uint8_t auxiliary = static_cast<uint8_t>(bytes[offset + 17]);
        if (auxiliary > symbolCount - i - 1) fail("invalid auxiliary symbol count");
        if (storage == 105) fail("weak externals are not yet supported");
        if (section == -1 && name != "@feat.00") fail("absolute symbols are not yet supported");
        if (section > static_cast<int>(sections.size())) fail("invalid symbol section");
        if (section > 0 && sections[section - 1].loaded) {
            if (value > sections[section - 1].size) fail("symbol outside section");
            if (storage == 3) name = "__pcc_local_coff_" + std::to_string(i);
            names[i] = name;
            valid[i] = true;
            object.symbols.push_back({name, sections[section - 1].base + value, true, false});
        } else if (section == 0 && storage == 2) {
            if (value) fail("common symbols are not yet supported");
            names[i] = name;
            valid[i] = true;
            object.symbols.push_back({name, 0, false, false});
        }
        i += auxiliary + 1;
    }
    for (size_t i = 0; i < count; ++i) {
        if (!sections[i].loaded) continue;
        size_t header = 20 + i * 40;
        size_t offset = field(bytes, header + 24, 4);
        size_t relocations = field(bytes, header + 32, 2);
        if (offset > bytes.size() || relocations > (bytes.size() - offset) / 10) fail("invalid relocation table");
        for (size_t n = 0; n < relocations; ++n) {
            size_t entry = offset + n * 10;
            uint32_t site = field(bytes, entry, 4);
            uint32_t symbol = field(bytes, entry + 4, 4);
            uint32_t kind = field(bytes, entry + 8, 2);
            if (symbol >= symbolCount || !valid[symbol]) fail("relocation references unsupported symbol");
            if (site > sections[i].size || sections[i].size - site < 4) fail("relocation outside section");
            uint16_t normalized = kind == 6 ? 1 : kind == 20 ? 2 : kind == 7 ? 3 : 0;
            if (!normalized) fail("unsupported i386 relocation " + std::to_string(kind));
            object.relocations.push_back({sections[i].base + site, names[symbol], normalized});
        }
    }
    return object;
}

}
