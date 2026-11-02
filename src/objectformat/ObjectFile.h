#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace pcc {

// One entry in an object file's own symbol table: a defined symbol's
// own value is normally a byte offset from the start of `.text`; an
// undefined (`isDefined=false`) one is a forward reference a linker
// must resolve against some *other* object's own definition of the
// same name (real COFF's own SectionNumber=0 convention for "resolve
// this elsewhere" - not a custom deviation, see CoffObjectFormat.h).
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
// own byte offsets have no relationship to at all, resolved by
// GenericLinker.h's own second, parallel layout pass (see that
// header's own `dataBaseAddress` comment). Always `false` (and
// meaningless to check) for every other, von-Neumann target.
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
// never by this object-file model itself, and every object-format
// reader/writer (see ObjectFormat.h) carries it through completely
// opaquely.
struct ObjRelocation {
    uint32_t offset = 0;
    std::string symbolName;
    uint16_t type = 0;
};

// A single compiled translation unit, before linking - the same
// pieces of information (code bytes, a data-space reservation size,
// symbol table, relocations) every supported object-file format
// (COFF/ELF/Mach-O/a.out/raw) can both represent and reconstruct
// exactly. This is the one model a codegen backend's own assembler
// builds, once, per compile (see ../codegen/Codegen.h's own
// `objectMode`) - a backend's own code never touches a real COFF/ELF/
// Mach-O/a.out byte layout at all; only the ObjectFormatWriter/
// ObjectFormatReader pair chosen for the requested `--format` (see
// ObjectFormat.h) does that, on this same neutral struct.
//
// `.text` is the only section with real bytes in it: every von-
// Neumann target's own two-pass assembler already interleaves code
// and data into that one flat address space the same way its real
// hardware sees it (see CoffObjectFormat.h's own header comment).
// `dataSize` is `.text`'s own Harvard-architecture counterpart - the
// number of bytes this object's own compile needs reserved in a
// second, separate data address space (always `0` for a von-Neumann
// target, which never touches this field) - see `ObjSymbol::isData`'s
// own comment above for what a symbol living there means, and
// GenericLinker.h's own `dataBaseAddress` for how a linker actually
// allocates it across every merged object. Unlike `.text`, this is a
// pure *reservation*, never a byte blob to copy into the linked image
// at all: real Harvard data memory is always RAM, with no "initial
// contents" of its own to carry (see e.g. MCS51Codegen.h's own header
// comment on why a global's own initializer has to run as code at
// startup instead).
struct ObjectFile {
    uint16_t machineType = 0;
    std::vector<uint8_t> textBytes;
    uint32_t dataSize = 0;
    std::vector<ObjSymbol> symbols;
    std::vector<ObjRelocation> relocations;
};

}  // namespace pcc
