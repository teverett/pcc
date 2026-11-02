#include "ObjectFormat.h"

#include <cstdint>
#include <unordered_set>

#include "objectformat/aout/AoutObjectFormat.h"
#include "objectformat/coff/CoffObjectFormat.h"
#include "objectformat/elf/ElfObjectFormat.h"
#include "objectformat/macho/MachOObjectFormat.h"
#include "objectformat/raw/RawObjectFormat.h"

namespace pcc {

std::unique_ptr<ObjectFormatWriter> makeObjectFormatWriter(ObjectFormatKind kind) {
    switch (kind) {
        case ObjectFormatKind::Coff:
            return std::make_unique<CoffObjectFormatWriter>();
        case ObjectFormatKind::Elf:
            return std::make_unique<ElfObjectFormatWriter>();
        case ObjectFormatKind::MachO:
            return std::make_unique<MachOObjectFormatWriter>();
        case ObjectFormatKind::Aout:
            return std::make_unique<AoutObjectFormatWriter>();
        case ObjectFormatKind::Raw:
            return std::make_unique<RawObjectFormatWriter>();
    }
    return nullptr;  // unreachable - every enumerator handled above
}

std::unique_ptr<ObjectFormatReader> makeObjectFormatReader(ObjectFormatKind kind) {
    switch (kind) {
        case ObjectFormatKind::Coff:
            return std::make_unique<CoffObjectFormatReader>();
        case ObjectFormatKind::Elf:
            return std::make_unique<ElfObjectFormatReader>();
        case ObjectFormatKind::MachO:
            return std::make_unique<MachOObjectFormatReader>();
        case ObjectFormatKind::Aout:
            return std::make_unique<AoutObjectFormatReader>();
        case ObjectFormatKind::Raw:
            return std::make_unique<RawObjectFormatReader>();
    }
    return nullptr;  // unreachable - every enumerator handled above
}

std::optional<ObjectFormatKind> objectFormatKindFromName(const std::string &name) {
    if (name == "coff") return ObjectFormatKind::Coff;
    if (name == "elf") return ObjectFormatKind::Elf;
    if (name == "macho") return ObjectFormatKind::MachO;
    if (name == "aout") return ObjectFormatKind::Aout;
    if (name == "objraw") return ObjectFormatKind::Raw;
    return std::nullopt;
}

std::string objectFormatName(ObjectFormatKind kind) {
    switch (kind) {
        case ObjectFormatKind::Coff:
            return "coff";
        case ObjectFormatKind::Elf:
            return "elf";
        case ObjectFormatKind::MachO:
            return "macho";
        case ObjectFormatKind::Aout:
            return "aout";
        case ObjectFormatKind::Raw:
            return "objraw";
    }
    return "";  // unreachable - every enumerator handled above
}

// Real magic numbers for the four formats that have one - checked in
// this order so a file that happens to start with a byte sequence
// one format's reader would accept but that's actually a different,
// more specific format's own magic never gets misidentified (COFF's
// own Machine-field-as-magic is the weakest signal here, so it's
// checked last among the four real ones).
ObjectFormatKind detectObjectFormat(const std::string &bytes) {
    if (bytes.size() >= 4 && static_cast<uint8_t>(bytes[0]) == 0x7F && bytes[1] == 'E' && bytes[2] == 'L' &&
        bytes[3] == 'F') {
        return ObjectFormatKind::Elf;
    }
    if (bytes.size() >= 4) {
        uint32_t magic = static_cast<uint8_t>(bytes[0]) | (static_cast<uint8_t>(bytes[1]) << 8) |
                          (static_cast<uint8_t>(bytes[2]) << 16) | (static_cast<uint32_t>(static_cast<uint8_t>(bytes[3])) << 24);
        if (magic == 0xFEEDFACE) return ObjectFormatKind::MachO;
    }
    if (bytes.size() >= 4) {
        uint32_t magic = static_cast<uint8_t>(bytes[0]) | (static_cast<uint8_t>(bytes[1]) << 8) |
                          (static_cast<uint8_t>(bytes[2]) << 16) | (static_cast<uint32_t>(static_cast<uint8_t>(bytes[3])) << 24);
        if (magic == 0407) return ObjectFormatKind::Aout;
    }
    // COFF has no real magic number of its own (its first field is
    // just a Machine ID - see CoffWriter.h's own header comment), so
    // this is a registry, not real format-sniffing: every target's
    // own `k<Target>CoffMachine` (see e.g.
    // ../codegen/m6502/M6502Assembler.h) must be added here, or its
    // own `--format=coff` objects silently fall through to the `Raw`
    // fallback below and lose their entire symbol table.
    static const std::unordered_set<uint16_t> kKnownCoffMachines = {
        0x1987,  // pdp7
        0x0086,  // 8086/8088
        0x5A38,  // z80
        0x6502,  // 6502
        0xC502,  // 65c02
        0x8080,  // 8080
        0x5708,  // stm8
        0x8008,  // 8008
        0x6328,  // mc68328
        0x6800,  // m68k
        0x16C3,  // m16c
        0x1121,  // pdp11
        0x3386,  // i386
        0xAA64,  // armv8a (AArch64)
        0xA032,  // armv7a (A32)
        0xA007,  // cortex-m3/m4/m7 (ArmM, Thumb/Thumb-2)
        0x3332,  // stm32
        0x4000,  // r10000/r4000 (MIPS)
        0x5043,  // powerpc
        0x8051,  // 8051/8052/8031/8032 (mcs51)
        0xFC18,  // pic18
        0x16F8,  // pic16
    };
    if (bytes.size() >= 2) {
        uint16_t machine = static_cast<uint8_t>(bytes[0]) | (static_cast<uint8_t>(bytes[1]) << 8);
        if (kKnownCoffMachines.count(machine)) return ObjectFormatKind::Coff;
    }
    return ObjectFormatKind::Raw;
}

}  // namespace pcc
