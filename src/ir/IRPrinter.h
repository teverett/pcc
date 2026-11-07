#pragma once

#include <string>

#include "IR.h"

namespace pcc {

// A human-readable dump of the IR: slots, functions, and each body as
// indented pseudo-code with names shown as "sourceName#id" (id disambiguates
// shadowed/same-named slots). This text is also parsed by IRValidator
// against grammar/PccIRLexer.g4 + PccIRParser.g4, as a self-check that the
// grammar hasn't drifted from this printer.
//
// `sourceFile`, if non-empty, limits which top-level declarations are
// printed to that file's own; a cross-file reference still prints normally.
std::string printIR(const ir::Program &prog, const std::string &sourceFile = "");

}  // namespace pcc
