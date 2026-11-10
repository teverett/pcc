#include "HostedPeReader.h"

#include <filesystem>
#include <map>
#include <stdexcept>

#include "fileformat/coff/PeExportReader.h"

namespace rcc {
namespace {

using Bytes = std::string;

[[noreturn]] void fail(const std::string &message) {
    throw std::runtime_error("hosted PE/COFF import reader: " + message);
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

bool isCoffImportObject(const Bytes &bytes) {
    return bytes.size() >= 20 && get16(bytes, 0) == 0 && get16(bytes, 2) == 0xffff;
}

HostedImport readCoffImportObject(const Bytes &bytes) {
    uint16_t machine = get16(bytes, 6);
    uint32_t dataSize = get32(bytes, 12);
    uint16_t typeInfo = get16(bytes, 18);
    if (machine != 0x014c || dataSize > bytes.size() - 20) fail("unsupported COFF import object machine or size");
    if ((typeInfo & 3) > 2) fail("invalid COFF import type");
    size_t names = 20;
    size_t symbolEnd = bytes.find('\0', names);
    if (symbolEnd == Bytes::npos || symbolEnd == names) fail("invalid COFF import object symbol name");
    size_t libraryStart = symbolEnd + 1;
    size_t libraryEnd = bytes.find('\0', libraryStart);
    if (libraryEnd == Bytes::npos || libraryEnd == libraryStart || libraryEnd >= 20 + dataSize) {
        fail("invalid COFF import object DLL name");
    }
    HostedImport result;
    result.symbol = bytes.substr(names, symbolEnd - names);
    result.name = result.symbol;
    result.library = bytes.substr(libraryStart, libraryEnd - libraryStart);
    result.isData = (typeInfo & 3) != 0;
    result.ordinal = get16(bytes, 16);
    uint16_t nameType = (typeInfo >> 2) & 7;
    if (nameType > 4) fail("unsupported COFF import name type");
    result.byOrdinal = nameType == 0;
    if (nameType == 2 || nameType == 3) {
        if (!result.name.empty() && (result.name[0] == '_' || result.name[0] == '@' || result.name[0] == '?')) {
            result.name.erase(0, 1);
        }
    }
    if (nameType == 3) {
        size_t at = result.name.find('@');
        if (at != std::string::npos) result.name.resize(at);
    }
    if (nameType == 4) {
        size_t exportStart = libraryEnd + 1;
        size_t exportEnd = bytes.find('\0', exportStart);
        if (exportEnd == Bytes::npos || exportEnd == exportStart || exportEnd >= 20 + dataSize) {
            fail("invalid COFF import export name");
        }
        result.name = bytes.substr(exportStart, exportEnd - exportStart);
    }
    if (result.symbol.rfind("__imp_", 0) == 0) result.symbol.erase(0, 6);
    if (!result.isData) result.aliases.push_back(result.symbol);
    result.aliases.push_back("__imp_" + result.symbol);
    return result;
}

std::vector<HostedImport> windowsDllImports(const Bytes &bytes, const std::string &path) {
    std::vector<HostedImport> imports;
    for (const auto &exported : readPeExports(bytes)) {
        HostedImport item;
        item.symbol = "_" + exported.name;
        item.name = exported.name;
        item.library = std::filesystem::path(path).filename().string();
        item.isData = exported.isData;
        item.aliases.push_back("__imp_" + item.symbol);
        if (!item.isData) item.aliases.push_back(item.symbol);
        imports.push_back(std::move(item));
    }
    return imports;
}

std::unordered_map<size_t, HostedImport> gnuCoffImports(const std::vector<ArchiveMember> &members) {
    std::string library;
    std::unordered_map<size_t, HostedImport> imports;
    for (size_t index = 0; index < members.size(); ++index) {
        const Bytes &bytes = members[index].bytes;
        if (isCoffImportObject(bytes)) continue;
        if (bytes.size() < 20 || get16(bytes, 0) != 0x14c) continue;
        uint16_t count = get16(bytes, 2);
        if (get16(bytes, 16) || count > (bytes.size() - 20) / 40) fail("invalid GNU COFF section table");
        std::map<std::string, Bytes> sections;
        for (size_t n = 0; n < count; ++n) {
            size_t header = 20 + n * 40;
            std::string name = bytes.substr(header, 8);
            name.resize(name.find('\0') == std::string::npos ? name.size() : name.find('\0'));
            if (name.rfind(".idata$", 0) != 0) continue;
            size_t size = get32(bytes, header + 16), offset = get32(bytes, header + 20);
            if (offset > bytes.size() || size > bytes.size() - offset) fail("invalid GNU import section");
            sections[name] = bytes.substr(offset, size);
        }
        if (sections.empty()) continue;
        auto dll = sections.find(".idata$7");
        if (dll != sections.end() && dll->second.size() > 4) {
            size_t end = dll->second.find('\0');
            if (end == Bytes::npos) fail("unterminated GNU import DLL name");
            std::string candidate = dll->second.substr(0, end);
            if (candidate.size() >= 4 && candidate.substr(candidate.size() - 4) == ".dll") {
                if (!library.empty() && library != candidate) fail("GNU import archive contains multiple DLL providers");
                library = candidate;
            }
        }
        auto hintName = sections.find(".idata$6");
        auto iat = sections.find(".idata$5");
        if (iat == sections.end()) continue;
        if (iat->second.empty() && sections.count(".idata$2")) continue;
        if (iat->second.size() != 4) fail("unsupported GNU import address table");
        HostedImport item;
        uint32_t value = get32(iat->second, 0);
        if (!(value & 0x80000000u) && hintName == sections.end() &&
            dll != sections.end() && dll->second.size() > 4) continue;
        if (value & 0x80000000u) {
            if (value & 0x7fff0000u) fail("invalid GNU import ordinal");
            item.byOrdinal = true;
            item.ordinal = static_cast<uint16_t>(value);
        } else {
            if (hintName == sections.end() || hintName->second.size() < 3) fail("GNU import has no hint/name record");
            size_t end = hintName->second.find('\0', 2);
            if (end == Bytes::npos || end == 2) fail("invalid GNU import name");
            item.name = hintName->second.substr(2, end - 2);
        }
        size_t offset = get32(bytes, 8), symbols = get32(bytes, 12);
        if (offset > bytes.size() || symbols > (bytes.size() - offset) / 18) fail("invalid GNU import symbol table");
        size_t strings = offset + symbols * 18;
        uint32_t stringSize = get32(bytes, strings);
        if (stringSize < 4 || stringSize > bytes.size() - strings) fail("invalid GNU import string table");
        item.isData = true;
        for (size_t n = 0; n < symbols;) {
            size_t at = offset + n * 18;
            uint8_t auxiliary = static_cast<uint8_t>(bytes[at + 17]);
            if (auxiliary > symbols - n - 1) fail("invalid GNU import auxiliary symbols");
            if (static_cast<uint8_t>(bytes[at + 16]) == 2 && get16(bytes, at + 12) > 0) {
                std::string name;
                if (get32(bytes, at) == 0) {
                    uint32_t position = get32(bytes, at + 4);
                    if (position < 4 || position >= stringSize) fail("invalid GNU import symbol name");
                    size_t end = bytes.find('\0', strings + position);
                    if (end == Bytes::npos || end >= strings + stringSize) fail("unterminated GNU import symbol name");
                    name = bytes.substr(strings + position, end - strings - position);
                } else {
                    name = bytes.substr(at, 8);
                    size_t end = name.find('\0');
                    if (end != Bytes::npos) name.resize(end);
                }
                item.aliases.push_back(name);
                if (name.rfind("__imp_", 0) != 0) {
                    item.symbol = name;
                    item.isData = false;
                } else if (item.symbol.empty()) item.symbol = name.substr(6);
            }
            n += auxiliary + 1;
        }
        if (item.aliases.empty()) fail("GNU import object has no exported symbols");
        imports.emplace(index, std::move(item));
    }
    if (!imports.empty() && library.empty()) fail("GNU import archive has no DLL provider");
    for (auto &entry : imports) entry.second.library = library;
    return imports;
}

}  // namespace rcc
