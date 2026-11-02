#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "objectformat/ObjectFile.h"

namespace pcc {

// A minimal, from-scratch COFF (Common Object File Format) object-file
// writer - one of several interchangeable ObjectFormatWriter
// implementations (see ../objectformat/ObjectFormat.h; this one is
// wrapped by ../objectformat/CoffObjectFormat.h) that every backend's
// own in-process assembler can produce a `--format=coff` object file
// through, without any backend ever touching real COFF byte layout
// itself. Not a general COFF writer: exactly what's needed to hold a
// real, separately-compiled object file with external symbols and
// relocations against them (real separate compilation,
// `--format=coff` from a single `.c` file - see IRBuilder.h's
// `allowExternal`) - one `.text` section of raw bytes, a symbol
// table (defined-vs-external entries), and a real relocation table.
// No `.data` section: every backend's own two-pass assembler already
// interleaves code and data into one flat address space the same way
// the real hardware sees it.
//
// COFF has no historical tie to any target this project supports
// (PDP-7 predates it entirely; real 16-bit 8086/DOS tooling used OMF,
// not COFF) - this is a deliberate, documented choice, the same
// spirit as other "documented deviation" choices already in this
// project (see docs/pdp-c-compiler.md's "COFF object files"). Every
// object written here uses a custom, non-registered Machine value
// (passed in by the caller - neither 0x1987 nor 0x0086, this
// project's own two examples, is a real COFF machine ID), which
// existing tooling's COFF readers - `objdump` included - reject
// outright as "not a valid object file" purely because of that
// unrecognized Machine field, even though every other field is laid
// out correctly; confirmed by patching a generated file's Machine
// field to 0x14c (IMAGE_FILE_MACHINE_I386) and re-running `objdump
// -h`/`-t`, which then parses it cleanly. Structural correctness is
// instead verified with a small ad-hoc script that parses the format
// directly (field offsets/sizes against this file's own layout) and
// by a real round trip through this project's own linker.
//
// `ObjSymbol`/`ObjRelocation`/`ObjectFile` are the format-neutral
// object model every supported object-file format shares - see
// ../objectformat/ObjectFile.h's own header comment; a relocation's
// `type` here is a raw, per-assembler-documented code (pdp7's
// OR/`+`/`-` word-combine kinds; 8086/z80's absolute/relative 16-bit
// patch kinds - see each assembler's own `Relocation`/relocation-kind
// comment for the exact meanings), this project's own custom values,
// the same spirit as the custom `Machine` field, since nothing REAL
// COFF defines matches any of these targets' own addressing.
// `symbolName` must name an entry already present in the `symbols`
// list passed to `writeCoffObject` (defined or not) - the writer
// resolves it to a real `SymbolTableIndex` itself.
//
// `machineType` is a raw 16-bit value written verbatim into the COFF
// file header's Machine field - see each assembler's own call site
// for which (non-standard, but documented) value it passes.
//
// `dataSize` (default 0, meaningless for every von-Neumann target)
// is a genuinely Harvard target's own ObjectFile::dataSize - see that
// field's own comment - written out as a second, always-present
// ".bss" section's own VirtualSize (no raw bytes - this is a pure
// reservation, never a byte blob), with an `isData` symbol's own
// SectionNumber pointing at that section instead of ".text"'s.
std::string writeCoffObject(uint16_t machineType, const std::vector<uint8_t> &textBytes,
                             const std::vector<ObjSymbol> &symbols,
                             const std::vector<ObjRelocation> &relocations = {}, uint32_t dataSize = 0);

}  // namespace pcc
