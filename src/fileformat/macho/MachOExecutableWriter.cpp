#include "MachOExecutableWriter.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <unordered_map>

namespace rcc {
namespace {

using Bytes = std::string;

[[noreturn]] void fail(const std::string &message) {
    throw std::runtime_error("Mach-O writer: " + message);
}

uint32_t get32(const Bytes &data, size_t offset) {
    return static_cast<uint32_t>(static_cast<uint8_t>(data[offset])) |
           (static_cast<uint32_t>(static_cast<uint8_t>(data[offset + 1])) << 8) |
           (static_cast<uint32_t>(static_cast<uint8_t>(data[offset + 2])) << 16) |
           (static_cast<uint32_t>(static_cast<uint8_t>(data[offset + 3])) << 24);
}

void put16(Bytes &data, size_t offset, uint16_t value) {
    if (offset > data.size() || data.size() - offset < 2) fail("internal output range error");
    data[offset] = static_cast<char>(value);
    data[offset + 1] = static_cast<char>(value >> 8);
}

void put32(Bytes &data, size_t offset, uint32_t value) {
    put16(data, offset, static_cast<uint16_t>(value));
    put16(data, offset + 2, static_cast<uint16_t>(value >> 16));
}

void put64(Bytes &data, size_t offset, uint64_t value) {
    put32(data, offset, static_cast<uint32_t>(value));
    put32(data, offset + 4, static_cast<uint32_t>(value >> 32));
}

void appendBE32(Bytes &data, uint32_t value) {
    data.push_back(static_cast<char>(value >> 24));
    data.push_back(static_cast<char>(value >> 16));
    data.push_back(static_cast<char>(value >> 8));
    data.push_back(static_cast<char>(value));
}

uint32_t alignTo(uint32_t value, uint32_t alignment) {
    if (value > std::numeric_limits<uint32_t>::max() - alignment) fail("output exceeds format limits");
    return (value + alignment - 1) & ~(alignment - 1);
}

void appendUleb(Bytes &data, uint64_t value) {
    do {
        uint8_t byte = static_cast<uint8_t>(value & 0x7f);
        value >>= 7;
        if (value != 0) byte |= 0x80;
        data.push_back(static_cast<char>(byte));
    } while (value != 0);
}

Bytes makeBindOpcodes(const MachOImage &image, uint32_t dataOffsetInVm) {
    Bytes bind;
    for (const auto &item : image.imports) {
        const uint32_t ordinal = item.libraryIndex + 1;
        if (ordinal <= 15) bind.push_back(static_cast<char>(0x10 | ordinal));
        else {
            bind.push_back(static_cast<char>(0x20));
            appendUleb(bind, ordinal);
        }
        bind.push_back(static_cast<char>(0x40));
        bind += item.symbol;
        bind.push_back('\0');
        bind.push_back(static_cast<char>(0x51));
        bind.push_back(static_cast<char>(0x72));
        appendUleb(bind, dataOffsetInVm + item.pointerOffset);
        bind.push_back(static_cast<char>(0x90));
    }
    bind.push_back('\0');
    return bind;
}

const std::array<uint32_t, 64> kShaK = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2,
};

Bytes sha256(const Bytes &input) {
    std::array<uint32_t, 8> hash = {
        0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
        0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19,
    };
    Bytes data = input;
    uint64_t bitLength = static_cast<uint64_t>(data.size()) * 8;
    data.push_back(static_cast<char>(0x80));
    while (data.size() % 64 != 56) data.push_back('\0');
    for (int shift = 56; shift >= 0; shift -= 8) data.push_back(static_cast<char>(bitLength >> shift));
    auto rotateRight = [](uint32_t value, unsigned count) {
        return (value >> count) | (value << (32 - count));
    };
    for (size_t block = 0; block < data.size(); block += 64) {
        uint32_t words[64];
        for (size_t i = 0; i < 16; ++i) {
            size_t p = block + i * 4;
            words[i] = (static_cast<uint32_t>(static_cast<uint8_t>(data[p])) << 24) |
                       (static_cast<uint32_t>(static_cast<uint8_t>(data[p + 1])) << 16) |
                       (static_cast<uint32_t>(static_cast<uint8_t>(data[p + 2])) << 8) |
                       static_cast<uint8_t>(data[p + 3]);
        }
        for (size_t i = 16; i < 64; ++i) {
            uint32_t s0 = rotateRight(words[i - 15], 7) ^ rotateRight(words[i - 15], 18) ^ (words[i - 15] >> 3);
            uint32_t s1 = rotateRight(words[i - 2], 17) ^ rotateRight(words[i - 2], 19) ^ (words[i - 2] >> 10);
            words[i] = words[i - 16] + s0 + words[i - 7] + s1;
        }
        uint32_t a = hash[0], b = hash[1], c = hash[2], d = hash[3];
        uint32_t e = hash[4], f = hash[5], g = hash[6], h = hash[7];
        for (size_t i = 0; i < 64; ++i) {
            uint32_t sum1 = rotateRight(e, 6) ^ rotateRight(e, 11) ^ rotateRight(e, 25);
            uint32_t choose = (e & f) ^ (~e & g);
            uint32_t temp1 = h + sum1 + choose + kShaK[i] + words[i];
            uint32_t sum0 = rotateRight(a, 2) ^ rotateRight(a, 13) ^ rotateRight(a, 22);
            uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
            uint32_t temp2 = sum0 + majority;
            h = g; g = f; f = e; e = d + temp1; d = c; c = b; b = a; a = temp1 + temp2;
        }
        hash[0] += a; hash[1] += b; hash[2] += c; hash[3] += d;
        hash[4] += e; hash[5] += f; hash[6] += g; hash[7] += h;
    }
    Bytes output;
    for (uint32_t word : hash) {
        for (int shift = 24; shift >= 0; shift -= 8) output.push_back(static_cast<char>(word >> shift));
    }
    return output;
}

Bytes makeDylibCommand(uint32_t command, const std::string &name) {
    uint32_t size = alignTo(static_cast<uint32_t>(24 + name.size() + 1), 8);
    Bytes bytes(size, '\0');
    put32(bytes, 0, command);
    put32(bytes, 4, size);
    put32(bytes, 8, 24);
    put32(bytes, 12, 2);
    put32(bytes, 16, 0x10000);
    put32(bytes, 20, 0x10000);
    bytes.replace(24, name.size(), name);
    return bytes;
}

Bytes makeStringCommand(uint32_t command, const std::string &value) {
    uint32_t size = alignTo(static_cast<uint32_t>(12 + value.size() + 1), 8);
    Bytes bytes(size, '\0');
    put32(bytes, 0, command);
    put32(bytes, 4, size);
    put32(bytes, 8, 12);
    bytes.replace(12, value.size(), value);
    return bytes;
}

void setSegmentName(Bytes &command, size_t offset, const char *name) {
    std::fill(command.begin() + static_cast<std::ptrdiff_t>(offset),
              command.begin() + static_cast<std::ptrdiff_t>(offset + 16), '\0');
    std::memcpy(command.data() + offset, name, std::min<size_t>(16, std::strlen(name)));
}

Bytes makeSegment(const char *name, uint64_t vmAddress, uint64_t vmSize,
                  uint64_t fileOffset, uint64_t fileSize, uint32_t maxProtection,
                  uint32_t initialProtection, const std::vector<Bytes> &sections) {
    uint32_t commandSize = 72;
    for (const auto &section : sections) commandSize += static_cast<uint32_t>(section.size());
    Bytes command(commandSize, '\0');
    put32(command, 0, 0x19);
    put32(command, 4, commandSize);
    setSegmentName(command, 8, name);
    put64(command, 24, vmAddress);
    put64(command, 32, vmSize);
    put64(command, 40, fileOffset);
    put64(command, 48, fileSize);
    put32(command, 56, maxProtection);
    put32(command, 60, initialProtection);
    put32(command, 64, static_cast<uint32_t>(sections.size()));
    size_t offset = 72;
    for (const auto &section : sections) {
        command.replace(offset, section.size(), section);
        offset += section.size();
    }
    return command;
}

Bytes makeSection(const char *sectionName, const char *segmentName, uint64_t vmAddress,
                  uint64_t size, uint32_t fileOffset, uint32_t alignment, uint32_t flags) {
    Bytes section(80, '\0');
    setSegmentName(section, 0, sectionName);
    setSegmentName(section, 16, segmentName);
    put64(section, 32, vmAddress);
    put64(section, 40, size);
    put32(section, 48, fileOffset);
    put32(section, 52, alignment);
    put32(section, 64, flags);
    return section;
}

void putNlist(Bytes &symbols, uint32_t stringIndex, uint8_t type, uint8_t section,
              uint16_t description, uint64_t value) {
    size_t offset = symbols.size();
    symbols.append(16, '\0');
    put32(symbols, offset, stringIndex);
    symbols[offset + 4] = static_cast<char>(type);
    symbols[offset + 5] = static_cast<char>(section);
    put16(symbols, offset + 6, description);
    put64(symbols, offset + 8, value);
}

}  // namespace

