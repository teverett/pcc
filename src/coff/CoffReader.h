#pragma once

#include <string>

#include "objectformat/ObjectFile.h"

namespace pcc {

// The read side of CoffWriter.h - reads back exactly the byte layout
// `writeCoffObject` produces, into the same format-neutral
// `ObjectFile` (see ../objectformat/ObjectFile.h) - the mirror image
// of the writer, not a general COFF reader (same "narrow, documented
// deviation" scope as the writer itself - see that file's header
// comment). `ObjRelocation::symbolName` here is `symbols[i].name` for
// whichever `symbols` entry the real `SymbolTableIndex` the file
// encodes names - resolved back to a name immediately on read, since
// a linker (unlike the writer) only ever wants to look relocations up
// by name, never by index.
//
// Throws std::runtime_error if `bytes` isn't a well-formed object
// file in this project's own documented layout (truncated, a
// relocation or symbol-table field pointing outside the buffer, etc).
ObjectFile readCoffObject(const std::string &bytes);

}  // namespace pcc
