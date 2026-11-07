#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace pcc {

// `value` is a byte offset from `.text` start (absolute address once linked is `base + value`),
// except when `isData` is set. `isDefined=false` is a forward reference a linker must resolve
// (COFF SectionNumber=0 convention).
//
// `isData` marks a Harvard-architecture backend's variable (MCS-51 XDATA, PIC16/PIC18
// file-register space) living in a separate data address space, not `.text`. Its `value` is a
// byte offset into that data-space reservation (`ObjectFile::dataSize`) pre-link, and the final
// absolute data-space address once linked (no `base` added). Always `false` for von-Neumann targets.
struct ObjSymbol {
    std::string name;
    uint32_t value = 0;
    bool isDefined = true;
    bool isData = false;
};

// A place in `.text` needing a patch once `symbolName`'s address is known. `type` is a
// backend-defined relocation-kind code, opaque to this struct and to file-format readers/writers;
// only the linker (Linker.h) interprets it. Empty once linked.
struct ObjRelocation {
    uint32_t offset = 0;
    std::string symbolName;
    uint16_t type = 0;
};

// One compiled translation unit, or, once linked, one whole program: the neutral model every
// FileFormatWriter/FileFormatReader (FileFormat.h) and every TargetLinker (Linker.h) operates on
// instead of touching a real COFF/ELF/Mach-O/etc. layout directly. Linked is distinguished by
// having no relocations left and every symbol `isDefined`.
//
// `.text` is the only section with real bytes; von-Neumann targets interleave code and data into
// it. `dataSize` is the Harvard-architecture counterpart: bytes to reserve in a separate data
// address space (0 for von-Neumann, and 0 once linked since data addresses are then resolved).
// It's a pure reservation, never a byte blob — Harvard data memory is RAM with no initial
// contents.
//
// `unitBits`/`base` are meaningless pre-link (`.text` is always byte-addressed from `org 0`
// regardless of target). Once linked, `unitBits` is the real hardware addressable-unit width
// (pdp7: 18, packed as 3 bytes/word big-endian), and `base` is the load address of `.text` byte/
// word 0. A linked `ObjSymbol::value` stays relative to `.text` start, not absolute (add `base`
// yourself), except for an `isData` symbol, whose value is already absolute.
struct ObjectFile {
    uint16_t machineType = 0;
    std::vector<uint8_t> textBytes;
    uint32_t dataSize = 0;
    std::vector<ObjSymbol> symbols;
    std::vector<ObjRelocation> relocations;
    uint32_t unitBits = 8;
    uint32_t base = 0;
};

}  // namespace pcc
