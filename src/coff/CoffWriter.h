#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace pcc {

// A minimal, from-scratch COFF (Common Object File Format) object-file
// writer, shared by both backends' own in-process assemblers
// (pdp7/Pdp7Assembler.cpp, codegen/x86_8086/I8086Assembler.cpp) for
// their `--format=coff` output, and read back by `CoffReader.h` (the
// linker's own input). Not a general COFF writer: exactly what's
// needed to hold either an already-fully-resolved program (no
// relocations at all - the original, single-file-program use case)
// *or* a real, separately-compiled object file with external
// symbols and relocations against them (real separate compilation,
// `--format=coff` from a single `.c` file - see IRBuilder.h's
// `allowExternal`) - one `.text` section of raw bytes, a symbol
// table (now with real defined-vs-external entries), and a real
// relocation table. No `.data` section: both backends' own two-pass
// assemblers already interleave code and data into one flat address
// space the same way the real hardware sees it.
//
// COFF has no historical tie to either target (PDP-7 predates it
// entirely; real 16-bit 8086/DOS tooling used OMF, not COFF) - this
// is a deliberate, documented choice for both, the same spirit as
// other "documented deviation" choices already in this project (see
// docs/pdp-c-compiler.md's "COFF object files"). Both assemblers write
// a custom, non-registered Machine value (neither 0x1987 nor 0x0086
// is a real COFF machine ID), which existing tooling's COFF readers -
// `objdump` included - reject outright as "not a valid object file"
// purely because of that unrecognized Machine field, even though
// every other field is laid out correctly; confirmed by patching a
// generated file's Machine field to 0x14c (IMAGE_FILE_MACHINE_I386)
// and re-running `objdump -h`/`-t`, which then parses it cleanly.
// Structural correctness is instead verified with a small ad-hoc
// script that parses the format directly (field offsets/sizes against
// this file's own layout) and, now that `CoffReader.h` exists, by a
// real round trip through this project's own linker.
struct CoffSymbol {
    std::string name;
    uint32_t value = 0;    // offset into the one .text section - meaningless unless isDefined
    bool isDefined = true;  // false = external/undefined (real COFF's own SectionNumber=0
                             // convention for "resolve this elsewhere" - not a custom deviation)
};

// One `IMAGE_RELOCATION`-shaped fixup: at `offset` bytes into `.text`,
// patch in `symbolName`'s (once resolved) value, combined according
// to `type` - a raw, per-assembler-documented code (pdp7's OR/`+`/`-`
// word-combine kinds; 8086's absolute/relative 16-bit patch kinds;
// see each assembler's own `Relocation`/relocation-kind comment for
// the exact meanings) - this project's own custom values, the same
// spirit as the custom `Machine` field, since nothing REAL COFF
// defines matches either target's own addressing. `symbolName` must
// name an entry already present in the `symbols` list passed to
// `writeCoffObject` (defined or not) - the writer resolves it to a
// real `SymbolTableIndex` itself.
struct CoffRelocation {
    uint32_t offset;
    std::string symbolName;
    uint16_t type;
};

// `machineType` is a raw 16-bit value written verbatim into the COFF
// file header's Machine field - see each assembler's own call site
// for which (non-standard, but documented) value it passes.
std::string writeCoffObject(uint16_t machineType, const std::vector<uint8_t> &textBytes,
                             const std::vector<CoffSymbol> &symbols,
                             const std::vector<CoffRelocation> &relocations = {});

}  // namespace pcc
