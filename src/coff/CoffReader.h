#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "CoffWriter.h"

namespace pcc {

// The linker's own input: reads back exactly the byte layout
// CoffWriter.h's `writeCoffObject` produces - the mirror image of
// that function, not a general COFF reader (same "narrow, documented
// deviation" scope as the writer itself - see that file's header
// comment). `CoffRelocation::symbolName` here is `symbols[i].name`
// for whichever `symbols` entry the real `SymbolTableIndex` the file
// encodes names - resolved back to a name immediately on read, since
// the linker (unlike the writer) only ever wants to look relocations
// up by name, never by index.
struct CoffObject {
    uint16_t machineType = 0;
    std::vector<uint8_t> textBytes;
    std::vector<CoffSymbol> symbols;
    std::vector<CoffRelocation> relocations;
};

// Throws std::runtime_error if `bytes` isn't a well-formed object file
// in this project's own documented layout (truncated, a relocation or
// symbol-table field pointing outside the buffer, etc).
CoffObject readCoffObject(const std::string &bytes);

}  // namespace pcc
