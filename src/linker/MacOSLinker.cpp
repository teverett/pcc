#include "MacOSLinker.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "codegen/arm64/Arm64Assembler.h"
#include "fileformat/archive/ArchiveReader.h"
#include "fileformat/macho/MachOExecutableWriter.h"
#include "fileformat/macho/MachOHostedReader.h"
#include "fileformat/ObjectFile.h"

namespace rcc {
namespace {

using Bytes = std::string;

[[noreturn]] void fail(const std::string &message) {
    throw std::runtime_error("native macOS linker: " + message);
}

struct Unit {
    ObjectFile object;
    std::string label;
};

bool isArchive(const Bytes &bytes) {
    return bytes.rfind("!<arch>\n", 0) == 0;
}

Library loadLibrary(const std::filesystem::path &path, const std::filesystem::path &sdkRoot,
                    const std::filesystem::path &searchDirectory) {
    std::ifstream input(path, std::ios::binary);
    if (!input) fail("cannot read input '" + path.string() + "'");
    std::ostringstream stream;
    stream << input.rdbuf();
    Bytes bytes = stream.str();
    Library library;
    library.filePath = path;
    library.searchDirectory = searchDirectory;
    if (path.extension() == ".tbd" || bytes.rfind("--- !tapi-tbd", 0) == 0) {
        StubLibrary stub = parseTbd(bytes, path.string());
        if (stub.hasTargetMetadata && !stub.supportsArm64MacOS) {
            fail("text-based stub has no arm64 macOS exports: '" + path.string() + "'");
        }
        library.installName = stub.installName;
        library.exports = std::move(stub.exports);
        std::unordered_set<std::string> seen;
        seen.insert(std::filesystem::weakly_canonical(path).string());
        for (const auto &reexport : stub.reexports) {
            auto child = stubPathForInstallName(sdkRoot, reexport);
            if (!child.empty()) collectTbdExports(child, sdkRoot, library.exports, seen);
        }
    } else {
        MachObjectInfo dylib = parseMachO(bytes, path.string(), false);
        library.installName = std::move(dylib.installName);
        library.exports = std::move(dylib.exportedSymbols);
    }
    return library;
}

std::vector<std::filesystem::path> libraryCandidates(const std::string &name,
                                                      const std::vector<std::filesystem::path> &searchPaths,
                                                      const std::filesystem::path &sdkRoot) {
    std::vector<std::filesystem::path> candidates;
    for (const auto &directory : searchPaths) {
        if (!name.empty() && name[0] == ':') {
            candidates.push_back(directory / name.substr(1));
        } else {
            candidates.push_back(directory / ("lib" + name + ".tbd"));
            candidates.push_back(directory / ("lib" + name + ".dylib"));
            candidates.push_back(directory / ("lib" + name + ".a"));
        }
    }
    if (!sdkRoot.empty() && !name.empty() && name[0] != ':') {
        candidates.push_back(sdkRoot / "usr/lib" / ("lib" + name + ".tbd"));
        candidates.push_back(sdkRoot / "usr/lib/system" / ("lib" + name + ".tbd"));
        candidates.push_back(sdkRoot / "usr/lib" / ("lib" + name + ".dylib"));
        candidates.push_back(sdkRoot / "usr/lib/system" / ("lib" + name + ".dylib"));
    }
    return candidates;
}

Bytes readFile(const std::string &path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) fail("cannot read input '" + path + "'");
    std::ostringstream stream;
    stream << input.rdbuf();
    if (!input.good() && !input.eof()) fail("failed reading input '" + path + "'");
    return stream.str();
}

struct Inputs {
    std::vector<Unit> units;
    std::vector<Bytes> archives;
    std::vector<Library> libraries;
    std::vector<std::filesystem::path> rpaths;
};

Inputs loadInputs(const std::vector<std::string> &arguments, const std::filesystem::path &sdkRoot) {
    Inputs result;
    std::vector<std::filesystem::path> searchPaths{"."};
    for (size_t i = 0; i < arguments.size(); ++i) {
        const std::string &argument = arguments[i];
        if (argument == "-L") {
            if (++i == arguments.size()) fail("-L requires a directory");
            searchPaths.emplace_back(arguments[i]);
            continue;
        }
        if (argument.rfind("-L", 0) == 0) {
            searchPaths.emplace_back(argument.substr(2));
            continue;
        }
        if (argument == "-l" || argument.rfind("-l", 0) == 0) {
            std::string name;
            if (argument == "-l") {
                if (++i == arguments.size()) fail("-l requires a library name");
                name = arguments[i];
            } else {
                name = argument.substr(2);
            }
            if (name.empty()) fail("empty library name");
            std::filesystem::path found;
            std::filesystem::path foundDirectory;
            for (const auto &candidate : libraryCandidates(name, searchPaths, sdkRoot)) {
                std::error_code error;
                if (std::filesystem::is_regular_file(candidate, error)) {
                    found = candidate;
                    foundDirectory = candidate.parent_path();
                    break;
                }
            }
            if (found.empty()) fail("library -l" + name + " was not found in the supplied search paths or SDK");
            Bytes data = readFile(found.string());
            if (isArchive(data)) {
                result.archives.push_back(std::move(data));
            } else {
                Library library = loadLibrary(found, sdkRoot, foundDirectory);
                if (!library.installName.empty() && library.installName[0] == '@' &&
                    !foundDirectory.empty()) {
                    result.rpaths.push_back(foundDirectory);
                }
                result.libraries.push_back(std::move(library));
            }
            continue;
        }
        std::string filename = std::filesystem::path(argument).filename().string();
        if (filename.find(".dylib") != std::string::npos || filename.find(".tbd") != std::string::npos) {
            Library library = loadLibrary(argument, sdkRoot, std::filesystem::path(argument).parent_path());
            if (!library.installName.empty() && library.installName[0] == '@') {
                result.rpaths.push_back(std::filesystem::path(argument).parent_path());
            }
            result.libraries.push_back(std::move(library));
            continue;
        }
        Bytes data = readFile(argument);
        if (isArchive(data)) {
            result.archives.push_back(std::move(data));
        } else {
            result.units.push_back({parseMachO(data, argument, true).object, argument});
        }
    }
    return result;
}

std::unordered_set<std::string> unresolved(const std::vector<Unit> &units) {
    std::unordered_set<std::string> definitions;
    std::unordered_set<std::string> needs;
    for (const auto &unit : units) {
        for (const auto &symbol : unit.object.symbols) {
            if (symbol.isDefined) definitions.insert(symbol.name);
            else needs.insert(symbol.name);
        }
    }
    for (const auto &name : definitions) needs.erase(name);
    return needs;
}

void extractArchiveMembers(Inputs &inputs) {
    for (const auto &archive : inputs.archives) {
        std::vector<ArchiveMember> members = readArchive(archive);
        std::vector<bool> included(members.size(), false);
        bool changed;
        do {
            changed = false;
            auto needed = unresolved(inputs.units);
            for (size_t i = 0; i < members.size(); ++i) {
                if (included[i]) continue;
                ObjectFile object;
                try {
                    object = parseMachO(members[i].bytes, members[i].name, true).object;
                } catch (const std::exception &) {
                    continue;
                }
                bool selected = std::any_of(object.symbols.begin(), object.symbols.end(),
                    [&](const ObjSymbol &symbol) { return symbol.isDefined && needed.count(symbol.name) != 0; });
                if (!selected) continue;
                inputs.units.push_back({std::move(object), members[i].name});
                included[i] = true;
                changed = true;
            }
        } while (changed);
    }
}

MachOImage mergeObjects(Inputs inputs) {
    extractArchiveMembers(inputs);
    auto needs = unresolved(inputs.units);
    std::map<std::string, uint32_t> importLibraries;
    for (const auto &symbol : needs) {
        bool found = false;
        for (uint32_t index = 0; index < inputs.libraries.size(); ++index) {
            if (inputs.libraries[index].exports.count(symbol)) {
                importLibraries[symbol] = index;
                found = true;
                break;
            }
        }
        if (!found) fail("unresolved symbol '" + symbol + "' (no selected arm64 macOS library exports it)");
    }
    bool hasSystemExit = false;
    for (uint32_t index = 0; index < inputs.libraries.size(); ++index) {
        if (inputs.libraries[index].installName == "/usr/lib/libSystem.B.dylib" &&
            inputs.libraries[index].exports.count("_exit")) {
            hasSystemExit = true;
            if (!importLibraries.count("_exit")) importLibraries["_exit"] = index;
            break;
        }
    }

    std::vector<uint32_t> unitBases(inputs.units.size());
    std::vector<uint32_t> dataBases(inputs.units.size());
    std::vector<uint32_t> bssBases(inputs.units.size());
    uint32_t startupSize = hasSystemExit ? 20 : 16;
    uint32_t textSize = startupSize;
    uint32_t initializedDataSize = 0;
    uint32_t bssSize = 0;
    auto alignTo = [](uint32_t value, uint32_t alignment) {
        return (value + alignment - 1) & ~(alignment - 1);
    };
    for (size_t i = 0; i < inputs.units.size(); ++i) {
        textSize = alignTo(textSize, 4);
        unitBases[i] = textSize;
        if (inputs.units[i].object.textBytes.size() > UINT32_MAX - textSize) fail("program text exceeds format limits");
        textSize += static_cast<uint32_t>(inputs.units[i].object.textBytes.size());
        dataBases[i] = alignTo(initializedDataSize, 8);
        if (inputs.units[i].object.dataBytes.size() > UINT32_MAX - dataBases[i]) {
            fail("program initialized data exceeds format limits");
        }
        initializedDataSize = dataBases[i] + static_cast<uint32_t>(inputs.units[i].object.dataBytes.size());
        bssBases[i] = alignTo(bssSize, 16);
        if (inputs.units[i].object.dataSize > UINT32_MAX - bssBases[i]) fail("program BSS exceeds format limits");
        bssSize = bssBases[i] + inputs.units[i].object.dataSize;
    }

    auto put32 = [](Bytes &data, size_t offset, uint32_t value) {
        data[offset] = static_cast<char>(value);
        data[offset + 1] = static_cast<char>(value >> 8);
        data[offset + 2] = static_cast<char>(value >> 16);
        data[offset + 3] = static_cast<char>(value >> 24);
    };
    auto get32 = [](const Bytes &data, size_t offset) {
        return static_cast<uint32_t>(static_cast<uint8_t>(data[offset])) |
               (static_cast<uint32_t>(static_cast<uint8_t>(data[offset + 1])) << 8) |
               (static_cast<uint32_t>(static_cast<uint8_t>(data[offset + 2])) << 16) |
               (static_cast<uint32_t>(static_cast<uint8_t>(data[offset + 3])) << 24);
    };

    MachOImage image;
    image.code.assign(textSize, '\0');
    image.data.assign(initializedDataSize, '\0');
    image.bssSize = bssSize;
    image.libraries = std::move(inputs.libraries);
    image.rpaths = std::move(inputs.rpaths);
    image.code[0] = static_cast<char>(0x00);
    image.code[1] = static_cast<char>(0x00);
    image.code[2] = static_cast<char>(0x00);
    image.code[3] = static_cast<char>(0x94);
    if (hasSystemExit) {
        put32(image.code, 4, 0x94000000u);
        put32(image.code, 8, 0xd2800030u);
        put32(image.code, 12, 0xf2a04010u);
        put32(image.code, 16, 0xd4001001u);
    } else {
        put32(image.code, 4, 0xd2800030u);
        put32(image.code, 8, 0xf2a04010u);
        put32(image.code, 12, 0xd4001001u);
    }
    image.entryOffset = 0;

    std::unordered_map<std::string, uint64_t> symbols;
    std::unordered_map<std::string, size_t> symbolOwners;
    std::vector<std::unordered_map<std::string, std::string>> renamed(inputs.units.size());
    for (size_t i = 0; i < inputs.units.size(); ++i) {
        const auto &object = inputs.units[i].object;
        std::copy(object.textBytes.begin(), object.textBytes.end(),
                  image.code.begin() + static_cast<std::ptrdiff_t>(unitBases[i]));
        std::copy(object.dataBytes.begin(), object.dataBytes.end(),
                  image.data.begin() + static_cast<std::ptrdiff_t>(dataBases[i]));
        for (const auto &symbol : object.symbols) {
            if (!symbol.isDefined || symbol.name == ".." || symbol.name == "..d") continue;
            std::string name = symbol.name;
            if (name.rfind("L", 0) == 0 || name.rfind("__rcc_local_", 0) == 0 ||
                name.rfind("__rcc_tmp_", 0) == 0) {
                std::string original = name;
                name += "$rcc" + std::to_string(i);
                renamed[i][original] = name;
            }
            uint64_t value = unitBases[i] + symbol.value;
            if (symbol.isData) {
                if (symbol.isBss) {
                    if (symbol.value > object.dataSize || bssBases[i] > UINT32_MAX - symbol.value) {
                        fail("Mach-O data symbol is outside its BSS section");
                    }
                    value = kDataSymbolTag | kBssSymbolTag | (bssBases[i] + symbol.value);
                } else {
                    if (symbol.value > object.dataBytes.size() ||
                        dataBases[i] > UINT32_MAX - symbol.value) {
                        fail("Mach-O data symbol is outside its initialized data section");
                    }
                    value = kDataSymbolTag | (dataBases[i] + symbol.value);
                }
            }
            auto previous = symbolOwners.find(name);
            if (previous != symbolOwners.end() && previous->second != i) fail("duplicate symbol '" + name + "'");
            symbols[name] = value;
            symbolOwners[name] = i;
        }
    }

    std::vector<Import> imports;
    for (const auto &entry : importLibraries) {
        Import item;
        item.symbol = entry.first;
        item.libraryIndex = entry.second;
        imports.push_back(std::move(item));
    }
    std::sort(imports.begin(), imports.end(), [](const Import &a, const Import &b) {
        return a.symbol < b.symbol;
    });
    uint32_t stubStart = alignTo(textSize, 4);
    uint32_t cursor = stubStart;
    for (auto &import : imports) {
        import.stubOffset = cursor;
        cursor += 12;
    }
    image.code.resize(cursor, '\0');
    uint32_t gotSize = static_cast<uint32_t>(imports.size() * 8);
    for (size_t i = 0; i < imports.size(); ++i) {
        imports[i].pointerOffset = static_cast<uint32_t>(i * 8);
    }
    image.got.assign(gotSize, '\0');
    for (auto &import : imports) {
        put32(image.code, import.stubOffset, 0x90000010u);
        put32(image.code, import.stubOffset + 4, 0xf9400210u);
        put32(image.code, import.stubOffset + 8, 0xd61f0200u);
    }

    auto symbolValue = [&](size_t unitIndex, const std::string &original) -> uint64_t {
        if (original == "..") return unitBases[unitIndex];
        auto local = renamed[unitIndex].find(original);
        std::string name = local == renamed[unitIndex].end() ? original : local->second;
        auto found = symbols.find(name);
        if (found != symbols.end()) return found->second;
        auto imported = std::find_if(imports.begin(), imports.end(),
                                     [&](const Import &item) { return item.symbol == original; });
        if (imported != imports.end()) return imported->stubOffset;
        fail("unresolved symbol '" + original + "'");
    };

    for (size_t i = 0; i < inputs.units.size(); ++i) {
        for (const auto &relocation : inputs.units[i].object.relocations) {
            uint32_t site = unitBases[i] + relocation.offset;
            if (site > image.code.size() || image.code.size() - site < 4) {
                fail("arm64 relocation lies outside __text in '" + inputs.units[i].label + "'");
            }
            uint64_t value = symbolValue(i, relocation.symbolName);
            auto imported = std::find_if(imports.begin(), imports.end(),
                [&](const Import &item) { return item.symbol == relocation.symbolName; });
            bool gotRelocation = relocation.type == kGotPage21 || relocation.type == kGotPageoff12;
            uint32_t dataOffset = alignTo(kPageSize + static_cast<uint32_t>(image.code.size()), kPageSize);
            uint64_t targetAddress;
            if (imported != imports.end() && (gotRelocation ||
                relocation.type == kArm64DarwinRelocPage21 ||
                relocation.type == kArm64DarwinRelocPageoff12)) {
                targetAddress = kImageBase + dataOffset + imported->pointerOffset;
            } else if (value & kDataSymbolTag) {
                uint32_t symbolOffset = static_cast<uint32_t>(value & ~(kDataSymbolTag | kBssSymbolTag));
                if (value & kBssSymbolTag) {
                    symbolOffset += alignTo(gotSize + initializedDataSize, 16);
                } else {
                    symbolOffset += gotSize;
                }
                targetAddress = kImageBase + dataOffset + symbolOffset;
            } else {
                targetAddress = kImageBase + kPageSize + value;
            }
            if (relocation.type == kArm64DarwinRelocBranch26) {
                int64_t delta = static_cast<int64_t>(targetAddress) -
                                static_cast<int64_t>(kImageBase + kPageSize + site);
                if ((delta & 3) != 0 || delta < -(1LL << 27) || delta >= (1LL << 27)) {
                    fail("arm64 branch target is out of range");
                }
                uint32_t instruction = get32(image.code, site);
                put32(image.code, site, (instruction & 0xfc000000u) |
                                       (static_cast<uint32_t>(delta >> 2) & 0x03ffffffu));
            } else if (relocation.type == kArm64DarwinRelocPage21 || relocation.type == kGotPage21) {
                patchPage21(image.code, site, targetAddress);
            } else if (relocation.type == kArm64DarwinRelocPageoff12 ||
                       relocation.type == kGotPageoff12) {
                patchPageoff12(image.code, site, targetAddress);
            } else {
                fail("unsupported arm64 relocation " + std::to_string(relocation.type));
            }
        }
    }

    auto main = symbols.find("_main");
    if (main == symbols.end()) fail("required _main symbol is missing");
    if (main->second & kDataSymbolTag) fail("_main must be in the Mach-O __text section");
    image.mainOffset = main->second;
    int64_t startupBranch = static_cast<int64_t>(image.mainOffset) - 0;
    if ((startupBranch & 3) || startupBranch >= (1LL << 27)) fail("main function is outside startup branch range");
    put32(image.code, 0, 0x94000000u | (static_cast<uint32_t>(startupBranch >> 2) & 0x03ffffffu));
    if (hasSystemExit) {
        auto exitStub = std::find_if(imports.begin(), imports.end(),
                                     [](const Import &item) { return item.symbol == "_exit"; });
        if (exitStub == imports.end()) fail("system exit import stub is missing");
        int64_t exitBranch = static_cast<int64_t>(exitStub->stubOffset) - 4;
        if ((exitBranch & 3) || exitBranch >= (1LL << 27)) fail("system exit stub is outside startup branch range");
        put32(image.code, 4, 0x94000000u | (static_cast<uint32_t>(exitBranch >> 2) & 0x03ffffffu));
    }
    image.imports = std::move(imports);
    for (const auto &entry : symbols) image.symbols.emplace_back(entry.first, entry.second);
    return image;
}

void writeAtomic(const std::string &path, const Bytes &bytes) {
    std::filesystem::path destination(path);
    std::filesystem::path directory = destination.parent_path();
    if (directory.empty()) directory = ".";
    std::filesystem::path temporary = directory / (destination.filename().string() + ".rcc-tmp");
    for (unsigned suffix = 0; std::filesystem::exists(temporary); ++suffix) {
        temporary = directory / (destination.filename().string() + ".rcc-tmp-" + std::to_string(suffix));
    }
    try {
        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            if (!output) fail("cannot create output '" + temporary.string() + "'");
            output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            output.close();
            if (!output) fail("failed writing output '" + temporary.string() + "'");
        }
        std::filesystem::permissions(temporary, std::filesystem::perms::owner_all |
                                     std::filesystem::perms::group_read |
                                     std::filesystem::perms::group_exec |
                                     std::filesystem::perms::others_read |
                                     std::filesystem::perms::others_exec,
                                     std::filesystem::perm_options::replace);
        std::filesystem::rename(temporary, destination);
    } catch (...) {
        std::error_code error;
        std::filesystem::remove(temporary, error);
        throw;
    }
}

}  // namespace

void nativeMacOSLink(const std::vector<std::string> &inputs, const std::string &outputPath) {
    std::filesystem::path sdkRoot = findSdkRoot();
    Inputs loaded = loadInputs(inputs, sdkRoot);
    MachOImage image = mergeObjects(std::move(loaded));
    Bytes output = makeMachExecutable(image);
    writeAtomic(outputPath, output);
}

}  // namespace rcc
