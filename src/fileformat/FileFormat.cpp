#include "FileFormat.h"

#include <cstdint>
#include <unordered_set>

#include "fileformat/aout/AoutFormat.h"
#include "fileformat/coff/CoffFormat.h"
#include "fileformat/elf/ElfFormat.h"
#include "fileformat/macho/MachOFormat.h"
#include "fileformat/flat/FlatFormat.h"
#include "fileformat/raw/RawFormat.h"
#include "fileformat/rim/RimFormat.h"
#include "fileformat/ptr/PtrFormat.h"
#include "fileformat/a7out/A7outFormat.h"
#include "fileformat/list/ListFormat.h"

namespace pcc {

std::unique_ptr<FileFormatWriter> makeFileFormatWriter(FileFormatKind kind) {
    switch (kind) {
        case FileFormatKind::Coff:
            return std::make_unique<CoffFormatWriter>();
        case FileFormatKind::Elf:
            return std::make_unique<ElfFormatWriter>();
        case FileFormatKind::MachO:
            return std::make_unique<MachOFormatWriter>();
        case FileFormatKind::Aout:
            return std::make_unique<AoutFormatWriter>();
        case FileFormatKind::Flat:
            return std::make_unique<FlatFormatWriter>();
        case FileFormatKind::Raw:
            return std::make_unique<RawFormatWriter>();
        case FileFormatKind::Rim:
            return std::make_unique<RimFormatWriter>();
        case FileFormatKind::Ptr:
            return std::make_unique<PtrFormatWriter>();
        case FileFormatKind::A7out:
            return std::make_unique<A7outFormatWriter>();
        case FileFormatKind::List:
            return std::make_unique<ListFormatWriter>();
    }
    return nullptr;  // unreachable - every enumerator handled above
}

std::unique_ptr<FileFormatReader> makeFileFormatReader(FileFormatKind kind) {
    switch (kind) {
        case FileFormatKind::Coff:
            return std::make_unique<CoffFormatReader>();
        case FileFormatKind::Elf:
            return std::make_unique<ElfFormatReader>();
        case FileFormatKind::MachO:
            return std::make_unique<MachOFormatReader>();
        case FileFormatKind::Aout:
            return std::make_unique<AoutFormatReader>();
        case FileFormatKind::Flat:
            return std::make_unique<FlatFormatReader>();
        case FileFormatKind::Raw:
            return std::make_unique<RawFormatReader>();
        case FileFormatKind::Rim:
            return std::make_unique<RimFormatReader>();
        case FileFormatKind::Ptr:
            return std::make_unique<PtrFormatReader>();
        case FileFormatKind::A7out:
            return std::make_unique<A7outFormatReader>();
        case FileFormatKind::List:
            return std::make_unique<ListFormatReader>();
    }
    return nullptr;  // unreachable - every enumerator handled above
}

std::optional<FileFormatKind> fileFormatKindFromName(const std::string &name) {
    if (name == "coff") return FileFormatKind::Coff;
    if (name == "elf") return FileFormatKind::Elf;
    if (name == "macho") return FileFormatKind::MachO;
    if (name == "aout") return FileFormatKind::Aout;
    if (name == "objraw") return FileFormatKind::Flat;
    if (name == "flat") return FileFormatKind::Flat;
    if (name == "raw") return FileFormatKind::Raw;
    if (name == "rim") return FileFormatKind::Rim;
    if (name == "ptr") return FileFormatKind::Ptr;
    if (name == "a7out") return FileFormatKind::A7out;
    if (name == "list") return FileFormatKind::List;
    return std::nullopt;
}

std::string fileFormatName(FileFormatKind kind) {
    switch (kind) {
        case FileFormatKind::Coff:
            return "coff";
        case FileFormatKind::Elf:
            return "elf";
        case FileFormatKind::MachO:
            return "macho";
        case FileFormatKind::Aout:
            return "aout";
        case FileFormatKind::Flat:
            return "flat";
        case FileFormatKind::Raw:
            return "raw";
        case FileFormatKind::Rim:
            return "rim";
        case FileFormatKind::Ptr:
            return "ptr";
        case FileFormatKind::A7out:
            return "a7out";
        case FileFormatKind::List:
            return "list";
    }
    return "";  // unreachable - every enumerator handled above
}

// Real magic numbers for the four formats that have one - checked in
// this order so a file that happens to start with a byte sequence
// one format's reader would accept but that's actually a different,
// more specific format's own magic never gets misidentified (COFF's
// own Machine-field-as-magic is the weakest signal here, so it's
// checked last among the four real ones).
FileFormatKind detectFileFormat(const std::string &bytes) {
    if (bytes.size() >= 4 && static_cast<uint8_t>(bytes[0]) == 0x7F && bytes[1] == 'E' && bytes[2] == 'L' &&
        bytes[3] == 'F') {
        return FileFormatKind::Elf;
    }
    if (bytes.size() >= 4) {
        uint32_t magic = static_cast<uint8_t>(bytes[0]) | (static_cast<uint8_t>(bytes[1]) << 8) |
                          (static_cast<uint8_t>(bytes[2]) << 16) |
                          (static_cast<uint32_t>(static_cast<uint8_t>(bytes[3])) << 24);
        if (magic == 0xFEEDFACE) return FileFormatKind::MachO;
    }
    if (bytes.size() >= 4) {
        uint32_t magic = static_cast<uint8_t>(bytes[0]) | (static_cast<uint8_t>(bytes[1]) << 8) |
                          (static_cast<uint8_t>(bytes[2]) << 16) |
                          (static_cast<uint32_t>(static_cast<uint8_t>(bytes[3])) << 24);
        if (magic == 0407) return FileFormatKind::Aout;
    }
    // COFF has no real magic number of its own (its first field is
    // just a Machine ID - see coff/CoffFormat.h's own header comment),
    // so this is a registry, not real format-sniffing: every target's
    // own `k<Target>CoffMachine` (see e.g.
    // ../codegen/m6502/M6502Assembler.h) must be added here, or its
    // own `--format=coff` objects silently fall through to the `Flat`
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
        if (kKnownCoffMachines.count(machine)) return FileFormatKind::Coff;
    }
    return FileFormatKind::Flat;
}

}  // namespace pcc
