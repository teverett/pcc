#pragma once

#include "AST.h"
#include "CParser.h"

namespace pcc {

// Walks an ANTLR C.g4 parse tree into the flat AST.h data model.
// Dispatches on the labeled-alternative Context subclasses with
// dynamic_cast rather than the generated Visitor interface, since
// the grammar's shape is fixed and known at every call site - a
// plain cast chain reads more directly than a double-dispatch visit.
Program buildProgram(CParser::ProgramContext *ctx);

}  // namespace pcc
