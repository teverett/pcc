#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace pcc {

// One entry in an object file's own symbol table: a defined symbol's
// own value is normally a byte offset from the start of `.text`
// (including once fully linked - see ObjectFile::base's own comment
// below for why a linked symbol's *absolute* address is `base +
// value`, not `value` alone); an undefined (`isDefined=false`) one is
// a forward reference a linker must resolve against some *other*
// object's own definition of the same name (real COFF's own
// SectionNumber=0 convention for "resolve this elsewhere" - not a
// custom deviation, see fileformat/coff/CoffFormat.h).
//
// `isData` is only ever set by a genuinely Harvard-architecture
// backend (separate code/data address spaces reached over separate
// buses - MCS-51's XDATA, PIC16/PIC18's file-register/Access-Bank
// space; see each one's own Codegen.h header comment) whose own
// variable storage can't live inside `.text` the way every von-Neumann
// target's own global/local does. A symbol with `isData=true` has its
// `value` interpreted as a byte offset into this object's own *data*-
// space reservation (see `ObjectFile::dataSize` below) instead of an
// offset into `.text` - a second, independent address space `.text`'s
// own byte offsets have no relationship to at all. Once fully linked,
// such a symbol's `value` is instead the real, final, absolute data-
// space address directly (no `base` added - see `base`'s own comment
// on why code and data addresses are handled differently once
// linked). Resolved pre-link by GenericLinker.h's own second,
// parallel layout pass (see that header's own `dataBaseAddress`
// comment). Always `false` (and meaningless to check) for every
// other, von-Neumann target.
struct ObjSymbol {
    std::string name;
    uint32_t value = 0;
    bool isDefined = true;
    bool isData = false;
};

// One place in `.text` that needs patching once `symbolName`'s real
// address is known. `type` is a backend's own small integer
// relocation-kind code (e.g. "16-bit absolute" vs "16-bit relative to
// the next instruction", or pdp7's own OR/`+`/`-` word-combine kinds -
// see Pdp7Assembler.h's own `kPdp7RelocOr` etc.); its meaning is
// defined by whichever linker (see ../linker/Linker.h) interprets it,
// never by this object-file model itself, and every file-format
// reader/writer (see FileFormat.h) carries it through completely
// opaquely. Always empty once fully linked (see `ObjectFile::base`'s
// own comment) - every relocation a TargetLinker's own merge() sees
// has, by definition, already been patched.
struct ObjRelocation {
    uint32_t offset = 0;
    std::string symbolName;
    uint16_t type = 0;
};

// One compiled translation unit - or, once linked, one whole program -
// the same shape either way: the same pieces of information (code
// bytes, a data-space reservation size, symbol table, relocations)
// every supported file format (COFF/ELF/Mach-O/a.out/raw/flat, and
// pdp7's own rim/ptr/a7out/list) can both represent and reconstruct
// exactly. This is the one model a codegen backend's own assembler
// builds, once, per compile (see ../codegen/Codegen.h's own
// `objectMode`), and the one model a TargetLinker's own merge() (see
// ../linker/Linker.h) builds, once, per link - a backend or linker
// never touches a real COFF/ELF/Mach-O/a.out/etc. byte layout
// directly; only the FileFormatWriter/FileFormatReader pair chosen
// for the requested `--format`/`--link --format` (see FileFormat.h)
// does that, on this same neutral struct, for either stage. A linked
// ObjectFile is distinguished only by having no relocations left (see
// ObjRelocation's own comment) and every symbol `isDefined`.
//
// `.text` is the only section with real bytes in it: every von-
// Neumann target's own two-pass assembler already interleaves code
// and data into that one flat address space the same way its real
// hardware sees it (see fileformat/coff/CoffFormat.h's own header
// comment). `dataSize` is `.text`'s own Harvard-architecture
// counterpart - the number of bytes this object's own compile needs
// reserved in a second, separate data address space (always `0` for
// a von-Neumann target, which never touches this field, and also `0`
// once linked - a linked program's own data space is already fully
// resolved into real, final addresses, with nothing left to reserve;
// see `ObjSymbol::isData`'s own comment above) - see
// `ObjSymbol::isData`'s own comment above for what a symbol living
// there means, and GenericLinker.h's own `dataBaseAddress` for how a
// linker actually allocates it across every merged pre-link object.
// Unlike `.text`, this is a pure *reservation*, never a byte blob to
// copy into any image at all: real Harvard data memory is always RAM,
// with no "initial contents" of its own to carry (see e.g.
// MCS51Codegen.h's own header comment on why a global's own
// initializer has to run as code at startup instead).
//
// `unitBits`/`base` are both meaningless (left at their defaults, `8`/
// `0`) before linking - a not-yet-linked object's own `.text` is
// always byte-addressed from its own `org 0` (see e.g.
// Z80Assembler.cpp's own `allowExternal_`), regardless of what this
// target's own real hardware addressable-unit size eventually turns
// out to be once GenericLinker.h's own `addressUnitBytes` enters the
// picture at link time. Once linked, `unitBits` is this target's own
// real hardware addressable-unit width in bits (`8` for a byte-
// addressed target; pdp7's own 18, carried through `.text` as 3
// packed bytes per word, big-endian - same packing a not-yet-linked
// pdp7 object's own `.text` already uses, see Pdp7Assembler.cpp's own
// `buildObjectFile()`, so a linked ObjectFile's `.text` needs no
// different packing convention at all), and `base` is the real
// address `.text` byte/word 0 loads at. A linked symbol's own
// `ObjSymbol::value` stays relative to `.text`'s own start exactly
// like a not-yet-linked one's (not an absolute address) - a
// consumer that wants the real, final address adds `base` itself
// (see e.g. fileformat/a7out/A7outFormat.cpp) - except a `isData`
// symbol's `value`, which is already the real, final, absolute data-
// space address with no `base` to add (data lives in a completely
// separate address space `base`/`.text` says nothing about - see
// `ObjSymbol::isData`'s own comment).
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
