#include "FileFormat.h"

#include <cstdint>
#include <unordered_set>

#include "fileformat/aout/AoutFormat.h"
#include "fileformat/coff/CoffFormat.h"
#include "fileformat/elf/ElfFormat.h"
#include "fileformat/macho/MachOFormat.h"
#include "fileformat/raw/RawFormat.h"
#include "fileformat/rim/RimFormat.h"
#include "fileformat/ptr/PtrFormat.h"
#include "fileformat/a7out/A7outFormat.h"
#include "fileformat/list/ListFormat.h"
#include "fileformat/dos33/Dos33Format.h"
#include "fileformat/prodos/ProDosFormat.h"
#include "fileformat/cmd/CmdFormat.h"
#include "fileformat/colecorom/ColecoRomFormat.h"
#include "fileformat/decb/DecbFormat.h"
#include "fileformat/prg/PrgFormat.h"

namespace rcc {

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
        case FileFormatKind::Dos33:
            return std::make_unique<Dos33FormatWriter>();
        case FileFormatKind::ProDos:
            return std::make_unique<ProDosFormatWriter>();
        case FileFormatKind::Cmd:
            return std::make_unique<CmdFormatWriter>();
        case FileFormatKind::ColecoRom:
            return std::make_unique<ColecoRomFormatWriter>();
        case FileFormatKind::Decb:
            return std::make_unique<DecbFormatWriter>();
        case FileFormatKind::Prg:
            return std::make_unique<PrgFormatWriter>();
    }
    return nullptr;
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
        case FileFormatKind::Dos33:
            return std::make_unique<Dos33FormatReader>();
        case FileFormatKind::ProDos:
            return std::make_unique<ProDosFormatReader>();
        case FileFormatKind::Cmd:
            return std::make_unique<CmdFormatReader>();
        case FileFormatKind::ColecoRom:
            return std::make_unique<ColecoRomFormatReader>();
        case FileFormatKind::Decb:
            return std::make_unique<DecbFormatReader>();
        case FileFormatKind::Prg:
            return std::make_unique<PrgFormatReader>();
    }
    return nullptr;
}

std::optional<FileFormatKind> fileFormatKindFromName(const std::string &name) {
    if (name == "coff") return FileFormatKind::Coff;
    if (name == "elf") return FileFormatKind::Elf;
    if (name == "macho") return FileFormatKind::MachO;
    if (name == "aout") return FileFormatKind::Aout;
    if (name == "objraw") return FileFormatKind::Raw;
    if (name == "raw") return FileFormatKind::Raw;
    if (name == "rim") return FileFormatKind::Rim;
    if (name == "ptr") return FileFormatKind::Ptr;
    if (name == "a7out") return FileFormatKind::A7out;
    if (name == "list") return FileFormatKind::List;
    if (name == "dos33") return FileFormatKind::Dos33;
    if (name == "prodos") return FileFormatKind::ProDos;
    if (name == "cmd") return FileFormatKind::Cmd;
    if (name == "colecorom") return FileFormatKind::ColecoRom;
    if (name == "decb") return FileFormatKind::Decb;
    if (name == "prg") return FileFormatKind::Prg;
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
        case FileFormatKind::Dos33:
            return "dos33";
        case FileFormatKind::ProDos:
            return "prodos";
        case FileFormatKind::Cmd:
            return "cmd";
        case FileFormatKind::ColecoRom:
            return "colecorom";
        case FileFormatKind::Decb:
            return "decb";
        case FileFormatKind::Prg:
            return "prg";
    }
    return "";
}

// Checked in this order so COFF's Machine-field-as-magic (weakest signal) never shadows the
// other three formats' real magic numbers.
FileFormatKind detectFileFormat(const std::string &bytes) {
    if (bytes.size() >= 4 && static_cast<uint8_t>(bytes[0]) == 0x7F && bytes[1] == 'E' && bytes[2] == 'L' &&
        bytes[3] == 'F') {
        return FileFormatKind::Elf;
    }
    if (bytes.size() >= 4) {
        uint32_t magic = static_cast<uint8_t>(bytes[0]) | (static_cast<uint8_t>(bytes[1]) << 8) |
                          (static_cast<uint8_t>(bytes[2]) << 16) |
                          (static_cast<uint32_t>(static_cast<uint8_t>(bytes[3])) << 24);
        if (magic == 0xFEEDFACE || magic == 0xFEEDFACF) return FileFormatKind::MachO;
    }
    if (bytes.size() >= 4) {
        uint32_t magic = static_cast<uint8_t>(bytes[0]) | (static_cast<uint8_t>(bytes[1]) << 8) |
                          (static_cast<uint8_t>(bytes[2]) << 16) |
                          (static_cast<uint32_t>(static_cast<uint8_t>(bytes[3])) << 24);
        if (magic == 0407) return FileFormatKind::Aout;
    }
    // COFF has no magic number, only a Machine ID; every target's k<Target>CoffMachine must be
    // listed here or its --format=coff objects fall through to the Raw fallback below.
    static const std::unordered_set<uint16_t> kKnownCoffMachines = {
        0x014C,  // standard Windows i386 COFF
        0x1987,  // pdp7
        0x0086,  // 8086/8088
        0x0D86,  // 8086-dos
        0x5A38,  // z80
        0x6809,  // 6809
        0x6502,  // 6502
        0xC502,  // 65c02
        0x8080,  // 8080
        0x5708,  // stm8
        0x8008,  // 8008
        0x6328,  // mc68328
        0x6800,  // m68k
        0x16C3,  // m16c
        0x1121,  // pdp11
        0x1010,  // pdp10
        0x3386,  // i386
        0xAA64,  // armv8a (AArch64)
        0xA032,  // armv7a (A32)
        0xA007,  // cortex-m3/m4/m7 (ArmM, Thumb/Thumb-2)
        0x3332,  // stm32
        0x4000,  // r10000/r4000 (MIPS)
        0x5043,  // powerpc
        0x5350,  // sparc
        0x5641,  // vax
        0x0184,  // alpha
        0x4156,  // avr
        0x5A74,  // saturn
        0x6713,  // tms320c6713 (currently unused - this target has no coff/object support yet,
                 // see Tms320C6713Codegen.h's own header comment; listed here only to satisfy
                 // every target's own reserved-machine-ID convention)
        0x8051,  // 8051/8052/8031/8032 (mcs51)
        0xFC18,  // pic18
        0x16F8,  // pic16
    };
    if (bytes.size() >= 2) {
        uint16_t machine = static_cast<uint8_t>(bytes[0]) | (static_cast<uint8_t>(bytes[1]) << 8);
        if (kKnownCoffMachines.count(machine)) return FileFormatKind::Coff;
    }
    return FileFormatKind::Raw;
}

}  // namespace rcc
