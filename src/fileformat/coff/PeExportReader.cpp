#include "PeExportReader.h"

#include <stdexcept>

namespace pcc {
namespace {

[[noreturn]] void fail(const std::string &message) {
    throw std::runtime_error("PE export reader: " + message);
}

uint32_t read(const std::string &bytes, size_t offset, size_t width) {
    if (offset > bytes.size() || width > bytes.size() - offset) fail("truncated DLL");
    uint32_t value = 0;
    for (size_t i = 0; i < width; ++i) value |= static_cast<uint32_t>(static_cast<uint8_t>(bytes[offset + i])) << (i * 8);
    return value;
}

}

std::vector<PeExport> readPeExports(const std::string &bytes) {
    if (bytes.size() < 64 || bytes.compare(0, 2, "MZ") != 0) fail("expected PE32 DLL");
    size_t pe = read(bytes, 60, 4);
    if (pe > bytes.size() || bytes.size() - pe < 24 || bytes.compare(pe, 4, std::string("PE\0\0", 4)) != 0 ||
        read(bytes, pe + 4, 2) != 0x14c || !(read(bytes, pe + 22, 2) & 0x2000)) {
        fail("expected i386 DLL");
    }
    size_t optional = pe + 24;
    size_t optionalSize = read(bytes, pe + 20, 2);
    if (optionalSize < 104 || optional > bytes.size() || optionalSize > bytes.size() - optional ||
        read(bytes, optional, 2) != 0x10b || read(bytes, optional + 92, 4) == 0) {
        fail("invalid PE32 optional header");
    }
    uint32_t exportRva = read(bytes, optional + 96, 4);
    uint32_t exportSize = read(bytes, optional + 100, 4);
    if (!exportRva) return {};
    size_t sections = optional + optionalSize;
    size_t sectionCount = read(bytes, pe + 6, 2);
    if (sections > bytes.size() || sectionCount > (bytes.size() - sections) / 40) fail("truncated section table");
    auto location = [&](uint32_t rva, size_t width) -> size_t {
        for (size_t i = 0; i < sectionCount; ++i) {
            size_t section = sections + i * 40;
            uint32_t address = read(bytes, section + 12, 4);
            uint32_t size = read(bytes, section + 16, 4);
            if (rva >= address && rva - address <= size && width <= size - (rva - address)) {
                uint64_t offset = static_cast<uint64_t>(read(bytes, section + 20, 4)) + rva - address;
                if (offset > bytes.size() || width > bytes.size() - offset) fail("export outside DLL");
                return static_cast<size_t>(offset);
            }
        }
        fail("export RVA outside file-backed sections");
    };
    size_t directory = location(exportRva, 40);
    uint32_t base = read(bytes, directory + 16, 4);
    uint32_t functions = read(bytes, directory + 20, 4);
    uint32_t names = read(bytes, directory + 24, 4);
    if (functions > bytes.size() / 4 || names > bytes.size() / 4) fail("invalid export table counts");
    size_t functionTable = location(read(bytes, directory + 28, 4), static_cast<size_t>(functions) * 4);
    size_t nameTable = location(read(bytes, directory + 32, 4), static_cast<size_t>(names) * 4);
    size_t ordinalTable = location(read(bytes, directory + 36, 4), static_cast<size_t>(names) * 2);
    std::vector<PeExport> result;
    for (uint32_t i = 0; i < names; ++i) {
        uint32_t index = read(bytes, ordinalTable + i * 2, 2);
        if (index >= functions || base > 65535 || index > 65535 - base) fail("invalid export ordinal");
        uint32_t functionRva = read(bytes, functionTable + index * 4, 4);
        if (!functionRva) continue;
        uint32_t nameRva = read(bytes, nameTable + i * 4, 4);
        std::string name;
        for (size_t n = 0; n < bytes.size(); ++n) {
            if (n > UINT32_MAX - nameRva) fail("export name overflow");
            char c = bytes[location(nameRva + static_cast<uint32_t>(n), 1)];
            if (!c) break;
            name.push_back(c);
        }
        if (name.empty()) fail("empty export name");
        bool forwarded = functionRva >= exportRva && functionRva - exportRva < exportSize;
        bool executable = forwarded;
        if (!forwarded) {
            for (size_t j = 0; j < sectionCount; ++j) {
                size_t section = sections + j * 40;
                uint32_t address = read(bytes, section + 12, 4);
                uint32_t size = read(bytes, section + 8, 4);
                if (functionRva >= address && functionRva - address < size) {
                    executable = (read(bytes, section + 36, 4) & 0x20000000) != 0;
                }
            }
        }
        result.push_back({std::move(name), static_cast<uint16_t>(base + index), !executable});
    }
    return result;
}

}
