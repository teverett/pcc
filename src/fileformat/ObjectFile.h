#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace rcc {

// `value` is a byte offset from `.text` start (absolute address once linked is `base + value`),
// except when `isData` is set. `isDefined=false` is a forward reference a linker must resolve
// (COFF SectionNumber=0 convention).
//
// `isData` marks a symbol outside `.text`. Before linking, Harvard targets use a byte offset into
// the data-space reservation (`ObjectFile::dataSize`); Mach-O uses an offset into `dataBytes`, or
// into `dataSize` when `isBss` is set. `isBss` distinguishes zero-fill storage from initialized
// bytes. Once linked, Harvard data-symbol values are already absolute.
struct ObjSymbol {
    std::string name;
    uint32_t value = 0;
    bool isDefined = true;
    bool isData = false;
    bool isBss = false;
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
// `textBytes` and `dataBytes` hold the code and initialized data section contents where the
// object format keeps them separate. `dataSize` reserves zero-fill data space; `dataBytes`
// contains initialized data and is empty when the format has no separate data section.
//
// `unitBits`/`base` are meaningless pre-link (`.text` is always byte-addressed from `org 0`
// regardless of target). Once linked, `unitBits` is the real hardware addressable-unit width
// (pdp7: 18, packed as 3 bytes/word big-endian), and `base` is the load address of `.text` byte/
// word 0. A linked `ObjSymbol::value` stays relative to `.text` start, not absolute (add `base`
// yourself), except for an `isData` symbol, whose value is already absolute.
struct ObjectFile {
    uint16_t machineType = 0;
    std::vector<uint8_t> textBytes;
    std::vector<uint8_t> dataBytes;
    uint32_t dataSize = 0;
    std::vector<ObjSymbol> symbols;
    std::vector<ObjRelocation> relocations;
    uint32_t unitBits = 8;
    uint32_t base = 0;
    uint32_t textAlignment = 1;
};

}  // namespace rcc