uint32_t readInstruction(const Bytes &bytes, size_t offset) {
    return get32(bytes, offset);
}

void writeInstruction(Bytes &bytes, size_t offset, uint32_t instruction) {
    put32(bytes, offset, instruction);
}

void patchPage21(Bytes &code, uint32_t site, uint64_t targetAddress) {
    uint32_t instruction = readInstruction(code, site);
    int64_t pages = (static_cast<int64_t>(targetAddress & ~0xfffULL) -
                     static_cast<int64_t>((kImageBase + kPageSize + site) & ~0xfffULL)) >> 12;
    if (pages < -(1LL << 20) || pages >= (1LL << 20)) fail("arm64 page relocation is out of range");
    uint32_t immediate = static_cast<uint32_t>(pages) & 0x1fffff;
    instruction = (instruction & ~0x60ffffe0u) | ((immediate & 3) << 29) | ((immediate >> 2) << 5);
    writeInstruction(code, site, instruction);
}

void patchPageoff12(Bytes &code, uint32_t site, uint64_t targetAddress) {
    uint32_t instruction = readInstruction(code, site);
    uint32_t low = static_cast<uint32_t>(targetAddress & 0xfff);
    uint32_t scale = (instruction & 0x7f000000u) == 0x11000000u ? 0 : (instruction >> 30) & 3;
    if ((low & ((1u << scale) - 1)) != 0) fail("unaligned arm64 page-offset relocation");
    instruction = (instruction & ~0x003ffc00u) | ((low >> scale) << 10);
    writeInstruction(code, site, instruction);
}

