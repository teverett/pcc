#include "HostedLinker.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

#ifdef _WIN32
#include <windows.h>
#endif

#include "fileformat/archive/ArchiveReader.h"
#include "fileformat/coff/HostedCoffReader.h"
#include "fileformat/coff/HostedPeExecutableWriter.h"
#include "fileformat/coff/HostedPeReader.h"
#include "fileformat/coff/PEImage.h"
#include "fileformat/elf/ELFImage.h"
#include "fileformat/elf/HostedElfExecutableWriter.h"
#include "fileformat/elf/HostedElfReader.h"
#include "NativeMacOSLinker.h"
#include "codegen/i386/I386Assembler.h"
#include "fileformat/FileFormat.h"

namespace pcc {
namespace {

using Bytes = std::string;

[[noreturn]] void fail(const std::string &message) {
    throw std::runtime_error("hosted linker: " + message);
}
uint16_t get16(const Bytes &s, size_t p) {
    if (p > s.size() || s.size() - p < 2) fail("truncated object file");
    return static_cast<uint8_t>(s[p]) | (static_cast<uint16_t>(static_cast<uint8_t>(s[p + 1])) << 8);
}
uint32_t get32(const Bytes &s, size_t p) {
    if (p > s.size() || s.size() - p < 4) fail("truncated object file");
    return static_cast<uint32_t>(get16(s, p)) | (static_cast<uint32_t>(get16(s, p + 2)) << 16);
}
void put16(Bytes &s, size_t p, uint16_t v) {
    if (p > s.size() || s.size() - p < 2) fail("internal output range error");
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
Bytes readFile(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) fail("cannot read input '" + path + "'");
    std::ostringstream stream;
    stream << in.rdbuf();
    if (!in.good() && !in.eof()) fail("failed reading input '" + path + "'");
    return stream.str();
}

struct Unit {
    ObjectFile object;
    std::string label;
};

ObjectFile readHostedObject(const Bytes &b, HostedTarget target) {
    if (target == HostedTarget::WindowsI386) return readHostedCoff(b);
    FileFormatKind kind = detectFileFormat(b);
    if ((target == HostedTarget::LinuxI386 && kind != FileFormatKind::Elf) ||
        (target == HostedTarget::WindowsI386 && kind != FileFormatKind::Coff)) {
        fail("input object format does not match the selected hosted target");
    }
    return makeFileFormatReader(kind)->read(b);
}

std::unordered_set<std::string> unresolved(const std::vector<Unit> &units) {
    std::unordered_set<std::string> defs, needs;
    for (const auto &unit : units) for (const auto &symbol : unit.object.symbols) {
        if (symbol.isDefined) defs.insert(symbol.name);
        else needs.insert(symbol.name);
    }
    for (const auto &name : defs) needs.erase(name);
    return needs;
}

struct Inputs {
    std::vector<Unit> units;
    std::vector<std::string> libraryNames;
    std::vector<HostedImport> imports;
    std::vector<DynamicLibrary> dynamicLibraries;
    bool elfArchiveGroup = false;
};

void loadElfLibrary(const std::filesystem::path &path, Inputs &inputs, std::vector<Bytes> &archives,
                    const std::vector<std::filesystem::path> &searchPaths,
                    std::set<std::filesystem::path> &active, unsigned &inputCount,
                    unsigned depth = 0, bool asNeeded = false) {
    if (depth > 8 || ++inputCount > 128) fail("GNU linker script input/depth limit exceeded");
    auto canonical = std::filesystem::weakly_canonical(path);
    if (!active.insert(canonical).second) fail("cyclic GNU linker script input");
    Bytes data = readFile(path.string());
    if (isElfDynamicImage(data)) {
        auto library = readElfDynamicLibrary(data, path.string());
        library.asNeeded = asNeeded;
        auto previous = std::find_if(inputs.dynamicLibraries.begin(), inputs.dynamicLibraries.end(),
                                     [&](const DynamicLibrary &item) { return item.name == library.name; });
        if (previous == inputs.dynamicLibraries.end()) inputs.dynamicLibraries.push_back(std::move(library));
        else previous->asNeeded &= asNeeded;
    } else if (data.rfind("!<arch>\n", 0) == 0) {
        archives.push_back(std::move(data));
    } else {
        if (data.size() > 65536) fail("GNU linker script exceeds 64 KiB limit");
        std::vector<std::string> tokens;
        for (size_t at = 0; at < data.size();) {
            unsigned char c = data[at];
            if (std::isspace(c)) { ++at; continue; }
            if (data.compare(at, 2, "/*") == 0) {
                size_t end = data.find("*/", at + 2);
                if (end == Bytes::npos) fail("unterminated GNU linker script comment");
                at = end + 2;
                continue;
            }
            if (c == '(' || c == ')' || c == ',' || c == ';') {
                tokens.emplace_back(1, c);
                ++at;
            } else if (c == '"' || c == '\'') {
                size_t end = data.find(c, at + 1);
                if (end == Bytes::npos) fail("unterminated GNU linker script filename");
                tokens.push_back(data.substr(at + 1, end - at - 1));
                at = end + 1;
            } else {
                size_t start = at;
                while (at < data.size() && !std::isspace(static_cast<unsigned char>(data[at])) &&
                       data[at] != '(' && data[at] != ')' && data[at] != ',' && data[at] != ';') ++at;
                tokens.push_back(data.substr(start, at - start));
            }
        }
        size_t at = 0;
        auto expect = [&](const std::string &token) {
            if (at >= tokens.size() || tokens[at++] != token) fail("invalid GNU linker script syntax");
        };
        auto resolve = [&](const std::string &name) {
            std::vector<std::filesystem::path> candidates;
            if (name.rfind("-l", 0) == 0 && name.size() > 2) {
                for (const auto &dir : searchPaths) {
                    candidates.push_back(dir / ("lib" + name.substr(2) + ".so"));
                    candidates.push_back(dir / ("lib" + name.substr(2) + ".a"));
                }
            } else {
                if (name.find_first_of("*?{}=$\\") != std::string::npos) fail("unsupported GNU linker script filename");
                candidates.emplace_back(name);
                candidates.push_back(path.parent_path() / name);
                for (const auto &dir : searchPaths) {
                    candidates.push_back(dir / name);
                    if (std::filesystem::path(name).is_absolute())
                        candidates.push_back(dir / std::filesystem::path(name).filename());
                }
            }
            for (const auto &candidate : candidates) {
                std::error_code ec;
                if (std::filesystem::is_regular_file(candidate, ec)) return candidate;
            }
            fail("GNU linker script input '" + name + "' was not found");
        };
        auto list = [&](auto &&self, bool optional, unsigned nesting) -> void {
            if (nesting > 8) fail("GNU linker script nesting limit exceeded");
            expect("(");
            while (at < tokens.size() && tokens[at] != ")") {
                std::string token = tokens[at++];
                if (token == "," || token == ";") continue;
                if (token == "AS_NEEDED" || token == "INPUT" || token == "GROUP") {
                    self(self, optional || token == "AS_NEEDED", nesting + 1);
                } else {
                    if (token == "(") fail("invalid GNU linker script input");
                    loadElfLibrary(resolve(token), inputs, archives, searchPaths, active, inputCount,
                                   depth + 1, optional);
                }
            }
            expect(")");
        };
        bool hasInput = false;
        while (at < tokens.size()) {
            std::string command = tokens[at++];
            if (command == ";") continue;
            if (command == "OUTPUT_FORMAT") {
                expect("(");
                if (at >= tokens.size() || tokens[at++] != "elf32-i386") fail("unsupported GNU linker script output format");
                expect(")");
            } else if (command == "INPUT" || command == "GROUP") {
                inputs.elfArchiveGroup |= command == "GROUP";
                hasInput = true;
                list(list, asNeeded, 0);
            } else fail("unsupported GNU linker script directive '" + command + "'");
        }
        if (!hasInput) fail("GNU linker script has no INPUT or GROUP");
    }
    active.erase(canonical);
}

Inputs loadInputs(const std::vector<std::string> &args, HostedTarget target) {
    Inputs result;
    std::vector<std::filesystem::path> searchPaths{"."};
    std::vector<Bytes> archives;
    std::set<std::filesystem::path> activeScripts;
    unsigned scriptInputs = 0;
#ifdef _WIN32
    if (target == HostedTarget::WindowsI386) {
        if (const char *root = std::getenv("SystemRoot")) {
            searchPaths.emplace_back(std::filesystem::path(root) / "SysWOW64");
            searchPaths.emplace_back(std::filesystem::path(root) / "System32");
        }
    }
#endif
    if (target == HostedTarget::LinuxI386) {
        for (size_t i = 0; i < args.size(); ++i) {
            if (args[i] == "-L") {
                if (++i == args.size()) fail("-L requires a directory");
                searchPaths.emplace_back(args[i]);
            } else if (args[i].rfind("-L", 0) == 0) searchPaths.emplace_back(args[i].substr(2));
        }
        searchPaths.insert(searchPaths.end(), {"/usr/lib/i386-linux-gnu", "/lib/i386-linux-gnu",
                                               "/usr/lib32", "/lib32", "/usr/lib", "/lib"});
    }
    for (size_t i = 0; i < args.size(); ++i) {
        const std::string &arg = args[i];
        if (arg == "-L") {
            if (++i == args.size()) fail("-L requires a directory");
            if (target != HostedTarget::LinuxI386) searchPaths.emplace_back(args[i]);
        } else if (arg.rfind("-L", 0) == 0) {
            if (target != HostedTarget::LinuxI386) searchPaths.emplace_back(arg.substr(2));
        } else if (arg == "-l" || arg.rfind("-l", 0) == 0) {
            std::string name;
            if (arg == "-l") {
                if (++i == args.size()) fail("-l requires a library name");
                name = args[i];
            } else name = arg.substr(2);
            if (name.empty()) fail("empty library name");
            result.libraryNames.push_back(name);
            std::filesystem::path found;
            for (const auto &dir : searchPaths) {
                std::vector<std::filesystem::path> candidates;
                if (!name.empty() && name[0] == ':') candidates.push_back(dir / name.substr(1));
                else if (target == HostedTarget::WindowsI386) {
                    candidates.push_back(dir / (name + ".dll"));
                    candidates.push_back(dir / (name + ".lib"));
                    candidates.push_back(dir / ("lib" + name + ".dll.a"));
                    candidates.push_back(dir / ("lib" + name + ".a"));
                }
                else {
                    if (target == HostedTarget::LinuxI386) candidates.push_back(dir / ("lib" + name + ".so"));
                    candidates.push_back(dir / ("lib" + name + ".a"));
                }
                for (const auto &candidate : candidates) {
                    std::error_code ec;
                    if (std::filesystem::is_regular_file(candidate, ec)) { found = candidate; break; }
                }
                if (!found.empty()) break;
            }
            if (found.empty()) fail("library -l" + name + " was not found in the supplied search paths");
            Bytes data = readFile(found.string());
            if (target == HostedTarget::WindowsI386 && data.rfind("MZ", 0) == 0) {
                auto imports = windowsDllImports(data, found.string());
                result.imports.insert(result.imports.end(), imports.begin(), imports.end());
            } else if (target == HostedTarget::LinuxI386 && data.rfind("!<arch>\n", 0) != 0) {
                loadElfLibrary(found, result, archives, searchPaths, activeScripts, scriptInputs);
            } else {
                archives.push_back(std::move(data));
            }
        } else {
            std::string filename = std::filesystem::path(arg).filename().string();
            if (target == HostedTarget::LinuxI386 && filename.find(".so") != std::string::npos) {
                loadElfLibrary(arg, result, archives, searchPaths, activeScripts, scriptInputs);
                continue;
            }
            Bytes data = readFile(arg);
            if (target == HostedTarget::WindowsI386 && data.rfind("MZ", 0) == 0) {
                auto imports = windowsDllImports(data, arg);
                result.imports.insert(result.imports.end(), imports.begin(), imports.end());
                continue;
            }
            if (data.rfind("!<arch>\n", 0) == 0) archives.push_back(std::move(data));
            else {
                if (target == HostedTarget::LinuxI386) validateHostedElfObject(data);
                result.units.push_back({readHostedObject(data, target), arg});
            }
        }
    }
    std::vector<std::vector<ArchiveMember>> archiveMembers;
    for (const auto &archive : archives) {
        auto members = readArchive(archive);
        if (target == HostedTarget::LinuxI386 && result.elfArchiveGroup) {
            if (archiveMembers.empty()) archiveMembers.emplace_back();
            for (auto &member : members) archiveMembers.front().push_back(std::move(member));
        } else archiveMembers.push_back(std::move(members));
    }
    for (const auto &members : archiveMembers) {
        auto gnuImports = target == HostedTarget::WindowsI386 ? gnuCoffImports(members) :
                                                              std::unordered_map<size_t, HostedImport>{};
        std::vector<bool> included(members.size(), false);
        bool changed;
        do {
            changed = false;
            auto needed = unresolved(result.units);
            if (target == HostedTarget::WindowsI386) {
                bool crt = std::any_of(result.libraryNames.begin(), result.libraryNames.end(),
                                       [](const std::string &name) { return name == "msvcrt" || name == "ucrtbase"; });
                for (const auto &entry : gnuImports) {
                    if (entry.second.library == "msvcrt.dll" || entry.second.library == "ucrtbase.dll") crt = true;
                }
                if (crt) needed.insert("_exit");
            }
            for (size_t i = 0; i < members.size(); ++i) {
                if (included[i]) continue;
                auto gnuImport = gnuImports.find(i);
                if (gnuImport != gnuImports.end()) {
                    const HostedImport &item = gnuImport->second;
                    if (std::any_of(item.aliases.begin(), item.aliases.end(),
                                    [&](const std::string &name) { return needed.count(name); })) {
                        result.imports.push_back(item);
                        included[i] = true;
                        changed = true;
                    }
                    continue;
                }
                if (target == HostedTarget::WindowsI386 && isCoffImportObject(members[i].bytes)) {
                    HostedImport imported = readCoffImportObject(members[i].bytes);
                    if (std::any_of(imported.aliases.begin(), imported.aliases.end(),
                                    [&](const std::string &name) { return needed.count(name); })) {
                        result.imports.push_back(std::move(imported));
                        included[i] = true;
                        changed = true;
                    }
                    continue;
                }
                ObjectFile object;
                try {
                    object = readHostedObject(members[i].bytes, target);
                } catch (const std::exception &error) {
                    fail("invalid archive member '" + members[i].name + "': " + error.what());
                }
                bool selected = false;
                for (const auto &sym : object.symbols) {
                    if (sym.isDefined && needed.count(sym.name)) { selected = true; break; }
                }
                if (selected) {
                    if (target == HostedTarget::LinuxI386) validateHostedElfObject(members[i].bytes);
                    result.units.push_back({std::move(object), members[i].name});
                    included[i] = true;
                    changed = true;
                    needed = unresolved(result.units);
                }
            }
        } while (changed);
    }
    if (target == HostedTarget::WindowsI386) {
        auto needed = unresolved(result.units);
        for (const auto &item : result.imports) {
            std::string library = item.library;
            std::transform(library.begin(), library.end(), library.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (library == "msvcrt.dll" || library == "ucrtbase.dll") needed.insert("_exit");
        }
        std::unordered_set<std::string> provided;
        std::vector<HostedImport> selected;
        for (auto &item : result.imports) {
            if (std::any_of(item.aliases.begin(), item.aliases.end(), [&](const std::string &name) {
                    return needed.count(name) && !provided.count(name);
                })) {
                for (const auto &name : item.aliases) provided.insert(name);
                selected.push_back(std::move(item));
            }
        }
        result.imports = std::move(selected);
    }
    return result;
}

ObjectFile assembleStart(HostedTarget target, bool useLibc = false) {
    if (target == HostedTarget::LinuxI386) {
        if (useLibc) {
            ObjectFile object;
            object.machineType = 0x3386;
            // The loader's EDX finalizer is the sixth __libc_start_main argument.
            const uint8_t bytes[] = {
                0x31,0xed,0x5e,0x89,0xe1,0x83,0xe4,0xf0,0x31,0xc0,
                0x50,0x54,0x52,0x50,0x50,0x51,0x56,0x68,0,0,0,0,
                0xe8,0,0,0,0,0xf4
            };
            object.textBytes.assign(std::begin(bytes), std::end(bytes));
            object.symbols = {{"_start", 0, true, false}, {"main", 0, false, false},
                              {"__libc_start_main", 0, false, false}};
            object.relocations = {{18, "main", 1}, {23, "__libc_start_main", 2}};
            return object;
        }
        return assembleI386Object(
            "org 0\n_start:\n"
            "    pop ecx\n"
            "    mov edx,esp\n"
            "    push edx\n"
            "    push ecx\n"
            "    call main\n"
            "    add esp,8\n"
            "    mov ebx,eax\n"
            "    mov eax,1\n"
            "    db 205,128\n");
    }
    if (target == HostedTarget::WindowsI386) {
        ObjectFile start = assembleI386Object(
            std::string("org 0\n_start:\n"
            "    mov eax,0\n"
            "    push eax\n"
            "    push eax\n"
            "    call _main\n"
            "    add esp,8\n"
            "    push eax\n"
            "    call ") + (useLibc ? "_exit\n" : "_ExitProcess\n") +
            "    hlt\n");
        start.machineType = 0x014c;
        return start;
    }
    fail("unsupported hosted target for startup assembly");
}

// The layout/symbol-resolution core shared by mergeElfObjects() and mergePeObjects() below - the
// one piece of the hosted i386 linker genuinely common to both target file formats. Each public
// wrapper hands it its own per-format inputs and pulls the fields its own `ELFImage`/`PEImage`
// needs back out of this intermediate result.
struct MergedProgram {
    std::string code;
    uint32_t bss = 0;
    uint32_t entry = 0;
    uint32_t importOffset = 0;
    uint32_t importSize = 0;
    uint32_t iatOffset = 0;
    uint32_t iatSize = 0;
    bool hasWindowsImports = false;
    std::vector<HostedImport> imports;
    std::vector<DynamicImport> dynamicImports;
    std::vector<DynamicLibrary> dynamicLibraries;
    uint32_t gotOffset = 0;
    std::vector<uint32_t> baseRelocations;
};

MergedProgram mergeObjects(std::vector<Unit> units, HostedTarget target, uint64_t runtimeBase,
                   std::vector<HostedImport> imports = {}, std::vector<DynamicImport> dynamicImports = {},
                   std::vector<DynamicLibrary> dynamicLibraries = {}) {
    if (target == HostedTarget::LinuxI386) {
        for (auto &unit : units) {
            auto &object = unit.object;
            for (const auto &relocation : object.relocations) {
                if (relocation.type != 2) continue;
                size_t at = relocation.offset;
                if (at > object.textBytes.size() || object.textBytes.size() - at < 4)
                    fail("ELF PC32 relocation lies outside .text in '" + unit.label + "'");
                uint32_t addend = 0;
                for (size_t i = 0; i < 4; ++i)
                    addend |= static_cast<uint32_t>(object.textBytes[at + i]) << (i * 8);
                // ObjectFile PC-relative displacements use the end of the operand as their origin.
                addend += 4;
                for (size_t i = 0; i < 4; ++i)
                    object.textBytes[at + i] = static_cast<uint8_t>(addend >> (i * 8));
            }
        }
    }
    ObjectFile startup = assembleStart(target,
        (target == HostedTarget::LinuxI386 && std::any_of(dynamicImports.begin(), dynamicImports.end(),
                    [](const DynamicImport &item) { return item.symbol == "__libc_start_main"; })) ||
        (target == HostedTarget::WindowsI386 && std::any_of(imports.begin(), imports.end(),
                    [](const HostedImport &item) { return item.symbol == "_exit"; })));
    units.insert(units.begin(), {std::move(startup), "<pcc startup>"});
    std::vector<uint32_t> bases(units.size()), dataBases(units.size());
    uint32_t textSize = 0, dataSize = 0;
    for (size_t i = 0; i < units.size(); ++i) {
        if (target == HostedTarget::WindowsI386) {
            size_t aligned = alignTo(textSize, units[i].object.textAlignment);
            if (aligned > UINT32_MAX) fail("aligned program text exceeds format limits");
            textSize = static_cast<uint32_t>(aligned);
        }
        bases[i] = textSize;
        dataBases[i] = dataSize;
        if (units[i].object.textBytes.size() > UINT32_MAX - textSize) fail("program text exceeds format limits");
        textSize += static_cast<uint32_t>(units[i].object.textBytes.size());
        if (units[i].object.dataSize > UINT32_MAX - dataSize) fail("program data exceeds format limits");
        dataSize += units[i].object.dataSize;
    }
    uint32_t dataVirtualOffset = textSize;
    MergedProgram out;
    out.code.assign(textSize, '\0');
    out.bss = dataSize;
    out.dynamicImports = std::move(dynamicImports);
    out.dynamicLibraries = std::move(dynamicLibraries);
    if (target == HostedTarget::LinuxI386 && !out.dynamicLibraries.empty()) {
        out.code.resize(static_cast<size_t>(textSize) + dataSize, '\0');
        out.bss = 0;
    }
    if (target == HostedTarget::WindowsI386) {
        if (std::none_of(imports.begin(), imports.end(), [](const HostedImport &item) {
                return item.symbol == "_exit" ||
                       std::find(item.aliases.begin(), item.aliases.end(), "_ExitProcess") != item.aliases.end();
            })) {
            imports.push_back({"_ExitProcess", "ExitProcess", "kernel32.dll", {"_ExitProcess", "ExitProcess"}});
        }
        out.hasWindowsImports = true;
        out.imports = std::move(imports);
        std::map<std::string, std::vector<size_t>> groups;
        uint32_t thunkStart = static_cast<uint32_t>(alignTo(textSize, 4));
        for (size_t i = 0; i < out.imports.size(); ++i) {
            out.imports[i].thunkOffset = thunkStart + static_cast<uint32_t>(i * 6);
            groups[out.imports[i].library].push_back(i);
        }
        out.importOffset = static_cast<uint32_t>(alignTo(thunkStart + out.imports.size() * 6, 4));
        uint32_t cursor = out.importOffset + static_cast<uint32_t>((groups.size() + 1) * 20);
        std::map<std::string, uint32_t> lookupOffsets, iatOffsets, libraryNameOffsets;
        for (const auto &group : groups) {
            lookupOffsets[group.first] = cursor;
            cursor += static_cast<uint32_t>((group.second.size() + 1) * 4);
        }
        for (const auto &group : groups) {
            iatOffsets[group.first] = cursor;
            cursor += static_cast<uint32_t>((group.second.size() + 1) * 4);
        }
        for (const auto &group : groups) {
            libraryNameOffsets[group.first] = cursor;
            cursor += static_cast<uint32_t>(group.first.size() + 1);
        }
        for (const auto &group : groups) {
            for (size_t index : group.second) {
                cursor = static_cast<uint32_t>(alignTo(cursor, 2));
                out.imports[index].hintNameOffset = cursor;
                if (!out.imports[index].byOrdinal) {
                    cursor += static_cast<uint32_t>(2 + out.imports[index].name.size() + 1);
                }
            }
        }
        out.iatOffset = groups.empty() ? 0 : iatOffsets.begin()->second;
        for (const auto &group : groups) out.iatSize += static_cast<uint32_t>((group.second.size() + 1) * 4);
        out.importSize = static_cast<uint32_t>((groups.size() + 1) * 20);
        Bytes importData(cursor - out.importOffset, '\0');
        constexpr uint32_t sectionRva = 0x1000;
        size_t descriptorIndex = 0;
        for (const auto &group : groups) {
            size_t descriptor = descriptorIndex++ * 20;
            uint32_t lookupRva = sectionRva + lookupOffsets[group.first];
            uint32_t nameRva = sectionRva + libraryNameOffsets[group.first];
            uint32_t iatRva = sectionRva + iatOffsets[group.first];
            put32(importData, descriptor, lookupRva);
            put32(importData, descriptor + 12, nameRva);
            put32(importData, descriptor + 16, iatRva);
            importData.replace(libraryNameOffsets[group.first] - out.importOffset,
                               group.first.size() + 1, group.first + '\0');
            for (size_t i = 0; i < group.second.size(); ++i) {
                HostedImport &item = out.imports[group.second[i]];
                item.iatOffset = iatOffsets[group.first] + static_cast<uint32_t>(i * 4);
                uint32_t hintNameRva = item.byOrdinal ? 0x80000000u | item.ordinal :
                                                       sectionRva + item.hintNameOffset;
                put32(importData, lookupOffsets[group.first] - out.importOffset + i * 4, hintNameRva);
                put32(importData, iatOffsets[group.first] - out.importOffset + i * 4, hintNameRva);
                if (!item.byOrdinal) {
                    importData.replace(item.hintNameOffset - out.importOffset + 2, item.name.size() + 1,
                                       item.name + '\0');
                }
            }
        }
        out.code.resize(thunkStart, '\0');
        for (const HostedImport &item : out.imports) {
            out.code += "\xff\x25";
            out.code.append(4, '\0');
            put32(out.code, item.thunkOffset + 2, kPeBase + sectionRva + item.iatOffset);
            out.baseRelocations.push_back(sectionRva + item.thunkOffset + 2);
        }
        out.code.resize(out.importOffset, '\0');
        out.code += importData;
    } else if (target == HostedTarget::LinuxI386 && !out.dynamicImports.empty()) {
        uint32_t pltStart = static_cast<uint32_t>(alignTo(out.code.size(), 16));
        out.gotOffset = static_cast<uint32_t>(alignTo(pltStart + (out.dynamicImports.size() + 1) * 16, 4));
        for (size_t i = 0; i < out.dynamicImports.size(); ++i) {
            out.dynamicImports[i].pltOffset = pltStart + static_cast<uint32_t>((i + 1) * 16);
            out.dynamicImports[i].gotOffset = out.gotOffset + 12 + static_cast<uint32_t>(i * 4);
        }
        out.code.resize(pltStart, '\x90');
        Bytes plt;
        plt += "\xff\x35";
        plt.append(4, '\0');
        plt += "\xff\x25";
        plt.append(4, '\0');
        plt.append("\x0f\x1f\x40\x00", 4);
        put32(plt, 2, static_cast<uint32_t>(runtimeBase + out.gotOffset + 4));
        put32(plt, 8, static_cast<uint32_t>(runtimeBase + out.gotOffset + 8));
        for (size_t i = 0; i < out.dynamicImports.size(); ++i) {
            const auto &item = out.dynamicImports[i];
            size_t start = plt.size();
            plt += "\xff\x25";
            plt.append(4, '\0');
            put32(plt, start + 2, static_cast<uint32_t>(runtimeBase + item.gotOffset));
            plt.push_back('\x68');
            uint32_t relocationIndex = static_cast<uint32_t>(i * 8);
            for (int shift = 0; shift < 32; shift += 8) plt.push_back(static_cast<char>(relocationIndex >> shift));
            plt.push_back('\xe9');
            int32_t displacement = static_cast<int32_t>(pltStart - (item.pltOffset + 16));
            for (int shift = 0; shift < 32; shift += 8) plt.push_back(static_cast<char>(displacement >> shift));
        }
        out.code += plt;
        out.code.resize(out.gotOffset, '\0');
        out.code.append(12 + out.dynamicImports.size() * 4, '\0');
        for (const auto &item : out.dynamicImports) {
            put32(out.code, item.gotOffset, static_cast<uint32_t>(runtimeBase + item.pltOffset + 6));
        }
    }
    std::unordered_map<std::string, uint32_t> symbols;
    std::unordered_map<std::string, size_t> owner;
    std::vector<std::unordered_map<std::string, std::string>> renamed(units.size());
    for (size_t i = 0; i < units.size(); ++i) {
        auto &obj = units[i].object;
        if (obj.machineType != 0 && obj.machineType != 0x3386 &&
            !(target == HostedTarget::WindowsI386 && obj.machineType == 0x014c)) {
            fail("object machine type does not match the hosted target");
        }
        std::copy(obj.textBytes.begin(), obj.textBytes.end(), out.code.begin() + bases[i]);
        for (const auto &sym : obj.symbols) {
            if (!sym.isDefined) continue;
            if (sym.name == ".." || sym.name == "..d") continue;
            std::string name = sym.name;
            bool local = name.rfind("__pcc_local_", 0) == 0 || name.rfind("__pcc_tmp_", 0) == 0;
            if (local) {
                auto old = renamed[i].find(name);
                if (old != renamed[i].end()) name = old->second;
                else {
                    std::string original = name;
                    name += "$pcc" + std::to_string(i);
                    renamed[i][original] = name;
                }
            }
            uint32_t value = sym.isData ? dataVirtualOffset + dataBases[i] + sym.value : bases[i] + sym.value;
            auto found = owner.find(name);
            if (found != owner.end() && found->second != i) fail("duplicate symbol '" + name + "'");
            symbols[name] = value;
            owner[name] = i;
        }
    }
    auto symbolValue = [&](size_t i, const std::string &original) -> uint32_t {
        if (original == "..") return bases[i];
        if (original == "..d") return textSize + dataBases[i];
        std::string name = original;
        auto local = renamed[i].find(original);
        if (local != renamed[i].end()) name = local->second;
        auto found = symbols.find(name);
        if (found == symbols.end()) {
            if (target == HostedTarget::LinuxI386) {
                auto imported = std::find_if(out.dynamicImports.begin(), out.dynamicImports.end(),
                    [&](const DynamicImport &item) { return item.symbol == original; });
                if (imported != out.dynamicImports.end()) return imported->pltOffset;
            }
            if (target == HostedTarget::WindowsI386) {
                auto imported = std::find_if(out.imports.begin(), out.imports.end(),
                    [&](const HostedImport &item) {
                        return std::find(item.aliases.begin(), item.aliases.end(), original) != item.aliases.end();
                    });
                if (imported != out.imports.end()) {
                    return original.rfind("__imp_", 0) == 0 ? imported->iatOffset : imported->thunkOffset;
                }
            }
            fail("unresolved symbol '" + original + "'");
        }
        return found->second;
    };
    for (size_t i = 0; i < units.size(); ++i) {
        for (const auto &r : units[i].object.relocations) {
            uint32_t site = bases[i] + r.offset;
            uint64_t targetAddress = runtimeBase + symbolValue(i, r.symbolName);
            if (site > out.code.size() || out.code.size() - site < 4) fail("i386 relocation lies outside .text");
            uint32_t addend = get32(out.code, site);
            uint32_t value;
            if (r.type == 1) {
                value = static_cast<uint32_t>(targetAddress) + addend;
                if (target == HostedTarget::WindowsI386) out.baseRelocations.push_back(0x1000 + site);
            }
            else if (r.type == 2) value = static_cast<uint32_t>(targetAddress - runtimeBase - (site + 4)) + addend;
            else if (target == HostedTarget::WindowsI386 && r.type == 3) {
                value = static_cast<uint32_t>(targetAddress - kPeBase) + addend;
            }
            else fail("unsupported hosted i386 relocation " + std::to_string(r.type));
            put32(out.code, site, value);
        }
    }
    auto entry = symbols.find("_start");
    if (entry == symbols.end()) fail("startup entry symbol is missing");
    out.entry = entry->second;
    auto main = symbols.find("main");
    if (main == symbols.end()) main = symbols.find("_main");
    if (main == symbols.end()) fail("required main symbol is missing");
    return out;
}

ELFImage mergeElfObjects(std::vector<Unit> units, uint64_t runtimeBase,
                   std::vector<DynamicImport> dynamicImports, std::vector<DynamicLibrary> dynamicLibraries) {
    MergedProgram merged = mergeObjects(std::move(units), HostedTarget::LinuxI386, runtimeBase, {},
                                        std::move(dynamicImports), std::move(dynamicLibraries));
    ELFImage image;
    image.code = std::move(merged.code);
    image.bss = merged.bss;
    image.entry = merged.entry;
    image.dynamicImports = std::move(merged.dynamicImports);
    image.dynamicLibraries = std::move(merged.dynamicLibraries);
    image.gotOffset = merged.gotOffset;
    return image;
}

PEImage mergePeObjects(std::vector<Unit> units, uint64_t runtimeBase, std::vector<HostedImport> imports) {
    MergedProgram merged = mergeObjects(std::move(units), HostedTarget::WindowsI386, runtimeBase,
                                        std::move(imports));
    PEImage image;
    image.code = std::move(merged.code);
    image.bss = merged.bss;
    image.entry = merged.entry;
    image.importOffset = merged.importOffset;
    image.importSize = merged.importSize;
    image.iatOffset = merged.iatOffset;
    image.iatSize = merged.iatSize;
    image.hasWindowsImports = merged.hasWindowsImports;
    image.imports = std::move(merged.imports);
    image.baseRelocations = std::move(merged.baseRelocations);
    return image;
}


void writeAtomic(const std::string &path, const Bytes &data, bool executable) {
    std::filesystem::path destination(path);
    std::filesystem::path parent = destination.parent_path();
    if (parent.empty()) parent = ".";
    std::filesystem::path temporary = parent / (destination.filename().string() + ".pcc-tmp");
    for (unsigned suffix = 0; std::filesystem::exists(temporary); ++suffix) {
        temporary = parent / (destination.filename().string() + ".pcc-tmp-" + std::to_string(suffix));
    }
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        if (!out) fail("cannot create temporary output beside '" + path + "'");
        out.write(data.data(), static_cast<std::streamsize>(data.size()));
        out.close();
        if (!out) {
            std::filesystem::remove(temporary);
            fail("failed writing output '" + path + "'");
        }
    }
    if (executable) {
        std::error_code permissionsError;
        std::filesystem::permissions(temporary,
            std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec |
                std::filesystem::perms::others_exec,
            std::filesystem::perm_options::add, permissionsError);
        if (permissionsError) {
            std::filesystem::remove(temporary);
            fail("cannot mark executable output '" + path + "': " + permissionsError.message());
        }
    }
#ifdef _WIN32
    if (!MoveFileExW(temporary.wstring().c_str(), destination.wstring().c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DWORD error = GetLastError();
        std::filesystem::remove(temporary);
        fail("cannot atomically replace output '" + path + "' (Windows error " + std::to_string(error) + ")");
    }
#else
    std::error_code ec;
    std::filesystem::rename(temporary, destination, ec);
    if (ec) {
        std::filesystem::remove(temporary);
        fail("cannot atomically replace output '" + path + "': " + ec.message());
    }
#endif
}

}  // namespace

void linkHosted(const std::vector<std::string> &inputs, HostedTarget target, const std::string &outputPath) {
    if (target == HostedTarget::MacOSArm64) {
        nativeMacOSLink(inputs, outputPath);
        return;
    }
    auto loaded = loadInputs(inputs, target);
    auto needed = unresolved(loaded.units);
    if (target == HostedTarget::LinuxI386 &&
        std::any_of(loaded.dynamicLibraries.begin(), loaded.dynamicLibraries.end(),
                    [](const DynamicLibrary &library) {
                        return !library.asNeeded && library.exports.count("__libc_start_main");
                    })) {
        needed.insert("__libc_start_main");
    }
    std::vector<DynamicImport> dynamicImports;
    for (const auto &name : needed) {
        if (target == HostedTarget::LinuxI386) {
            auto library = std::find_if(loaded.dynamicLibraries.begin(), loaded.dynamicLibraries.end(),
                [&](const DynamicLibrary &candidate) { return candidate.exports.count(name) != 0; });
            if (library != loaded.dynamicLibraries.end()) {
                const auto &symbol = library->exports.at(name);
                bool untypedFunction = symbol.type == 0 &&
                    std::none_of(loaded.units.begin(), loaded.units.end(), [&](const Unit &unit) {
                        return std::any_of(unit.object.relocations.begin(), unit.object.relocations.end(),
                                           [&](const auto &relocation) {
                                               return relocation.symbolName == name && relocation.type != 2;
                                           });
                    });
                if (symbol.type != 2 && symbol.type != 10 && !untypedFunction) {
                    fail("unsupported ELF data/TLS import '" + name + "'");
                }
                dynamicImports.push_back({name, library->name, 0, 0, symbol.version});
                library->asNeeded = false;
                continue;
            }
            fail("unresolved symbol '" + name + "'; provide a matching i386 shared library or static archive");
        } else if (target == HostedTarget::WindowsI386 &&
                   std::any_of(loaded.imports.begin(), loaded.imports.end(),
                       [&](const HostedImport &item) {
                           return std::find(item.aliases.begin(), item.aliases.end(), name) != item.aliases.end();
                       })) {
            continue;
        } else {
            if (target == HostedTarget::WindowsI386) {
                fail("unresolved Windows import '" + name + "'; provide a supported i386 COFF import library");
            }
            fail("unsupported unresolved symbol '" + name + "'");
        }
    }
    if (target == HostedTarget::LinuxI386) {
        loaded.dynamicLibraries.erase(
            std::remove_if(loaded.dynamicLibraries.begin(), loaded.dynamicLibraries.end(),
                           [](const DynamicLibrary &library) { return library.asNeeded; }),
            loaded.dynamicLibraries.end());
    }
    Bytes output;
    if (target == HostedTarget::LinuxI386) {
        ELFImage image = mergeElfObjects(std::move(loaded.units), kElfBase + 0x1000,
                                         std::move(dynamicImports), std::move(loaded.dynamicLibraries));
        output = makeElf(image);
    } else {
        PEImage image = mergePeObjects(std::move(loaded.units), kPeBase + 0x1000, std::move(loaded.imports));
        output = makePe(image);
    }
    writeAtomic(outputPath, output, target != HostedTarget::WindowsI386);
}

}  // namespace pcc
