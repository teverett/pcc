#pragma once

#include "IR.h"

namespace pcc {

// Prints `prog` (see IRPrinter.h - the same text --dump-ir writes)
// and parses that text with a parser generated from
// grammar/PccIRLexer.g4 + grammar/PccIRParser.g4: the IR's own
// grammar, describing exactly what IRPrinter.cpp emits. Called from
// main.cpp's compileOneFile(), right after IRBuilder and any
// optimizer pass (see optimizers/) have both run, on every compile -
// so every program pcc ever builds, in whatever shape the optimizers
// left it in, is checked against this grammar, not just a
// hand-picked few.
//
// Throws std::runtime_error if the printed IR fails to parse. That
// can only mean IRPrinter.cpp and grammar/PccIR*.g4 have drifted
// apart (one prints a shape the other doesn't accept) - a pcc bug,
// never something about the user's own source file.
void validateIRSyntax(const ir::Program &prog);

}  // namespace pcc
