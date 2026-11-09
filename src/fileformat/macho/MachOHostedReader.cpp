#include "MachOHostedReader.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "codegen/arm64/Arm64Assembler.h"

namespace pcc {
namespace {

using Bytes = std::string;

// This reader's own relocation-type codes for a GOT-relative load (arm64 Mach-O relocation
// kinds 5/6) - mirrors fileformat/macho/MachOExecutableWriter.h's identical constants, which
// the writer needs for the exact same two kinds; duplicated rather than shared across a
// reader->writer include to keep this reader independent of how the image gets written.
constexpr uint16_t kGotPage21 = 7;
constexpr uint16_t kGotPageoff12 = 8;

[[noreturn]] void fail(const std::string &message) {
    throw std::runtime_error("Mach-O reader: " + message);
}

uint8_t byteAt(const Bytes &data, size_t offset) {
    if (offset >= data.size()) fail("truncated input file");
    return static_cast<uint8_t>(data[offset]);
}

uint16_t get16(const Bytes &data, size_t offset) {
    return static_cast<uint16_t>(byteAt(data, offset)) |
           (static_cast<uint16_t>(byteAt(data, offset + 1)) << 8);
}

uint32_t get32(const Bytes &data, size_t offset) {
    return static_cast<uint32_t>(get16(data, offset)) |
           (static_cast<uint32_t>(get16(data, offset + 2)) << 16);
}

uint64_t get64(const Bytes &data, size_t offset) {
    return static_cast<uint64_t>(get32(data, offset)) |
           (static_cast<uint64_t>(get32(data, offset + 4)) << 32);
}

Bytes readFile(const std::string &path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) fail("cannot read input '" + path + "'");
    std::ostringstream stream;
    stream << input.rdbuf();
    if (!input.good() && !input.eof()) fail("failed reading input '" + path + "'");
    return stream.str();
}

std::string trim(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.erase(value.begin());
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.pop_back();
    return value;
}

std::string unquote(std::string value) {
    value = trim(std::move(value));
    if (value.size() >= 2 && ((value.front() == '\'' && value.back() == '\'') ||
                              (value.front() == '"' && value.back() == '"'))) {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

std::vector<std::string> yamlList(std::string input) {
    std::vector<std::string> result;
    bool quoted = false;
    char quote = '\0';
    std::string item;
    for (char c : input) {
        if (quoted) {
            if (c == quote) quoted = false;
            else item.push_back(c);
        } else if (c == '\'' || c == '"') {
            quoted = true;
            quote = c;
        } else if (c == '[' || c == ']' || c == ',') {
            if (c == ',' || c == ']') {
                std::string token = trim(item);
                if (!token.empty()) result.push_back(token);
                item.clear();
            }
        } else {
            item.push_back(c);
        }
    }
    std::string token = trim(item);
    if (!token.empty()) result.push_back(token);
    return result;
}

bool supportedTarget(const std::string &target) {
    return target == "arm64-macos" || target == "arm64e-macos" || target == "arm64e.x1-macos";
}

std::vector<std::string> readYamlFieldList(const std::vector<std::string> &lines, size_t &lineIndex,
                                           const std::string &initialValue) {
    std::string content = initialValue;
    while (content.find(']') == std::string::npos && lineIndex + 1 < lines.size()) {
        content.push_back(' ');
        content += trim(lines[++lineIndex]);
    }
    if (content.find(']') == std::string::npos) fail("unterminated target library list in text-based stub");
    return yamlList(content);
}

uint64_t readUleb(const Bytes &bytes, size_t &offset, size_t limit) {
    uint64_t value = 0;
    unsigned shift = 0;
    while (offset < limit) {
        uint8_t byte = byteAt(bytes, offset++);
        if (shift >= 64 || ((static_cast<uint64_t>(byte & 0x7f) << shift) >> shift) != (byte & 0x7f)) {
            fail("malformed Mach-O export trie ULEB128");
        }
        value |= static_cast<uint64_t>(byte & 0x7f) << shift;
        if ((byte & 0x80) == 0) return value;
        shift += 7;
    }
    fail("truncated Mach-O export trie ULEB128");
}

void collectExportTrieNode(const Bytes &bytes, uint32_t trieOffset, uint32_t trieSize,
                           uint64_t nodeOffset, const std::string &prefix,
                           std::unordered_set<std::string> &exports,
                           std::unordered_set<uint64_t> &visited) {
    if (nodeOffset >= trieSize || !visited.insert(nodeOffset).second) {
        if (nodeOffset >= trieSize) fail("Mach-O export trie child is outside its data range");
        return;
    }
    size_t cursor = static_cast<size_t>(trieOffset) + nodeOffset;
    size_t end = static_cast<size_t>(trieOffset) + trieSize;
    uint64_t terminalSize = readUleb(bytes, cursor, end);
    if (terminalSize > end - cursor) fail("Mach-O export trie terminal exceeds its data range");
    size_t terminalEnd = cursor + static_cast<size_t>(terminalSize);
    if (terminalSize != 0) {
        uint64_t flags = readUleb(bytes, cursor, terminalEnd);
        constexpr uint64_t kReexportFlag = 0x08;
        if ((flags & kReexportFlag) == 0 && !prefix.empty()) exports.insert(prefix);
        cursor = terminalEnd;
    }
    if (cursor >= end) fail("truncated Mach-O export trie node");
    uint8_t childCount = byteAt(bytes, cursor++);
    for (uint8_t child = 0; child < childCount; ++child) {
        size_t edgeEnd = bytes.find('\0', cursor);
        if (edgeEnd == Bytes::npos || edgeEnd >= end) fail("unterminated Mach-O export trie edge");
        std::string edge = bytes.substr(cursor, edgeEnd - cursor);
        cursor = edgeEnd + 1;
        uint64_t childOffset = readUleb(bytes, cursor, end);
        collectExportTrieNode(bytes, trieOffset, trieSize, childOffset,
                              prefix + edge, exports, visited);
    }
}

std::string cstringAt(const Bytes &bytes, size_t offset, size_t limit) {
    if (offset >= bytes.size() || limit > bytes.size() || offset >= limit) fail("invalid Mach-O string offset");
    size_t end = bytes.find('\0', offset);
    if (end == Bytes::npos || end >= limit) fail("unterminated Mach-O string");
    return bytes.substr(offset, end - offset);
}

}  // namespace

StubLibrary parseTbd(const Bytes &bytes, const std::string &label) {
    std::vector<std::string> lines;
    std::istringstream input(bytes);
    std::string line;
    while (std::getline(input, line)) lines.push_back(line);

    StubLibrary result;
    bool exportsSection = false;
    bool reexportsSection = false;
    bool blockSupported = false;
    bool blockHasTargets = false;
    bool blockExports = false;
    bool blockReexports = false;
    for (size_t i = 0; i < lines.size(); ++i) {
        std::string current = trim(lines[i]);
        if (i != 0 && current.rfind("--- !tapi-tbd", 0) == 0) break;
        if (current.empty() || current[0] == '#') continue;
        if (current.rfind("install-name:", 0) == 0) {
            result.installName = unquote(current.substr(std::string("install-name:").size()));
            continue;
        }
        if (current.rfind("archs:", 0) == 0) {
            result.hasTargetMetadata = true;
            auto architectures = readYamlFieldList(lines, i, current.substr(std::string("archs:").size()));
            if (std::find(architectures.begin(), architectures.end(), "arm64") != architectures.end()) {
                result.supportsArm64MacOS = true;
            }
            continue;
        }
        if (current == "exports:") {
            exportsSection = true;
            reexportsSection = false;
            blockHasTargets = false;
            blockExports = false;
            continue;
        }
        if (current == "reexported-libraries:") {
            reexportsSection = true;
            exportsSection = false;
            blockHasTargets = false;
            blockReexports = false;
            continue;
        }
        if (!current.empty() && current[0] != '-' &&
            (current.find(':') != std::string::npos) &&
            (lines[i].find_first_not_of(" \t") == 0)) {
            exportsSection = false;
            reexportsSection = false;
        }
        if (!exportsSection && !reexportsSection) continue;

        if (current.rfind("- targets:", 0) == 0) {
            result.hasTargetMetadata = true;
            blockSupported = false;
            blockHasTargets = true;
            blockExports = false;
            blockReexports = false;
            auto targets = readYamlFieldList(lines, i, current.substr(std::string("- targets:").size()));
            blockSupported = std::any_of(targets.begin(), targets.end(), supportedTarget);
            result.supportsArm64MacOS = result.supportsArm64MacOS || blockSupported;
            continue;
        }
        if (current.rfind("targets:", 0) == 0) {
            result.hasTargetMetadata = true;
            auto targets = readYamlFieldList(lines, i, current.substr(std::string("targets:").size()));
            blockSupported = std::any_of(targets.begin(), targets.end(), supportedTarget);
            result.supportsArm64MacOS = result.supportsArm64MacOS || blockSupported;
            blockHasTargets = true;
            continue;
        }
        if (exportsSection && (current.rfind("symbols:", 0) == 0 ||
                               current.rfind("weak-symbols:", 0) == 0)) {
            blockExports = true;
            blockReexports = false;
            auto values = readYamlFieldList(lines, i, current.substr(current.find(':') + 1));
            if (blockSupported) {
                for (auto &symbol : values) result.exports.insert(unquote(std::move(symbol)));
            }
            continue;
        }
        if (reexportsSection && current.rfind("libraries:", 0) == 0) {
            blockReexports = true;
            blockExports = false;
            auto values = readYamlFieldList(lines, i, current.substr(std::string("libraries:").size()));
            if (blockSupported) {
                for (auto &library : values) result.reexports.push_back(unquote(std::move(library)));
            }
            continue;
        }
        if (!blockHasTargets && current.rfind("symbols:", 0) == 0) {
            blockSupported = true;
        }
        if (blockExports || blockReexports) continue;
    }
    if (result.installName.empty()) fail("text-based stub has no install name: '" + label + "'");
    return result;
}

std::filesystem::path findSdkRoot() {
    if (const char *sdkRoot = std::getenv("SDKROOT")) {
        std::filesystem::path candidate(sdkRoot);
        if (std::filesystem::is_directory(candidate / "usr/lib")) return candidate;
    }
    const std::vector<std::filesystem::path> candidates = {
        "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk",
        "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk",
        "/Applications/Xcode-beta.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk",
    };
    for (const auto &candidate : candidates) {
        if (std::filesystem::is_directory(candidate / "usr/lib")) return candidate;
    }
    return {};
}

std::filesystem::path stubPathForInstallName(const std::filesystem::path &sdkRoot,
                                             const std::string &installName) {
    if (sdkRoot.empty()) return {};
    std::filesystem::path relative = installName;
    while (!relative.empty() && relative.is_absolute()) relative = relative.relative_path();
    std::filesystem::path direct = sdkRoot / relative;
    std::filesystem::path stub = direct;
    stub.replace_extension(".tbd");
    if (std::filesystem::is_regular_file(stub)) return stub;
    std::filesystem::path system = sdkRoot / "usr/lib/system" / relative.filename();
    system.replace_extension(".tbd");
    if (std::filesystem::is_regular_file(system)) return system;
    return {};
}

void collectTbdExports(const std::filesystem::path &path, const std::filesystem::path &sdkRoot,
                       std::unordered_set<std::string> &exports,
                       std::unordered_set<std::string> &seen) {
    std::error_code error;
    auto canonical = std::filesystem::weakly_canonical(path, error);
    const std::string key = error ? path.string() : canonical.string();
    if (!seen.insert(key).second) return;
    StubLibrary parsed = parseTbd(readFile(path.string()), path.string());
    if (parsed.hasTargetMetadata && !parsed.supportsArm64MacOS) return;
    exports.insert(parsed.exports.begin(), parsed.exports.end());
    for (const auto &reexport : parsed.reexports) {
        auto child = stubPathForInstallName(sdkRoot, reexport);
        if (!child.empty()) collectTbdExports(child, sdkRoot, exports, seen);
    }
}

MachObjectInfo parseMachO(const Bytes &bytes, const std::string &label, bool requireObject) {
    if (bytes.size() < 32 || get32(bytes, 0) != 0xfeedfacf) {
        fail("unsupported Mach-O file (expected thin 64-bit arm64): '" + label + "'");
    }
    MachObjectInfo result;
    result.cpuType = get32(bytes, 4);
    result.cpuSubtype = get32(bytes, 8) & 0x00ffffff;
    result.fileType = get32(bytes, 12);
    if (result.cpuType != 0x0100000c || (result.cpuSubtype != 0 && result.cpuSubtype != 2)) {
        fail("Mach-O input is not arm64: '" + label + "'");
    }
    if (requireObject && result.fileType != 1) fail("expected an arm64 MH_OBJECT input: '" + label + "'");
    if (!requireObject && result.fileType != 6) fail("expected an arm64 MH_DYLIB input: '" + label + "'");

    constexpr uint32_t kLcSegment64 = 0x19;
    constexpr uint32_t kLcSymtab = 0x2;
    constexpr uint32_t kLcIdDylib = 0xd;
    constexpr uint32_t kLcDyldInfo = 0x22;
    constexpr uint32_t kLcDyldExportsTrie = 0x80000033;
    const uint32_t commandCount = get32(bytes, 16);
    size_t commandOffset = 32;
    uint64_t textOffset = 0, textSize = 0, dataOffset = 0, dataSize = 0;
    uint64_t dataSectionAddress = 0, bssSize = 0, bssSectionAddress = 0;
    uint64_t relocationOffset = 0, relocationCount = 0;
    uint32_t dataSectionIndex = 0, bssSectionIndex = 0;
    uint32_t symbolOffset = 0, symbolCount = 0, stringOffset = 0, stringSize = 0;
    uint32_t sectionOrdinal = 0;
    for (uint32_t commandIndex = 0; commandIndex < commandCount; ++commandIndex) {
        uint32_t command = get32(bytes, commandOffset);
        uint32_t size = get32(bytes, commandOffset + 4);
        if (size < 8 || commandOffset > bytes.size() || bytes.size() - commandOffset < size) {
            fail("invalid Mach-O load command in '" + label + "'");
        }
        if (command == kLcSegment64) {
            uint32_t sectionCount = get32(bytes, commandOffset + 64);
            size_t sectionOffset = commandOffset + 72;
            for (uint32_t sectionIndex = 0; sectionIndex < sectionCount; ++sectionIndex, sectionOffset += 80) {
                if (sectionOffset + 80 > commandOffset + size) fail("truncated Mach-O section in '" + label + "'");
                ++sectionOrdinal;
                std::string sectionName = cstringAt(bytes, sectionOffset, sectionOffset + 16);
                uint64_t sectionSize = get64(bytes, sectionOffset + 40);
                if (sectionName == "__text") {
                    textSize = sectionSize;
                    textOffset = get32(bytes, sectionOffset + 48);
                    relocationOffset = get32(bytes, sectionOffset + 56);
                    relocationCount = get32(bytes, sectionOffset + 60);
                } else if (sectionName == "__data") {
                    dataSize = sectionSize;
                    dataOffset = get32(bytes, sectionOffset + 48);
                    dataSectionAddress = get64(bytes, sectionOffset + 32);
                    dataSectionIndex = sectionOrdinal;
                } else if (sectionName == "__bss") {
                    if ((get32(bytes, sectionOffset + 64) & 0xff) != 1) {
                        fail("Mach-O __bss section is not zero-fill in '" + label + "'");
                    }
                    bssSize = sectionSize;
                    bssSectionAddress = get64(bytes, sectionOffset + 32);
                    bssSectionIndex = sectionOrdinal;
                } else if (requireObject && sectionSize != 0) {
                    fail("unsupported non-empty Mach-O section '" + sectionName + "' in '" + label + "'");
                }
            }
        } else if (command == kLcSymtab) {
            symbolOffset = get32(bytes, commandOffset + 8);
            symbolCount = get32(bytes, commandOffset + 12);
            stringOffset = get32(bytes, commandOffset + 16);
            stringSize = get32(bytes, commandOffset + 20);
        } else if (command == kLcIdDylib && !requireObject) {
            uint32_t nameOffset = get32(bytes, commandOffset + 8);
            result.installName = cstringAt(bytes, commandOffset + nameOffset, commandOffset + size);
        } else if (!requireObject && command == kLcDyldInfo) {
            result.exportTrieOffset = get32(bytes, commandOffset + 40);
            result.exportTrieSize = get32(bytes, commandOffset + 44);
        } else if (!requireObject && command == kLcDyldExportsTrie) {
            result.exportTrieOffset = get32(bytes, commandOffset + 8);
            result.exportTrieSize = get32(bytes, commandOffset + 12);
        }
        commandOffset += size;
    }
    if (requireObject) {
        if (textOffset > bytes.size() || textSize > bytes.size() - textOffset ||
            dataOffset > bytes.size() || dataSize > bytes.size() - dataOffset ||
            symbolOffset > bytes.size() || stringOffset > bytes.size() ||
            stringSize > bytes.size() - stringOffset ||
            symbolCount > (bytes.size() - symbolOffset) / 16 ||
            relocationOffset > bytes.size() ||
            relocationCount > (bytes.size() - relocationOffset) / 8) {
            fail("Mach-O section or table exceeds file bounds in '" + label + "'");
        }
        if (bssSize > UINT32_MAX) fail("Mach-O BSS exceeds format limits in '" + label + "'");
        result.object.machineType = kArm64DarwinMachine;
        result.object.dataSize = static_cast<uint32_t>(bssSize);
        result.object.dataBytes.assign(bytes.begin() + static_cast<std::ptrdiff_t>(dataOffset),
                                       bytes.begin() + static_cast<std::ptrdiff_t>(dataOffset + dataSize));
        result.object.textBytes.assign(bytes.begin() + static_cast<std::ptrdiff_t>(textOffset),
                                       bytes.begin() + static_cast<std::ptrdiff_t>(textOffset + textSize));
        std::vector<std::string> symbolNames(symbolCount);
        for (uint32_t i = 0; i < symbolCount; ++i) {
            size_t position = static_cast<size_t>(symbolOffset) + i * 16;
            uint32_t name = get32(bytes, position);
            uint8_t type = byteAt(bytes, position + 4);
            uint8_t section = byteAt(bytes, position + 5);
            uint64_t value = get64(bytes, position + 8);
            if (name >= stringSize) fail("Mach-O symbol name offset outside string table in '" + label + "'");
            bool defined = (type & 0x0e) == 0x0e;
            symbolNames[i] = cstringAt(bytes, stringOffset + name, stringOffset + stringSize);
            if (defined && (type & 0x01) == 0) {
                symbolNames[i] = "__pcc_local_" + std::to_string(i) + "_" + symbolNames[i];
            }
            if (defined && bssSectionIndex != 0 && section == bssSectionIndex) {
                if (value < bssSectionAddress || value - bssSectionAddress > bssSize) {
                    fail("Mach-O BSS symbol lies outside its section in '" + label + "'");
                }
                value -= bssSectionAddress;
            } else if (defined && dataSectionIndex != 0 && section == dataSectionIndex) {
                if (value < dataSectionAddress || value - dataSectionAddress > dataSize) {
                    fail("Mach-O data symbol lies outside its section in '" + label + "'");
                }
                value -= dataSectionAddress;
            }
            result.object.symbols.push_back(
                {symbolNames[i], defined ? static_cast<uint32_t>(value) : 0, defined,
                 defined && (section == dataSectionIndex || section == bssSectionIndex),
                 defined && bssSectionIndex != 0 && section == bssSectionIndex});
        }
        for (uint32_t i = 0; i < relocationCount; ++i) {
            size_t position = static_cast<size_t>(relocationOffset) + i * 8;
            uint32_t address = get32(bytes, position);
            uint32_t info = get32(bytes, position + 4);
            uint32_t symbolIndex = info & 0x00ffffff;
            uint32_t kind = info >> 28;
            if (static_cast<int32_t>(address) < 0 || symbolIndex >= symbolNames.size() ||
                ((info >> 27) & 1) == 0) {
                fail("unsupported local or malformed arm64 relocation in '" + label + "'");
            }
            uint16_t type = 0;
            if (kind == 2) type = kArm64DarwinRelocBranch26;
            else if (kind == 3) type = kArm64DarwinRelocPage21;
            else if (kind == 4) type = kArm64DarwinRelocPageoff12;
            else if (kind == 5) type = kGotPage21;
            else if (kind == 6) type = kGotPageoff12;
            else fail("unsupported arm64 Mach-O relocation " + std::to_string(kind) + " in '" + label + "'");
            result.object.relocations.push_back(
                {address, symbolNames[symbolIndex], type});
        }
    } else {
        if (result.installName.empty()) result.installName = label;
        if (symbolOffset > bytes.size() || stringOffset > bytes.size() ||
            stringSize > bytes.size() - stringOffset || symbolCount > (bytes.size() - symbolOffset) / 16) {
            fail("invalid Mach-O dylib symbol table in '" + label + "'");
        }
        for (uint32_t i = 0; i < symbolCount; ++i) {
            size_t position = static_cast<size_t>(symbolOffset) + i * 16;
            uint32_t name = get32(bytes, position);
            uint8_t type = byteAt(bytes, position + 4);
            if (name >= stringSize || (type & 0xe) != 0xe || (type & 0x01) == 0) continue;
            result.exportedSymbols.insert(cstringAt(bytes, stringOffset + name, stringOffset + stringSize));
        }
        if (result.exportTrieSize != 0) {
            if (result.exportTrieOffset > bytes.size() ||
                result.exportTrieSize > bytes.size() - result.exportTrieOffset) {
                fail("Mach-O export trie exceeds file bounds in '" + label + "'");
            }
            std::unordered_set<uint64_t> visited;
            collectExportTrieNode(bytes, result.exportTrieOffset, result.exportTrieSize,
                                  0, "", result.exportedSymbols, visited);
        }
    }
    return result;
}

}  // namespace pcc
