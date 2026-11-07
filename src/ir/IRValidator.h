#pragma once

#include "IR.h"

namespace pcc {

// Prints `prog` (see IRPrinter.h) and parses that text against the
// generated grammar/PccIRLexer.g4 + PccIRParser.g4 parser, on every compile.
// Throws std::runtime_error on a parse failure, which can only mean
// IRPrinter.cpp and the grammar have drifted apart — a pcc bug, not a
// problem with the user's source.
void validateIRSyntax(const ir::Program &prog);

}  // namespace pcc