Bytes makeMachExecutable(MachOImage &image) {
    constexpr uint32_t kHeaderSize = 32;
    constexpr uint32_t kSectionSize = 80;
    constexpr uint32_t kCodeSignatureCommand = 0x1d;
    constexpr uint32_t kBindInfoCommand = 0x80000022;
    constexpr uint32_t kLoadDylinkerCommand = 0xe;
    constexpr uint32_t kLoadDylibCommand = 0xc;
    constexpr uint32_t kRpathCommand = 0x8000001c;
    constexpr uint32_t kMainCommand = 0x80000028;
    constexpr uint32_t kBuildVersionCommand = 0x32;
    const std::string dylinkerPath = "/usr/lib/dyld";

    std::vector<Bytes> textSections = {
        makeSection("__text", "__TEXT", 0, image.code.size(), 0, 2, 0x80000400),
    };
    std::vector<Bytes> dataSections;
    const uint32_t gotSize = static_cast<uint32_t>(image.imports.size() * 8);
    uint32_t bssOffset = alignTo(gotSize + static_cast<uint32_t>(image.data.size()), 16);
    if (gotSize) dataSections.push_back(makeSection("__got", "__DATA", 0, gotSize, 0, 3, 0));
    if (!image.data.empty()) {
        dataSections.push_back(makeSection("__data", "__DATA", 0, image.data.size(), 0, 3, 0));
    }
    if (image.bssSize) {
        dataSections.push_back(makeSection("__bss", "__DATA", 0, image.bssSize, 0, 4, 1));
    }

    std::vector<Bytes> commands;
    commands.push_back(makeSegment("__PAGEZERO", 0, kImageBase, 0, 0, 0, 0, {}));
    commands.push_back(makeSegment("__TEXT", kImageBase, 0, 0, 0, 5, 5, textSections));
    if (!dataSections.empty()) commands.push_back(makeSegment("__DATA", 0, 0, 0, 0, 3, 3, dataSections));
    commands.push_back(makeSegment("__LINKEDIT", 0, 0, 0, 0, 1, 1, {}));
    commands.push_back(makeStringCommand(kLoadDylinkerCommand, dylinkerPath));
    for (const auto &library : image.libraries) commands.push_back(makeDylibCommand(kLoadDylibCommand, library.installName));
    std::sort(image.rpaths.begin(), image.rpaths.end());
    image.rpaths.erase(std::unique(image.rpaths.begin(), image.rpaths.end()), image.rpaths.end());
    image.rpaths.erase(std::remove_if(image.rpaths.begin(), image.rpaths.end(),
                                     [](const std::filesystem::path &path) { return path.empty(); }),
                       image.rpaths.end());
    for (const auto &rpath : image.rpaths) {
        if (!rpath.empty()) commands.push_back(makeStringCommand(kRpathCommand, rpath.string()));
    }
    Bytes mainCommand(24, '\0');
    put32(mainCommand, 0, kMainCommand);
    put32(mainCommand, 4, 24);
    commands.push_back(mainCommand);
    Bytes versionCommand(24, '\0');
    put32(versionCommand, 0, kBuildVersionCommand);
    put32(versionCommand, 4, 24);
    put32(versionCommand, 8, 1);
    put32(versionCommand, 12, 0x000b0000);
    put32(versionCommand, 16, 0x000b0000);
    commands.push_back(versionCommand);
    Bytes dyldInfoCommand(48, '\0');
    put32(dyldInfoCommand, 0, kBindInfoCommand);
    put32(dyldInfoCommand, 4, 48);
    commands.push_back(dyldInfoCommand);
    Bytes symtabCommand(24, '\0');
    put32(symtabCommand, 0, 2);
    put32(symtabCommand, 4, 24);
    commands.push_back(symtabCommand);
    Bytes dysymtabCommand(80, '\0');
    put32(dysymtabCommand, 0, 0xb);
    put32(dysymtabCommand, 4, 80);
    commands.push_back(dysymtabCommand);
    Bytes codeSignatureCommand(16, '\0');
    put32(codeSignatureCommand, 0, kCodeSignatureCommand);
    put32(codeSignatureCommand, 4, 16);
    commands.push_back(codeSignatureCommand);

    uint32_t commandBytes = 0;
    for (const auto &command : commands) commandBytes += static_cast<uint32_t>(command.size());
    if (alignTo(kHeaderSize + commandBytes, 16) > kPageSize) {
        fail("Mach-O load commands exceed the supported header page");
    }
    uint32_t codeOffset = kPageSize;
    uint32_t dataOffset = alignTo(codeOffset + static_cast<uint32_t>(image.code.size()), kPageSize);
    uint32_t initializedDataSize = gotSize + static_cast<uint32_t>(image.data.size());
    uint32_t dataVmSize = alignTo(bssOffset + image.bssSize, kPageSize);
    for (const auto &item : image.imports) {
        uint64_t pointerAddress = kImageBase + dataOffset + item.pointerOffset;
        patchPage21(image.code, item.stubOffset, pointerAddress);
        patchPageoff12(image.code, item.stubOffset + 4, pointerAddress);
    }
    if (!dataSections.empty()) {
        Bytes dataSegment = commands[2];
        put64(dataSegment, 24, kImageBase + dataOffset);
        put64(dataSegment, 32, dataVmSize);
        put64(dataSegment, 40, dataOffset);
        put64(dataSegment, 48, initializedDataSize);
        commands[2] = std::move(dataSegment);
    }
    uint32_t linkeditOffset = alignTo(dataOffset + (dataSections.empty() ? 0 : dataVmSize), kPageSize);
    Bytes binds = makeBindOpcodes(image, 0);
    Bytes symbolTable;
    Bytes stringTable(1, '\0');
    std::unordered_map<std::string, uint32_t> stringOffsets;
    auto appendName = [&](const std::string &name) {
        auto found = stringOffsets.find(name);
        if (found != stringOffsets.end()) return found->second;
        uint32_t offset = static_cast<uint32_t>(stringTable.size());
        stringTable += name;
        stringTable.push_back('\0');
        stringOffsets[name] = offset;
        return offset;
    };
    uint32_t externalDefinitions = 0;
    std::vector<std::pair<std::string, uint64_t>> definitions = image.symbols;
    std::sort(definitions.begin(), definitions.end(),
              [](const auto &a, const auto &b) { return a.first < b.first; });
    for (const auto &definition : definitions) {
        uint64_t value = definition.second;
        bool isData = (value & kDataSymbolTag) != 0;
        bool isText = !isData;
        bool isBss = (value & kBssSymbolTag) != 0;
        uint8_t dataSection = static_cast<uint8_t>(2 + (gotSize != 0));
        uint8_t bssSection = static_cast<uint8_t>(dataSection + (!image.data.empty() ? 1 : 0));
        uint8_t section = isText ? 1 : isBss ? bssSection : dataSection;
        uint64_t symbolOffset = value & ~(kDataSymbolTag | kBssSymbolTag);
        uint64_t address = isText ? kImageBase + codeOffset + value :
                           kImageBase + dataOffset + (isBss ? bssOffset : gotSize) + symbolOffset;
        putNlist(symbolTable, appendName(definition.first), 0x0f, section, 0, address);
        ++externalDefinitions;
    }
    for (const auto &item : image.imports) {
        uint16_t ordinal = static_cast<uint16_t>(item.libraryIndex + 1);
        putNlist(symbolTable, appendName(item.symbol), 0x01, 0, ordinal, 0);
    }
    Bytes linkedit = binds;
    linkedit.resize(alignTo(static_cast<uint32_t>(linkedit.size()), 8), '\0');
    uint32_t symbolOffset = linkeditOffset + static_cast<uint32_t>(linkedit.size());
    uint32_t symbolCount = static_cast<uint32_t>(definitions.size() + image.imports.size());
    uint32_t stringOffset = symbolOffset + static_cast<uint32_t>(symbolTable.size());
    uint32_t stringSize = static_cast<uint32_t>(stringTable.size());
    linkedit += symbolTable;
    linkedit += stringTable;
    uint32_t signatureOffset = alignTo(linkeditOffset + static_cast<uint32_t>(linkedit.size()), 16);
    uint32_t pageSlots = (signatureOffset + kSignaturePageSize - 1) / kSignaturePageSize;
    uint32_t signatureSize = 20 + 48 + pageSlots * 32;

    put64(commands[1], 24, kImageBase);
    put64(commands[1], 32, dataOffset);
    put64(commands[1], 40, 0);
    put64(commands[1], 48, dataOffset);
    put64(commands[1], 72 + 32, kImageBase + codeOffset);
    put64(commands[1], 72 + 40, image.code.size());
    put32(commands[1], 72 + 48, codeOffset);
    if (!dataSections.empty()) {
        size_t dataCommandIndex = 2;
        put64(commands[dataCommandIndex], 24, kImageBase + dataOffset);
        put64(commands[dataCommandIndex], 32, dataVmSize);
        put64(commands[dataCommandIndex], 40, dataOffset);
        put64(commands[dataCommandIndex], 48, initializedDataSize);
        size_t sectionOffset = 72;
        if (gotSize) {
            put64(commands[dataCommandIndex], sectionOffset + 32, kImageBase + dataOffset);
            put64(commands[dataCommandIndex], sectionOffset + 40, gotSize);
            put32(commands[dataCommandIndex], sectionOffset + 48, dataOffset);
            sectionOffset += kSectionSize;
        }
        if (!image.data.empty()) {
            put64(commands[dataCommandIndex], sectionOffset + 32, kImageBase + dataOffset + gotSize);
            put64(commands[dataCommandIndex], sectionOffset + 40, image.data.size());
            put32(commands[dataCommandIndex], sectionOffset + 48, dataOffset + gotSize);
            sectionOffset += kSectionSize;
        }
        if (image.bssSize) {
            put64(commands[dataCommandIndex], sectionOffset + 32, kImageBase + dataOffset + bssOffset);
            put64(commands[dataCommandIndex], sectionOffset + 40, image.bssSize);
        }
    }
    size_t linkeditCommandIndex = dataSections.empty() ? 2 : 3;
    put64(commands[linkeditCommandIndex], 24, kImageBase + linkeditOffset);
    put64(commands[linkeditCommandIndex], 32, alignTo(signatureOffset + signatureSize - linkeditOffset, kPageSize));
    put64(commands[linkeditCommandIndex], 40, linkeditOffset);
    put64(commands[linkeditCommandIndex], 48, signatureOffset + signatureSize - linkeditOffset);
    size_t commandIndex = (dataSections.empty() ? 3 : 4) + 1;
    for (size_t i = 0; i < image.libraries.size(); ++i) ++commandIndex;
    commandIndex += image.rpaths.size();
    put64(commands[commandIndex], 8, codeOffset + image.entryOffset);

    uint32_t bindOffset = linkeditOffset;
    put32(commands[commandIndex + 2], 16, bindOffset);
    put32(commands[commandIndex + 2], 20, static_cast<uint32_t>(binds.size()));
    put32(commands[commandIndex + 3], 8, symbolOffset);
    put32(commands[commandIndex + 3], 12, symbolCount);
    put32(commands[commandIndex + 3], 16, stringOffset);
    put32(commands[commandIndex + 3], 20, stringSize);
    put32(commands[commandIndex + 4], 8, 0);
    put32(commands[commandIndex + 4], 12, 0);
    put32(commands[commandIndex + 4], 16, 0);
    put32(commands[commandIndex + 4], 20, externalDefinitions);
    put32(commands[commandIndex + 4], 24, externalDefinitions);
    put32(commands[commandIndex + 4], 28, static_cast<uint32_t>(image.imports.size()));
    put32(commands[commandIndex + 5], 8, signatureOffset);
    put32(commands[commandIndex + 5], 12, signatureSize);

    Bytes output(codeOffset, '\0');
    put32(output, 0, 0xfeedfacf);
    put32(output, 4, 0x0100000c);
    put32(output, 8, 0);
    put32(output, 12, 2);
    put32(output, 16, static_cast<uint32_t>(commands.size()));
    put32(output, 20, commandBytes);
    put32(output, 24, 0x00200084);
    size_t commandOffset = kHeaderSize;
    for (const auto &command : commands) {
        output.replace(commandOffset, command.size(), command);
        commandOffset += command.size();
    }
    output += image.code;
    output.resize(dataOffset, '\0');
    output += image.got;
    output += image.data;
    output.resize(linkeditOffset, '\0');
    output += linkedit;
    output.resize(signatureOffset, '\0');
    Bytes signedBytes = output;
    Bytes codeDirectory;
    appendBE32(codeDirectory, 0xfade0c02);
    appendBE32(codeDirectory, 0);
    appendBE32(codeDirectory, 0x20001);
    appendBE32(codeDirectory, 2);
    appendBE32(codeDirectory, 48);
    appendBE32(codeDirectory, 44);
    appendBE32(codeDirectory, 0);
    appendBE32(codeDirectory, pageSlots);
    appendBE32(codeDirectory, signatureOffset);
    codeDirectory.push_back(32);
    codeDirectory.push_back(2);
    codeDirectory.push_back(0);
    codeDirectory.push_back(12);
    appendBE32(codeDirectory, 0);
    codeDirectory += "rcc\0";
    codeDirectory.resize(48, '\0');
    for (uint32_t slot = 0; slot < pageSlots; ++slot) {
        size_t pageOffset = static_cast<size_t>(slot) * kSignaturePageSize;
        codeDirectory += sha256(signedBytes.substr(pageOffset, std::min<size_t>(
            kSignaturePageSize, signedBytes.size() - pageOffset)));
    }
    uint32_t codeDirectorySize = static_cast<uint32_t>(codeDirectory.size());
    codeDirectory[4] = static_cast<char>(codeDirectorySize >> 24);
    codeDirectory[5] = static_cast<char>(codeDirectorySize >> 16);
    codeDirectory[6] = static_cast<char>(codeDirectorySize >> 8);
    codeDirectory[7] = static_cast<char>(codeDirectorySize);
    Bytes signature;
    appendBE32(signature, 0xfade0cc0);
    appendBE32(signature, 20 + static_cast<uint32_t>(codeDirectory.size()));
    appendBE32(signature, 1);
    appendBE32(signature, 0);
    appendBE32(signature, 20);
    signature += codeDirectory;
    output += signature;
    return output;
}

}  // namespace rcc
