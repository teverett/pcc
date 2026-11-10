#pragma once

#include "IR.h"

namespace rcc {

// Prints `prog` (see IRPrinter.h) and parses that text against the
// generated grammar/RccIRLexer.g4 + RccIRParser.g4 parser, on every compile.
// Throws std::runtime_error on a parse failure, which can only mean
// IRPrinter.cpp and the grammar have drifted apart — a rcc bug, not a
// problem with the user's source.
void validateIRSyntax(const ir::Program &prog);

}  // namespace rcc
